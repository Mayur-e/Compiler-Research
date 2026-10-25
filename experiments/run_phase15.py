#!/usr/bin/env python3
"""
run_phase15.py — Phase 15 Campaign Runner

Automates the full Phase 15 performance analysis campaign:
  1. Runs perf-analyzer for each benchmark (BM01-BM08) with --reps 10 --warmup 1
  2. Collects JSON output from each run
  3. Merges all results into visualization/performance_data.json
  4. Also regenerates static traces (A/B/C) for all benchmarks (for Phase 13 tab)
  5. Also re-runs correlation pipeline for safe benchmarks (for Phase 14 tab)

Safety classifications are preserved exactly as in run_correlation.py:
  BM06: UB (uninitialized pointer traversal) — excluded from concrete runtime
  BM08: Unsafe runtime — excluded from concrete runtime
  BM01-BM05, BM07: Valid for concrete execution

Usage:
    python experiments/run_phase15.py [--reps N] [--no-runtime] [--no-traces]
"""

import os
import sys
import subprocess
import json
import argparse

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BUILD_DIR    = os.path.join(PROJECT_ROOT, "build")
BENCH_DIR    = os.path.join(SCRIPT_DIR, "benchmarks")
VIS_DIR      = os.path.join(PROJECT_ROOT, "visualization")
SRC_DIR      = os.path.join(PROJECT_ROOT, "src")
INCLUDE_DIR  = os.path.join(PROJECT_ROOT, "include")

PERF_EXE  = os.path.join(BUILD_DIR, "src", "perf-analyzer.exe")
TRACE_EXE = os.path.join(BUILD_DIR, "src", "trace-driver.exe")
INSTR_EXE = os.path.join(BUILD_DIR, "src", "runtime-instrumentor.exe")
CORR_EXE  = os.path.join(BUILD_DIR, "src", "correlation-driver.exe")
RT_EVENTS_C = os.path.join(SRC_DIR, "runtime", "runtime_events.c")

# Ensure MSYS2 GCC is on PATH
os.environ["PATH"] = "D:\\msys64\\ucrt64\\bin;" + os.environ.get("PATH", "")

BENCHMARKS = [
    ("BM01", "bm01_simple_alloc.c",      True,  "Simple allocation (no loop)"),
    ("BM02", "bm02_linked_list_build.c", True,  "Linked list construction"),
    ("BM03", "bm03_dead_pointers.c",     True,  "Dead pointer pruning demo"),
    ("BM04", "bm04_aliasing.c",          True,  "Aliasing / pointer copies"),
    ("BM05", "bm05_branch_alloc.c",      True,  "Branch with allocation"),
    ("BM06", "bm06_loop_list.c",         False, "UB: uninitialized traversal"),
    ("BM07", "bm07_multi_field_graph.c", True,  "Multi-field graph (next+prev)"),
    ("BM08", "bm08_genuine_loop.c",      False, "Unsafe: uninitialized loop exit"),
]


def run_cmd(cmd, desc, timeout=30):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, timeout=timeout)
        if res.returncode not in (0, 2):   # 2 = BC mismatch warning (non-fatal)
            print(f"  [-] ERROR during {desc}: returncode={res.returncode}")
            if res.stderr.strip():
                print(f"      {res.stderr.strip()[:300]}")
            return False, res.stdout, res.stderr
        return True, res.stdout, res.stderr
    except subprocess.TimeoutExpired:
        print(f"  [-] TIMEOUT during {desc}")
        return False, "", "Timeout"


def load_json(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def check_prerequisites():
    missing = []
    for exe in [PERF_EXE, TRACE_EXE]:
        if not os.path.exists(exe):
            missing.append(exe)
    if missing:
        print("[-] Missing executables — build first:")
        for m in missing:
            print(f"    {m}")
        print("    Run: cmake --build build --target perf-analyzer trace-driver")
        sys.exit(1)


def run_static_traces(bm_id, src_path):
    """Run trace-driver for configs A, B, C. Returns dict of paths."""
    paths = {}
    for cfg in ("A", "B", "C"):
        out = os.path.join(VIS_DIR, f"{bm_id}_{cfg}_trace.json")
        ok, _, _ = run_cmd([TRACE_EXE, src_path, cfg, out],
                            f"static trace {cfg} for {bm_id}")
        if ok:
            paths[cfg] = out
    return paths


def run_perf_analyzer(bm_id, src_path, reps, warmup):
    """Run perf-analyzer and return parsed JSON."""
    out_json = os.path.join(BUILD_DIR, f"{bm_id}_perf.json")
    cmd = [PERF_EXE, src_path,
           "--reps",   str(reps),
           "--warmup", str(warmup),
           "--json",   out_json,
           "--explain"]
    ok, stdout, stderr = run_cmd(cmd, f"perf-analyzer for {bm_id}", timeout=120)
    if not ok and not os.path.exists(out_json):
        return None
    data = load_json(out_json)
    if data:
        print(f"  [+] perf-analyzer completed: BC={data.get('bc_invariant', '?')}")
    return data


def run_concrete_correlation(bm_id, src_path, trace_paths):
    """Instrument, compile, run, correlate. Returns corr_json path or None."""
    instr_c = os.path.join(BUILD_DIR, f"{bm_id}_instrumented.c")
    ok_i, _, _ = run_cmd([INSTR_EXE, src_path, "-o", instr_c, "--"],
                          f"instrumentation {bm_id}")
    if not ok_i:
        return None

    concrete_exe = os.path.join(BUILD_DIR, f"{bm_id}_concrete.exe")
    gcc_cmd = ["gcc", "-O0", "-g", f"-I{INCLUDE_DIR}",
               instr_c, RT_EVENTS_C, "-o", concrete_exe]
    ok_c, _, _ = run_cmd(gcc_cmd, f"compile {bm_id}")
    if not ok_c:
        return None

    rt_json = os.path.join(BUILD_DIR, f"{bm_id}_runtime_events.json")
    ok_r, _, _ = run_cmd([concrete_exe, rt_json], f"execute {bm_id}", timeout=5)
    if not ok_r:
        return None

    corr_json = os.path.join(VIS_DIR, f"{bm_id}_correlated_trace.json")
    corr_cmd = [CORR_EXE,
                "--runtime", rt_json,
                "--trace-a", trace_paths.get("A", ""),
                "--trace-b", trace_paths.get("B", ""),
                "--trace-c", trace_paths.get("C", ""),
                "--benchmark", bm_id,
                "--output", corr_json]
    ok_corr, _, _ = run_cmd(corr_cmd, f"correlate {bm_id}")
    if not ok_corr:
        return None
    return corr_json


def main():
    parser = argparse.ArgumentParser(description="Phase 15 Campaign Runner")
    parser.add_argument("--reps",        type=int, default=10)
    parser.add_argument("--warmup",      type=int, default=1)
    parser.add_argument("--no-runtime",  action="store_true", help="Skip concrete runtime")
    parser.add_argument("--no-traces",   action="store_true", help="Skip static trace generation")
    args = parser.parse_args()

    check_prerequisites()
    os.makedirs(VIS_DIR, exist_ok=True)

    print("=" * 70)
    print("  PHASE 15 — MDE-HRA PERFORMANCE ANALYSIS CAMPAIGN")
    print(f"  Reps: {args.reps}   Warmup: {args.warmup}")
    print("=" * 70)

    all_perf = {}     # bm_id -> perf JSON data
    all_status = {}   # bm_id -> status dict

    for bm_id, filename, safe, note in BENCHMARKS:
        src_path = os.path.join(BENCH_DIR, filename)
        if not os.path.exists(src_path):
            print(f"\n[!] Missing benchmark: {src_path} — skipping")
            continue

        print(f"\n[*] {bm_id} — {note}")
        if not safe:
            print(f"    Classification: EXCLUDED from concrete runtime ({note})")

        trace_paths = {}

        # 1. Static traces (for Phase 13 solver trace debugger tab)
        if not args.no_traces:
            print(f"    Generating static traces A/B/C...")
            trace_paths = run_static_traces(bm_id, src_path)
            missing = [c for c in ("A", "B", "C") if c not in trace_paths]
            if missing:
                print(f"    [!] Failed to generate traces for configs: {missing}")

        # 2. Performance analysis
        print(f"    Running perf-analyzer ({args.reps} reps, {args.warmup} warmup)...")
        perf_data = run_perf_analyzer(bm_id, src_path, args.reps, args.warmup)

        if perf_data:
            all_perf[bm_id] = perf_data
            bc_ok = perf_data.get("bc_invariant", False)
            print(f"    BC invariant: {'PASS' if bc_ok else 'FAIL'}")
        else:
            print(f"    [!] perf-analyzer produced no JSON for {bm_id}")

        # 3. Concrete runtime correlation (safe benchmarks only)
        corr_path = None
        if safe and not args.no_runtime and os.path.exists(INSTR_EXE):
            print(f"    Running concrete runtime correlation...")
            corr_path = run_concrete_correlation(bm_id, src_path, trace_paths)
            if corr_path:
                print(f"    [+] Correlated trace: {corr_path}")
            else:
                print(f"    [!] Concrete correlation failed — continuing")
        elif not safe:
            print(f"    Skipping concrete runtime (UB/unsafe)")

        all_status[bm_id] = {
            "safe": safe,
            "note": note,
            "traces_ok": bool(trace_paths),
            "perf_ok": perf_data is not None,
            "corr_ok": corr_path is not None,
            "bc_match": all_perf[bm_id].get("bc_invariant", False) if perf_data else None,
        }

    # ---------------------------------------------------------------
    # Build merged performance_data.json for the visualization
    # ---------------------------------------------------------------
    print("\n[*] Building performance_data.json for visualization...")

    merged = {
        "phase": 15,
        "description": "MDE-HRA Performance Analysis Campaign",
        "reps": args.reps,
        "warmup": args.warmup,
        "benchmarks": {},
    }

    for bm_id, filename, safe, note in BENCHMARKS:
        perf = all_perf.get(bm_id)
        status = all_status.get(bm_id, {})

        entry = {
            "id": bm_id,
            "filename": filename,
            "safe_for_runtime": safe,
            "classification": note,
            "perf_ok": perf is not None,
            "bc_match": perf.get("bc_invariant", False) if perf else None,
            "corr_trace": f"{bm_id}_correlated_trace.json" if status.get("corr_ok") else None,
            "configs": {},
        }

        if perf:
            for cfg_data in perf.get("summary", []):
                cfg = cfg_data.get("config", "?")
                entry["configs"][cfg] = cfg_data

        merged["benchmarks"][bm_id] = entry

    perf_json_path = os.path.join(VIS_DIR, "performance_data.json")
    with open(perf_json_path, "w") as f:
        json.dump(merged, f, indent=2)
    print(f"[+] Written: {perf_json_path}")

    # ---------------------------------------------------------------
    # Summary table
    # ---------------------------------------------------------------
    print("\n" + "=" * 80)
    print(f"{'BM':<6} | {'BC Match':<9} | {'Perf OK':<8} | {'Runtime':<9} | {'Classification'}")
    print("-" * 80)
    for bm_id, filename, safe, note in BENCHMARKS:
        s = all_status.get(bm_id, {})
        bc  = "PASS" if s.get("bc_match") else ("FAIL" if s.get("bc_match") is False else "N/A")
        pok = "YES"  if s.get("perf_ok") else "NO"
        rt  = "YES"  if s.get("corr_ok") else ("EXCL" if not safe else "SKIP")
        print(f"{bm_id:<6} | {bc:<9} | {pok:<8} | {rt:<9} | {note}")
    print("=" * 80)
    print(f"\n[+] Phase 15 campaign complete.")
    print(f"    Performance data: {perf_json_path}")
    print(f"    Open visualization/index.html in a browser to view results.")


if __name__ == "__main__":
    main()
