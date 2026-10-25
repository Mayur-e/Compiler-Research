# MDE-HRA Experimental Results & Evaluation Archive

This directory contains the complete, reproducible archive of experimental results, raw output logs, aggregated summaries, execution traces, and research evaluation reports for the **MDE-HRA** static analysis engine.

## Directory Organization

```
results/
├── raw/                     - Detailed machine-readable outputs per test run
│   ├── benchmarks/          - 4-configuration JSON outputs for BM01–BM08 (8 files)
│   ├── synthetic/           - 4-configuration JSON outputs for Q1 synthetic suite (4 files)
│   ├── scaling/             - 80 runs across N in {8, 16, 32, 64} with raw_runs.csv & stdout/stderr
│   └── gcc14/               - Intake manifest (gcc_c_manifest.tsv) and compiler diagnostics for 50 GCC tests
├── summary/                 - Consolidated Markdown and CSV tables for publication
│   ├── benchmarks_summary.md/.csv
│   ├── synthetic_summary.md/.csv
│   ├── scaling_summary.md/.csv
│   └── gcc_evaluation_summary.md/.json
├── traces/                  - Runtime correlation traces & static CFG solver traces (33 files)
├── reports/                 - Formal audit and final upgrade reports (4 files)
├── MANIFEST.tsv             - Complete file manifest with byte sizes and descriptions
└── README.md                - This directory guide
```

## Key Evaluation Findings

### 1. Dual Invariant Verification (100% Across All Suites)

- **Invariant 1 ($B \equiv C$)**: Live-at-exit canonical graphs between Liveness+HRA (B) and Liveness+HRA+MDE (C) match identically across all 66 evaluated benchmarks (**100% PASS**).
- **Invariant 2 ($A \equiv D$)**: Baseline HRA (A) and HRA+MDE (D) match identically across all program variables across all evaluated benchmarks (**100% PASS**).
- **Violations**: **0 (0.0%)**.

### 2. MDE Deduplication & Memory Footprint

- **Hit Ratios**: MDE state deduplication achieved hit ratios up to **91.1%** in `dead_temporaries` and **89.9%** in `loop_chain`.
- **Retained State Reduction**: Combining liveness pruning with MDE reduced retained canonical heap memory by **62.5%** in `loop_chain` (11,476 B in Config D down to 4,300 B in Config C).

### 3. Summary Tables Reference

| Suite | Summary Markdown | CSV Table | Status |
| :--- | :--- | :--- | :---: |
| **Standard Benchmarks** | [benchmarks_summary.md](file:///d:/TS/Seminar/Project/results/summary/benchmarks_summary.md) | [benchmarks_summary.csv](file:///d:/TS/Seminar/Project/results/summary/benchmarks_summary.csv) | Verified |
| **Synthetic Benchmarks** | [synthetic_summary.md](file:///d:/TS/Seminar/Project/results/summary/synthetic_summary.md) | [synthetic_summary.csv](file:///d:/TS/Seminar/Project/results/summary/synthetic_summary.csv) | Verified |
| **Scaling Experiment** | [scaling_summary.md](file:///d:/TS/Seminar/Project/results/summary/scaling_summary.md) | [scaling_summary.csv](file:///d:/TS/Seminar/Project/results/summary/scaling_summary.csv) | Verified |
| **GCC Testsuite (50 Tests)** | [gcc_evaluation_summary.md](file:///d:/TS/Seminar/Project/results/summary/gcc_evaluation_summary.md) | [gcc_evaluation_summary.json](file:///d:/TS/Seminar/Project/results/summary/gcc_evaluation_summary.json) | Verified |

### 4. Primary Research Reports

- [mde_hra_q1_upgrade_report.md](file:///d:/TS/Seminar/Project/results/reports/mde_hra_q1_upgrade_report.md): Master research upgrade and empirical evaluation report.
- [implementation_audit.md](file:///d:/TS/Seminar/Project/results/reports/implementation_audit.md): Systematic technical review addressing all critical defects.
- [benchmark_protocol.md](file:///d:/TS/Seminar/Project/results/reports/benchmark_protocol.md): Formal experimental setup and invariant definitions.
- [test_coverage.md](file:///d:/TS/Seminar/Project/results/reports/test_coverage.md): Comprehensive inventory of tests, commands, and results.
