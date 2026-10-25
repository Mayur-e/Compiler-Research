# MDE-HRA Evaluated Benchmark Corpus

This directory contains a complete, self-contained copy of all C benchmark programs evaluated in the MDE-HRA static analysis project.

**Total Programs**: 66 C files

## Directory Structure

```
benchmarks/
├── standard/      (8 files)  - Project standard baseline benchmarks (BM01 - BM08)
├── synthetic/     (4 files)  - Evaluation pack benchmarks (alias_fields, dead_temporaries, loop_chain, low_reuse)
├── scaling/       (4 files)  - Controlled size pointer chains (N = 8, 16, 32, 64)
├── gcc_torture/   (50 files) - Flat copy of ingested GCC c-torture execution tests
└── gcc/           - Original GCC testsuite hierarchy for intake scripts
```

## 1. Project Standard Benchmarks (`benchmarks/standard/`)

| File | Description | Lines | Mode | Status |
| :--- | :--- | :---: | :---: | :---: |
| [bm01_simple_alloc.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm01_simple_alloc.c) | Classical pointer pattern test | 19 | STRICT | PASS (B==C, A==D) |
| [bm02_linked_list_build.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm02_linked_list_build.c) | Classical pointer pattern test | 24 | STRICT | PASS (B==C, A==D) |
| [bm03_dead_pointers.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm03_dead_pointers.c) | Classical pointer pattern test | 26 | STRICT | PASS (B==C, A==D) |
| [bm04_aliasing.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm04_aliasing.c) | Classical pointer pattern test | 30 | STRICT | PASS (B==C, A==D) |
| [bm05_branch_alloc.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm05_branch_alloc.c) | Classical pointer pattern test | 36 | STRICT | PASS (B==C, A==D) |
| [bm06_loop_list.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm06_loop_list.c) | Classical pointer pattern test | 29 | STRICT | PASS (B==C, A==D) |
| [bm07_multi_field_graph.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm07_multi_field_graph.c) | Classical pointer pattern test | 44 | STRICT | PASS (B==C, A==D) |
| [bm08_genuine_loop.c](file:///d:/TS/Seminar/Project/benchmarks/standard/bm08_genuine_loop.c) | Classical pointer pattern test | 53 | STRICT | PASS (B==C, A==D) |

## 2. Evaluation Pack Synthetic Benchmarks (`benchmarks/synthetic/`)

| File | Focus Area | Lines | Mode | Status |
| :--- | :--- | :---: | :---: | :---: |
| [alias_fields.c](file:///d:/TS/Seminar/Project/benchmarks/synthetic/alias_fields.c) | Structural deduplication & liveness | 12 | STRICT | PASS (B==C, A==D) |
| [dead_temporaries.c](file:///d:/TS/Seminar/Project/benchmarks/synthetic/dead_temporaries.c) | Structural deduplication & liveness | 16 | STRICT | PASS (B==C, A==D) |
| [loop_chain.c](file:///d:/TS/Seminar/Project/benchmarks/synthetic/loop_chain.c) | Structural deduplication & liveness | 14 | STRICT | PASS (B==C, A==D) |
| [low_reuse.c](file:///d:/TS/Seminar/Project/benchmarks/synthetic/low_reuse.c) | Structural deduplication & liveness | 12 | STRICT | PASS (B==C, A==D) |

## 3. Controlled Chain Scaling Benchmarks (`benchmarks/scaling/`)

| File | Chain Length ($N$) | Lines | Mode | Status |
| :--- | :---: | :---: | :---: | :---: |
| [chain_16.c](file:///d:/TS/Seminar/Project/benchmarks/scaling/chain_16.c) | 16 Nodes | 14 | STRICT | PASS (B==C, A==D) |
| [chain_32.c](file:///d:/TS/Seminar/Project/benchmarks/scaling/chain_32.c) | 32 Nodes | 14 | STRICT | PASS (B==C, A==D) |
| [chain_64.c](file:///d:/TS/Seminar/Project/benchmarks/scaling/chain_64.c) | 64 Nodes | 14 | STRICT | PASS (B==C, A==D) |
| [chain_8.c](file:///d:/TS/Seminar/Project/benchmarks/scaling/chain_8.c) | 8 Nodes | 14 | STRICT | PASS (B==C, A==D) |

## 4. Ingested GCC C-Torture Benchmarks (`benchmarks/gcc_torture/`)

Contains 50 real C torture tests from the official GCC repository (`gcc.c-torture/execute`).
Evaluated with `mdehra-driver`: sound subset gating verified under `--strict`, and 100% dual invariant compliance ($B==C$ and $A==D$) verified under `--permissive`.

| Sample File | Ingestion Status | Strict Mode | Permissive Mode | Invariants ($B==C, A==D$) |
| :--- | :---: | :---: | :---: | :---: |
| [20000112-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000112-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000113-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000113-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000121-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000121-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000205-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000205-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000217-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000217-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000223-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000223-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000224-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000224-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000225-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000225-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000227-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000227-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| [20000313-1.c](file:///d:/TS/Seminar/Project/benchmarks/gcc_torture/20000313-1.c) | COMPILE_OK | REJECTED | PASS | VERIFIED |
| *... and 40 more files* | ... | ... | ... | ... |

Full details available in [MANIFEST.tsv](file:///d:/TS/Seminar/Project/benchmarks/MANIFEST.tsv).
