# GCC Testsuite Intake and Evaluation Summary

- **Total Ingested Tests**: 50
- **Syntax Compilation (`gcc -fsyntax-only`)**: 48 OK, 2 Target-Specific / Fail
- **Strict Mode Sound Subset**: 1 accepted, 49 soundly rejected with diagnostic codes
- **Permissive Mode Execution**: 50 translated and solved, 0 failed
- **Dual Invariants Preserved**: **50 / 50 (100%)** ($Canonical(B) == Canonical(C)$ and $Canonical(A) == Canonical(D)$)
- **Invariant Violations**: **0 (0%)**

## Corpus Subset Breakdown

| Test Name | Compile Status | Strict Mode | Permissive Mode | Invariants ($B==C$, $A==D$) | Diagnostics |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `20000112-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000113-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000121-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000205-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000217-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000223-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000224-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000225-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000227-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000313-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000314-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000314-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000314-3.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000402-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000403-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-3.c` | COMPILE_FAIL_OR_TARGET_SPECIFIC | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-4.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-5.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000412-6.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000419-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000422-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000503-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000511-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000519-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000519-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000523-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000528-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000603-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000605-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000605-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000605-3.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000622-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000703-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000706-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000706-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000706-3.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000706-4.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000706-5.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000707-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000715-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000715-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000717-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000717-2.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000717-3.c` | COMPILE_FAIL_OR_TARGET_SPECIFIC | REJECTED | PASS | **VERIFIED** | 0 |
| `20000717-4.c` | COMPILE_OK | PASS | PASS | **VERIFIED** | 0 |
| `20000717-5.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000722-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
| `20000726-1.c` | COMPILE_OK | REJECTED | PASS | **VERIFIED** | 0 |
