# Evaluation Pack Synthetic Benchmarks Summary

Evaluated with `mdehra-driver` in strict mode. All benchmarks verified invariants: B==C and A==D.

### Table 1: Configuration Invariants and Solver Timing

| Benchmark | SHA-256 | Blocks | Status | B==C | A==D | Time A (ms) | Time B (ms) | Time C (ms) | Time D (ms) | Peak RSS (KB) |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| alias_fields | `ae25cde1da73` | 9 | OK | PASS | PASS | 0.2021 | 0.1139 | 0.3772 | 0.3244 | 24860 |
| dead_temporaries | `46265be5fe6f` | 14 | OK | PASS | PASS | 0.3904 | 0.2931 | 0.6717 | 0.5103 | 24864 |
| loop_chain | `434513facfbd` | 19 | OK | PASS | PASS | 1.3327 | 1.1289 | 1.4667 | 2.1988 | 24896 |
| low_reuse | `afaf8a4ec1e6` | 11 | OK | PASS | PASS | 0.2629 | 0.2063 | 0.4883 | 0.3931 | 24864 |

### Table 2: Pruning and MDE Structural Deduplication Efficiency

| Benchmark | Pruned (B) | Pruned (C) | MDE Hits (C) | MDE Misses (C) | Hit Ratio (C) | MDE Hits (D) | Retained Bytes (C) | Retained Bytes (D) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| alias_fields | 2 | 2 | 14 | 4 | 77.8% | 15 | 912 B | 1260 B |
| dead_temporaries | 0 | 0 | 82 | 8 | 91.1% | 86 | 1692 B | 992 B |
| loop_chain | 0 | 0 | 133 | 15 | 89.9% | 150 | 4300 B | 11476 B |
| low_reuse | 0 | 0 | 18 | 4 | 81.8% | 19 | 1136 B | 1376 B |
