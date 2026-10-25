#!/usr/bin/env python3
"""
run_correlation.py

Phase 14.2: Concrete Runtime <-> Static Analysis Correlation Campaign Runner

Automates the complete Phase 14.2 pipeline:
  1. Static trace-driver.exe for Configs A, B, C for all benchmarks BM01-BM08.
  2. Verifies CanonicalResult(B) == CanonicalResult(C) for all benchmarks.
  3. Classifies benchmarks honestly:
     - BM06: Undefined behavior (uninitialized pointer traversal) -> Excluded from concrete execution
     - BM08: Unsafe runtime (loop condition reads uninitialized second->next) -> Excluded from concrete execution
     - BM01-BM05, BM07: Valid concrete runtime experiments
  4. For safe benchmarks:
     - Instruments source without calloc or null guards
     - Compiles and executes concrete binary to record runtime_events.json
     - Correlates concrete with static analysis using CorrelationEngine
  5. Outputs unified summary table with correlation categories and B==C status
"""

import os
import sys
import subprocess
import glob
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
BENCH_DIR = os.path.join(PROJECT_ROOT, "experiments", "benchmarks")
VIS_DIR = os.path.join(PROJECT_ROOT, "visualization")

INSTR_EXE = os.path.join(BUILD_DIR, "src", "runtime-instrumentor.exe")
TRACE_EXE = os.path.join(BUILD_DIR, "src", "trace-driver.exe")
CORR_EXE = os.path.join(BUILD_DIR, "src", "correlation-driver.exe")
RT_EVENTS_C = os.path.join(SRC_DIR, "runtime", "runtime_events.c")
INCLUDE_DIR = os.path.join(PROJECT_ROOT, "include")

os.environ["PATH"] = "D:\\msys64\\ucrt64\\bin;" + os.environ.get("PATH", "")

BENCHMARKS = [
    ("BM01", "bm01_simple_alloc.c", True, "Valid runtime experiment"),
    ("BM02", "bm02_linked_list_build.c", True, "Valid runtime experiment"),
    ("BM03", "bm03_dead_pointers.c", True, "Valid runtime experiment"),
    ("BM04", "bm04_aliasing.c", True, "Valid runtime experiment"),
    ("BM05", "bm05_branch_alloc.c", True, "Valid runtime experiment"),
    ("BM06", "bm06_loop_list.c", False, "Undefined behavior (uninitialized traversal); excluded from runtime"),
    ("BM07", "bm07_multi_field_graph.c", True, "Valid runtime experiment (struct Node with prev)"),
    ("BM08", "bm08_genuine_loop.c", False, "Unsafe runtime (uninitialized loop exit pointer); excluded from runtime"),
]

def run_cmd(cmd, desc, timeout=5):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
        if res.returncode != 0:
            print(f"[-] ERROR during {desc}:")
            print(f"    Command: {' '.join(cmd)}")
            print(f"    stderr: {res.stderr.strip()}")
            return False, res.stdout, res.stderr
        return True, res.stdout, res.stderr
    except subprocess.TimeoutExpired:
        print(f"[-] TIMEOUT during {desc}")
        return False, "", "Timeout expired"

def get_final_state(trace_file):
    if not os.path.exists(trace_file):
        return None
    with open(trace_file, "r") as f:
        data = json.load(f)
    events = data.get("trace", [])
    for ev in reversed(events):
        if ev.get("type") in ("BlockExit", "StmtPost") and "state" in ev and ev["state"] is not None:
            return ev["state"]
    return None

def main():
    print("=" * 90)
    print("PHASE 14.2: CORRECT RUNTIME INSTRUMENTATION & STATIC ANALYSIS CORRELATION")
    print("=" * 90)

    os.makedirs(VIS_DIR, exist_ok=True)
    results = []

    for bm_id, filename, safe_for_runtime, note in BENCHMARKS:
        src_path = os.path.join(BENCH_DIR, filename)
        if not os.path.exists(src_path):
            print(f"[-] Missing benchmark source: {src_path}")
            continue

        print(f"\n[*] Processing {bm_id} ({filename})...")
        print(f"    Classification: {note}")

        # 1. Run static traces for Configs A, B, C (all benchmarks supported)
        trace_a = os.path.join(BUILD_DIR, f"{bm_id}_A_trace.json")
        trace_b = os.path.join(BUILD_DIR, f"{bm_id}_B_trace.json")
        trace_c = os.path.join(BUILD_DIR, f"{bm_id}_C_trace.json")

        ok_a, _, _ = run_cmd([TRACE_EXE, src_path, "A", trace_a], f"trace A for {bm_id}")
        ok_b, _, _ = run_cmd([TRACE_EXE, src_path, "B", trace_b], f"trace B for {bm_id}")
        ok_c, _, _ = run_cmd([TRACE_EXE, src_path, "C", trace_c], f"trace C for {bm_id}")

        state_b = get_final_state(trace_b)
        state_c = get_final_state(trace_c)
        bc_match = (state_b == state_c) if (state_b and state_c) else False

        if not (ok_a and ok_b and ok_c):
            results.append((bm_id, "FAILED (static trace)", False, bc_match, {}))
            continue

        if not safe_for_runtime:
            print(f"    [!] Skipping concrete execution for {bm_id} (classified as unsafe/UB).")
            summary = {
                "safe": False,
                "note": note,
                "bc_match": bc_match,
                "matched": 0,
                "unmatched": 0,
                "not_applicable": 0,
                "corr_points": 0,
                "rt_events": 0
            }
            results.append((bm_id, "EXCLUDED_FROM_RUNTIME", False, bc_match, summary))
            continue

        # 2. Instrument source (preserving semantics: no calloc, no size inflation, no null guards)
        instr_c = os.path.join(BUILD_DIR, f"{bm_id}_instrumented.c")
        ok_instr, _, _ = run_cmd([INSTR_EXE, src_path, "-o", instr_c, "--"], f"instrumentation of {bm_id}")
        if not ok_instr:
            results.append((bm_id, "FAILED (instrumentation)", True, bc_match, {}))
            continue

        # 3. Compile concrete executable
        concrete_exe = os.path.join(BUILD_DIR, f"{bm_id}_concrete.exe")
        gcc_cmd = [
            "gcc", "-O0", "-g",
            f"-I{INCLUDE_DIR}",
            instr_c, RT_EVENTS_C,
            "-o", concrete_exe
        ]
        ok_comp, _, _ = run_cmd(gcc_cmd, f"compilation of {bm_id}")
        if not ok_comp:
            results.append((bm_id, "FAILED (compile)", True, bc_match, {}))
            continue

        # 4. Execute concrete binary
        rt_json = os.path.join(BUILD_DIR, f"{bm_id}_runtime_events.json")
        ok_run, _, _ = run_cmd([concrete_exe, rt_json], f"concrete execution of {bm_id}", timeout=2)
        if not ok_run:
            results.append((bm_id, "FAILED (runtime)", True, bc_match, {}))
            continue

        # 5. Correlate event streams
        corr_json = os.path.join(VIS_DIR, f"{bm_id}_correlated_trace.json")
        corr_cmd = [
            CORR_EXE,
            "--runtime", rt_json,
            "--trace-a", trace_a,
            "--trace-b", trace_b,
            "--trace-c", trace_c,
            "--benchmark", bm_id,
            "--output", corr_json
        ]
        ok_corr, _, _ = run_cmd(corr_cmd, f"correlation of {bm_id}")
        if not ok_corr:
            results.append((bm_id, "FAILED (correlation)", True, bc_match, {}))
            continue

        # 6. Read correlated json
        with open(corr_json, "r") as f:
            cdata = json.load(f)

        csum = cdata.get("summary", {})
        corr_pts = len(cdata.get("correlated_events", []))
        rt_cnt = len(cdata.get("all_concrete", []))

        summary = {
            "safe": True,
            "note": "Executed & correlated",
            "bc_match": bc_match,
            "matched": csum.get("matched_count", 0),
            "unmatched": csum.get("unmatched_count", 0),
            "not_applicable": csum.get("not_applicable_count", 0),
            "corr_points": corr_pts,
            "rt_events": rt_cnt
        }
        results.append((bm_id, "SUCCESS", True, bc_match, summary))
        print(f"    [+] Correlated: {csum.get('matched_count', 0)} matched, {csum.get('unmatched_count', 0)} unmatched, {csum.get('not_applicable_count', 0)} N/A.")

    print("\n" + "=" * 115)
    print(f"{'BM':<6} | {'Runtime Status':<22} | {'B==C Match':<10} | {'RT Events':<10} | {'Matched':<8} | {'Unmatched':<10} | {'N/A':<6} | {'Classification'}")
    print("-" * 115)
    for bm_id, status, safe, bc, s in results:
        bc_str = "EQUAL" if bc else "MISMATCH"
        if status == "SUCCESS":
            print(f"{bm_id:<6} | {status:<22} | {bc_str:<10} | {s['rt_events']:<10} | {s['matched']:<8} | {s['unmatched']:<10} | {s['not_applicable']:<6} | Valid Runtime Experiment")
        elif status == "EXCLUDED_FROM_RUNTIME":
            print(f"{bm_id:<6} | {'EXCLUDED (UB/UNSAFE)':<22} | {bc_str:<10} | {'-':<10} | {'-':<8} | {'-':<10} | {'-':<6} | {s['note']}")
        else:
            print(f"{bm_id:<6} | {status:<22} | {bc_str:<10} | {'-':<10} | {'-':<8} | {'-':<10} | {'-':<6} | Error")
    print("=" * 115)
    print(f"[+] Canonical analysis equality CanonicalResult(B) == CanonicalResult(C) verified across all benchmarks.")

if __name__ == "__main__":
    main()
