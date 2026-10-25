# Critical implementation audit: MDE-HRA

## Verdict

**The current code is a research prototype for a restricted allocation-site graph analysis, not yet a general C/C++ pointer analyzer and not yet a Q1-ready implementation.** The manuscript itself correctly admits the most important negative result: on eight tiny synthetic functions, MDE state canonicalization is 5.1–9.0× slower than liveness-only analysis; process-wide peak memory cannot distinguish configurations; only 3/50 transfers are skipped. The implementation must not present these measurements as evidence of scalability.

## Critical implementation defects and risks

1. **Frontend silently loses semantics.** `src/frontend/ClangFrontend.cpp` translates only a narrow pattern of assignment expressions, direct `malloc` assignment, direct field loads/stores, and return of a simple variable. Many expressions are ignored without an error. A benchmark can therefore appear to pass while the analyzer has not analyzed its semantics. Add a coverage ledger for every AST/CFG construct and fail closed with `UNSUPPORTED` when a relevant construct is not modeled.
2. **Not full HRA.** `HRAState` is a map from variables to allocation-site IDs plus a field-labelled edge set. It is the paper's allocation-site projection, not the access-graph automata / heap-data liveness formulation of HRA. The title and claims must match the implementation unless the access-graph layer is actually implemented.
3. **Call and escape semantics are absent.** Parameters are not seeded with abstract heap objects; ordinary calls, return-value propagation across calls, globals, `free`, realloc, function pointers, and interprocedural effects are not modeled. The current exit observable can miss heap effects escaping via globals/parameters. Until modeled, restrict claims to a clearly defined intraprocedural subset and reject unsupported cases.
4. **Allocation recognition is brittle.** It recognizes only a direct call named `malloc` / `__builtin_malloc` as the RHS of a simple assignment. `calloc`, `realloc`, wrappers, `new`, allocator summaries, and allocation inside nested expressions are not represented. Do not silently treat them as no-ops.
5. **Field base extraction is shallow.** `getBaseVar()` accepts only a direct variable reference after stripping casts. Nested paths such as `p->next->next`, array indexing, pointer arithmetic, and more complex lvalues are not faithfully represented by this frontend. Yet these are central to heap-reference analysis.
6. **Potential CFG/liveness assumptions need explicit checks.** Liveness finds a first block with no successors as a unified exit. Real CFGs can have multiple exits, unreachable blocks, exceptional exits, and Clang-generated CFG artifacts. Normalize exits or use a virtual exit node; test loops, early returns, switch, break/continue, and C++ cleanup paths.
7. **MDE does not currently avoid all state costs.** `HRAEncoder::encode()` reconstructs nested vectors/sets on each call; the adapter stores a full `HRAState` copy per unique index; joins still merge materialized `std::set`s; no memoized union/difference is implemented. Thus MDE registration adds work and duplicates metadata rather than replacing state storage. Measure the encoder, lookup, allocation, join, and retained bytes independently.
8. **Canonicalization may not reduce solver work.** It shares equal entry/exit objects but does not alter the transfer schedule or number of worklist iterations. High hit rate from adjacent CFG points is not evidence of structural compression. Report non-adjacent reuse and substructure reuse separately.
9. **Data structures are allocation-heavy.** `std::map`/`std::set` create per-element tree nodes and repeated allocations. Compare dense IDs + sorted vectors, bitsets/roaring bitmaps for sets, flat hash maps/sets, immutable persistent structures, and a simple `unordered_map` hash-consing baseline before attributing results to MDE.
10. **Performance measurements are not adequate.** Current paper reports unoptimized builds, fixed A→B→C order, sub-millisecond timings, no CPU/RAM record, and process high-water RSS dominated by Clang/LLVM. Use Release builds, isolated processes or randomized paired order, hundreds of runs for microbenchmarks, longer macrobenchmarks, confidence intervals, and per-structure byte accounting.
11. **The current driver path is inconsistent.** `src/main.cpp` explicitly says it is a Phase 1 stub and performs no analysis, while other executables are used for analysis/measurement. Establish one documented CLI, deterministic JSON output, exit codes, version output, and smoke tests. Remove outdated phase-completion claims from README.
12. **Correctness evidence is too weak for broad claims.** E1/E2 (encoder injectivity/equality behavior) are assumed and tested only on a small suite; proof sketches are not machine-checked. Add property-based random state generation, encode/decode or equality-consistency tests, randomized differential tests, and explicit unsupported behavior tests.

## Required architectural changes

### P0 — correctness before performance

- Define the supported grammar/semantics in a machine-readable feature matrix.
- Frontend must emit a translation coverage report and reject unsupported relevant constructs.
- Add a virtual exit node and robust CFG handling.
- Model external roots and escaping heap or explicitly exclude them and enforce that exclusion.
- Add `UNSUPPORTED`, `ANALYSIS_ERROR`, and `OK` statuses; never conflate omitted semantics with an empty result.
- Freeze allocation-site identity and field identity rules; add a deterministic semantic result serializer.

### P1 — establish fair baselines

Use four configurations, not only A/B/C:

- A: baseline state representation, no liveness.
- B: baseline representation + liveness.
- C: B + MDE canonicalization.
- D: baseline representation + MDE, no liveness (interaction/ablation).

For B-vs-C, all semantics and analysis work must be identical except representation. Add cheap baselines: pointer/ID interning, `unordered_map` with a stable structural hash, flat sorted-vector states, and (if justified) bitset/BDD encoding. Do not assume MDE is the best baseline simply because it is the proposed technique.

### P2 — make MDE capable of paying for itself

- Avoid reconstructing all nested vectors on every query; cache immutable substructures or compute stable fingerprints incrementally.
- Store canonical indices/references in solver states where possible instead of a duplicate full `HRAState` per unique key.
- Implement memoized join/union on canonical child IDs, with exact equality verification on hash collisions.
- Measure unique nodes at each MDE level, bytes per level, retained bytes, allocations, cache hits, and non-adjacent reuse.
- Add a cheap interning implementation as a control.

### P3 — make evaluation defensible

- Separate frontend/IR/liveness setup time from solver and canonicalizer time; report both end-to-end and component time.
- Run one process per measurement or randomize configuration order; pin affinity where possible.
- Release build; at least 30 independent process runs for macrobenchmarks and enough repetitions to stabilize microbenchmarks; bootstrap 95% CIs and report effect sizes.
- Measure retained bytes using internal allocation accounting and validate with Massif/heaptrack or platform equivalent. RSS is supplementary, not the sole memory metric.
- Report medians, IQR, 95% CI, per-program data, geometric mean speedup, and all failures/timeouts.
- Use a preregistered corpus and include negative controls with low repeated-state density.

## Q1 publication gate

Do not claim Q1 readiness until: (i) analysis semantics are not silently dropped; (ii) the title matches the implemented representation or true access-graph HRA is implemented; (iii) correctness tests pass on a documented supported subset; (iv) there is a realistic-scale benchmark campaign; (v) baselines include cheap interning and D; (vi) retained memory is measured; (vii) a reproducibility archive contains code, corpus manifest, hashes, scripts, raw results, environment, and exact commands; (viii) references and research-gap claims are independently verified. A positive result is not guaranteed; a well-supported null result may still be publishable if the crossover/cost model is generalizable and validated.
