# Project Standard Benchmarks (BM01-BM08) Summary

Evaluated with `mdehra-driver` in strict mode. All benchmarks verified invariants: B==C and A==D.

### Table 1: Configuration Invariants and Solver Timing

| Benchmark | SHA-256 | Blocks | Status | B==C | A==D | Time A (ms) | Time B (ms) | Time C (ms) | Time D (ms) | Peak RSS (KB) |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| bm01 | `b305c4ff76c3` | 3 | OK | PASS | PASS | 0.0545 | 0.0317 | 0.2328 | 0.1438 | 22808 |
| bm02 | `e3ae910b4047` | 3 | OK | PASS | PASS | 0.0826 | 0.0529 | 0.3357 | 0.244 | 22820 |
| bm03 | `a72b4cfbaef3` | 3 | OK | PASS | PASS | 0.0549 | 0.0225 | 0.2196 | 0.152 | 22768 |
| bm04 | `ee1a05c3a3ed` | 3 | OK | PASS | PASS | 0.0725 | 0.0438 | 0.2719 | 0.1872 | 22852 |
| bm05 | `34511e0bc349` | 6 | OK | PASS | PASS | 0.104 | 0.0724 | 0.3935 | 0.3719 | 22972 |
| bm06 | `04d65a3971d9` | 3 | OK | PASS | PASS | 0.0586 | 0.0497 | 0.2619 | 0.1539 | 22824 |
| bm07 | `f98dcb7058b9` | 3 | OK | PASS | PASS | 0.1057 | 0.0767 | 0.42 | 0.3489 | 22852 |
| bm08 | `fa7cde00d79b` | 7 | OK | PASS | PASS | 0.2167 | 0.1369 | 0.4293 | 0.587 | 22964 |

### Table 2: Pruning and MDE Structural Deduplication Efficiency

| Benchmark | Pruned (B) | Pruned (C) | MDE Hits (C) | MDE Misses (C) | Hit Ratio (C) | MDE Hits (D) | Retained Bytes (C) | Retained Bytes (D) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| bm01 | 0 | 0 | 4 | 2 | 66.7% | 4 | 452 B | 552 B |
| bm02 | 0 | 0 | 4 | 2 | 66.7% | 4 | 512 B | 712 B |
| bm03 | 2 | 2 | 4 | 2 | 66.7% | 4 | 392 B | 600 B |
| bm04 | 0 | 0 | 4 | 2 | 66.7% | 4 | 452 B | 852 B |
| bm05 | 0 | 0 | 8 | 4 | 66.7% | 8 | 1136 B | 1744 B |
| bm06 | 0 | 0 | 4 | 2 | 66.7% | 4 | 388 B | 776 B |
| bm07 | 1 | 1 | 4 | 2 | 66.7% | 4 | 624 B | 1128 B |
| bm08 | 0 | 0 | 18 | 4 | 81.8% | 18 | 1072 B | 1872 B |
