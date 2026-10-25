# Benchmark protocol for a journal submission

## Benchmark layers

1. **Unit and generated tests:** thousands of small, deterministic pointer programs, with a concrete/hand-derived expected abstract result where feasible. Include positive and negative discrimination tests for fields and allocation sites.
2. **Controlled scaling:** generated chain/tree/list/branch/loop programs with size parameter N and independently controlled repeated-state density. Include high-reuse and low-reuse negative controls.
3. **Established suites:** GCC testsuite C tests, SV-COMP ReachSafety/MemorySafety C benchmarks, Juliet C/C++ weakness tests, and selected real C projects. These are candidate sources, not a license to feed every test into this restricted analyzer. Pin suite revisions and licenses; filter by semantic support and retain a manifest of included/excluded files with reasons.
4. **Cross-system baselines:** LLVM AA query evaluation (`opt`/`aa-eval`, version-pinned), plus the project's own A/B/C/D and cheap interning baselines. LLVM AA answers are not directly interchangeable with the project's HRA result; compare only a defined common query set and report the semantic mismatch clearly.

## Per-program metadata

Record SHA-256, source origin/revision/license, C/C++ standard, compile flags, lines of code, AST/IR instruction count, CFG blocks/edges, pointer values, allocation sites, fields, calls, supported-feature coverage, analysis status, timeout, and peak memory.

## Timing

- Release build, fixed compiler and optimization flags.
- Separate cold process runs for end-to-end metrics; warm microbenchmarks for component-level metrics.
- Randomize config order per repetition or use one process per config.
- Report startup/frontend time, liveness time, solver time, canonicalization time, result normalization time, total wall time, and query latency separately.
- Use a timeout policy fixed before the run; keep failures and timeouts in the table.
- Collect at least 30 independent runs for longer programs. For sub-millisecond operations, batch many analyses in one timed loop and divide by the number of completed operations; verify result checksums to prevent dead-code elimination.

## Memory

Report (a) process peak RSS as supplementary system memory, (b) retained bytes for HRA states, (c) retained bytes by MDE level, (d) temporary allocation volume/count, (e) unique canonical states and unique substructures. Validate internal counters with a heap profiler on a representative subset.

## Correctness

Every candidate run must emit `OK`, `UNSUPPORTED`, or `ERROR`. For all `OK` cases, require B=C exact normalized-result equivalence; also test the claimed pruning preservation obligation. If a conservative fallback changes a query from `NoAlias` to `MayAlias`, classify it as soundness-preserving but not exact-answer-equivalent. Never label unsupported code as a successful empty analysis.

## Statistical reporting

Report per-program medians and IQR, paired speedup ratios, bootstrap 95% confidence intervals, and an aggregate geometric mean with its interval. Use paired comparisons on the same benchmark and report practical effect sizes. Include all raw observations and plotting scripts. Do not select only programs that improve.

## GCC testsuite and other corpus intake

GCC's testsuite is huge and includes target-specific tests, expected compile failures, diagnostics tests, runtime tests, and non-C programs. It is not a single ready-made pointer-analysis benchmark. Use it as a **source corpus**, pin a GCC release/commit, filter `.c` files, compile with an explicitly chosen target/standard, and preserve DejaGNU metadata where relevant. For this project, a candidate file must first pass the supported-subset checker. SV-COMP and Juliet likewise require category selection, license/revision recording, and exclusion reasons.
