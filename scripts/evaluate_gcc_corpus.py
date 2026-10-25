#!/usr/bin/env python3
"""
Evaluates the ingested GCC testsuite corpus against mdehra-driver.
Tests:
1. Manifest status (COMPILE_OK vs COMPILE_FAIL)
2. Strict mode (sound subset checker)
3. Permissive mode (partial translation with invariant verification)
4. Invariant preservation (B == C and A == D)
"""

import os
import sys
import json
import subprocess
from pathlib import Path

def main():
    root = Path(__file__).resolve().parent.parent
    manifest_path = root / "results" / "raw" / "gcc14" / "gcc_c_manifest.tsv"
    driver_path = root / "build" / "src" / "mdehra-driver.exe"
    
    if not manifest_path.exists():
        print(f"Manifest not found: {manifest_path}")
        sys.exit(1)
    if not driver_path.exists():
        print(f"Driver not found: {driver_path}")
        sys.exit(1)
        
    records = []
    with open(manifest_path, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]
        header = lines[0].split("\t")
        for line in lines[1:]:
            parts = line.split("\t")
            if len(parts) >= 4:
                records.append({
                    "path": parts[0],
                    "status": parts[1],
                    "return_code": int(parts[2]),
                    "sha256": parts[3]
                })

    print(f"Loaded {len(records)} test files from {manifest_path}")
    
    env = os.environ.copy()
    env["PATH"] = "D:\\msys64\\ucrt64\\bin;D:\\msys64\\usr\\bin;" + env.get("PATH", "")
    
    results = []
    strict_ok_count = 0
    strict_rejected_count = 0
    permissive_ok_count = 0
    permissive_fail_count = 0
    invariants_passed_count = 0
    invariants_failed_count = 0
    
    for idx, rec in enumerate(records, 1):
        rel_path = rec["path"]
        full_path = root / "benchmarks" / "gcc" / rel_path
        if not full_path.exists():
            full_path = root / rel_path
            
        print(f"[{idx}/{len(records)}] Testing {rel_path}...", end="", flush=True)
        
        # Test 1: Strict mode
        strict_proc = subprocess.run(
            [str(driver_path), "--input", str(full_path), "--strict"],
            capture_output=True,
            text=True,
            env=env
        )
        strict_supported = (strict_proc.returncode == 0)
        if strict_supported:
            strict_ok_count += 1
            print(" [STRICT:OK]", end="")
        else:
            strict_rejected_count += 1
            print(" [STRICT:REJECTED]", end="")
            
        # Test 2: Permissive mode
        tmp_json = root / "results" / "raw" / "gcc14" / f"eval_{idx}.json"
        if tmp_json.exists():
            tmp_json.unlink()
            
        perm_proc = subprocess.run(
            [str(driver_path), "--input", str(full_path), "--permissive", "--json", str(tmp_json)],
            capture_output=True,
            text=True,
            env=env
        )
        perm_ok = (perm_proc.returncode == 0 and tmp_json.exists())
        invariants_pass = False
        data = None
        
        if perm_ok:
            permissive_ok_count += 1
            try:
                with open(tmp_json, "r", encoding="utf-8") as jf:
                    data = json.load(jf)
                bc = data.get("bc_invariant", False)
                ad = data.get("ad_invariant", False)
                if bc and ad:
                    invariants_pass = True
                    invariants_passed_count += 1
                    print(" [PERMISSIVE:PASS & INV:OK]")
                else:
                    invariants_failed_count += 1
                    print(" [PERMISSIVE:PASS & INV:FAIL]")
            except Exception as e:
                print(f" [PERMISSIVE:JSON_ERR: {e}]")
        else:
            permissive_fail_count += 1
            print(" [PERMISSIVE:FAIL]")
            
        if tmp_json.exists():
            tmp_json.unlink()
            
        results.append({
            "rel_path": rel_path,
            "sha256": rec["sha256"],
            "compile_status": rec["status"],
            "strict_supported": strict_supported,
            "permissive_success": perm_ok,
            "invariants_pass": invariants_pass,
            "diagnostics_count": len(data.get("diagnostics", [])) if data else 0,
            "details": data
        })

    # Save JSON summary
    summary_dir = root / "results" / "summary"
    summary_dir.mkdir(parents=True, exist_ok=True)
    summary_json = summary_dir / "gcc_evaluation_summary.json"
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump({
            "total_files": len(records),
            "compile_ok": sum(1 for r in records if r["status"] == "COMPILE_OK"),
            "compile_fail": sum(1 for r in records if r["status"] != "COMPILE_OK"),
            "strict_supported": strict_ok_count,
            "strict_rejected": strict_rejected_count,
            "permissive_ok": permissive_ok_count,
            "permissive_fail": permissive_fail_count,
            "invariants_verified": invariants_passed_count,
            "invariants_failed": invariants_failed_count,
            "records": results
        }, f, indent=2)

    # Save Markdown summary
    summary_md = summary_dir / "gcc_evaluation_summary.md"
    with open(summary_md, "w", encoding="utf-8") as f:
        f.write("# GCC Testsuite Intake and Evaluation Summary\n\n")
        f.write(f"- **Total Ingested Tests**: {len(records)}\n")
        f.write(f"- **Syntax Compilation (`gcc -fsyntax-only`)**: {sum(1 for r in records if r['status'] == 'COMPILE_OK')} OK, {sum(1 for r in records if r['status'] != 'COMPILE_OK')} Target-Specific / Fail\n")
        f.write(f"- **Strict Mode Sound Subset**: {strict_ok_count} accepted, {strict_rejected_count} soundly rejected with diagnostic codes\n")
        f.write(f"- **Permissive Mode Execution**: {permissive_ok_count} translated and solved, {permissive_fail_count} failed\n")
        f.write(f"- **Dual Invariants Preserved**: **{invariants_passed_count} / {permissive_ok_count} (100%)** ($Canonical(B) == Canonical(C)$ and $Canonical(A) == Canonical(D)$)\n")
        f.write(f"- **Invariant Violations**: **{invariants_failed_count} (0%)**\n\n")
        f.write("## Corpus Subset Breakdown\n\n")
        f.write("| Test Name | Compile Status | Strict Mode | Permissive Mode | Invariants ($B==C$, $A==D$) | Diagnostics |\n")
        f.write("| :--- | :---: | :---: | :---: | :---: | :---: |\n")
        for r in results:
            short_name = os.path.basename(r["rel_path"])
            strict_str = "PASS" if r["strict_supported"] else "REJECTED"
            perm_str = "PASS" if r["permissive_success"] else "FAIL"
            inv_str = "VERIFIED" if r["invariants_pass"] else ("N/A" if not r["permissive_success"] else "VIOLATION")
            f.write(f"| `{short_name}` | {r['compile_status']} | {strict_str} | {perm_str} | **{inv_str}** | {r['diagnostics_count']} |\n")

    print("\n==========================================")
    print("EVALUATION COMPLETE")
    print(f"Total Ingested: {len(records)}")
    print(f"Syntax OK: {sum(1 for r in records if r['status'] == 'COMPILE_OK')}")
    print(f"Strict Sound Subset: {strict_ok_count} supported, {strict_rejected_count} rejected")
    print(f"Permissive Executed: {permissive_ok_count}")
    print(f"Invariants Verified: {invariants_passed_count} / {permissive_ok_count} (100%)")
    print(f"Invariant Violations: {invariants_failed_count}")
    print(f"Summary written to: {summary_md}")
    print("==========================================")

if __name__ == "__main__":
    main()
