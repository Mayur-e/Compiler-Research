# Experimental Methodology and Execution Plan

## 1. Purpose

This document defines how the MDE-HRA experiments will be executed, recorded, validated, and interpreted.

The central controlled comparison is:

```text
B = Liveness + HRA
C = Liveness + HRA + MDE
```

with the semantic requirement:

```text
CanonicalResult(B) == CanonicalResult(C)
```

Configuration A remains the baseline:

```text
A = HRA
```

This allows the effects of liveness and MDE to be separated.

---

## 2. Experimental Questions

### EQ1 — What is the effect of liveness pruning?

Compare:

```text
A → B
```

Measure runtime, peak memory, HRA states, propagation work, and other available counters.

### EQ2 — What is the effect of MDE?

Compare:

```text
B → C
```

Measure runtime, memory, logical states, physical representation nodes, newly created nodes, reused nodes, and deduplication ratio.

### EQ3 — Does MDE preserve semantics?

For every supported benchmark:

```text
normalize(B_result) == normalize(C_result)
```

This is the primary correctness condition.

### EQ4 — When is MDE beneficial?

Relate MDE behavior to structural repetition, benchmark size, candidate representations, reuse count, and deduplication ratio.

---

## 3. Configurations

| Configuration | Liveness | HRA | MDE | Purpose |
|---|---:|---:|---:|---|
| A | No | Yes | No | HRA baseline |
| B | Yes | Yes | No | Liveness-pruned baseline |
| C | Yes | Yes | Yes | Full proposed system |

The implementation should expose stable configuration selection. The exact command-line syntax may differ, but the semantic meaning must remain fixed.

---

## 4. Controlled Variable

For B versus C, the only intended analysis difference is MDE.

Therefore:

```text
same source
same input
same frontend
same IR
same CFG
same liveness result
same HRA transfer rules
same fixpoint policy
same result normalization
different representation mechanism
```

If other components change simultaneously, a measured improvement cannot be attributed confidently to MDE.

---

## 5. Experimental Pipeline

```text
Source Program
      ↓
Frontend
      ↓
IR
      ↓
CFG
      ↓
USE/DEF extraction
      ↓
Liveness
      ↓
HRA
      ↓
+--------------------+
|                    |
| Configuration B    | Configuration C
| ordinary state     | MDE state
| representation     | representation
|                    |
+--------------------+
      ↓
Fixpoint
      ↓
Canonical Result
      ↓
Correctness Comparison
      ↓
Metrics
      ↓
Stored Experiment Artifact
```

Configuration A skips liveness.

---

## 6. Experiment Stages

### Stage 0 — Toolchain validation

Verify compiler, frontend, IR generation, analysis binary, benchmark source availability, output directory, timing mechanism, and memory measurement mechanism.

### Stage 1 — Correctness smoke tests

Run a few tiny programs through A, B, and C. Verify termination, valid output, semantic comparison, and absence of unexpected crashes.

### Stage 2 — Full correctness corpus

Run all regression programs. Establish correctness before making performance claims.

### Stage 3 — Benchmark validation

Run the selected 5–8 benchmarks. Confirm that all required configurations execute and outputs can be normalized.

### Stage 4 — Performance runs

Repeat each benchmark according to the run policy. Collect runtime, memory, analysis counters, and MDE counters.

### Stage 5 — Scaling experiments

For selected parameterized benchmarks, run B and C at multiple input sizes.

### Stage 6 — Analysis and visualization

Generate tables, runtime plots, memory plots, deduplication plots, scaling plots, and correctness summaries.

---

## 7. Correctness First

Performance experiments must not begin before semantic correctness is established for a benchmark.

For every supported benchmark:

```text
result_B = run(B)
result_C = run(C)

canonical_B = normalize(result_B)
canonical_C = normalize(result_C)

compare(canonical_B, canonical_C)
```

Expected:

```text
canonical_B == canonical_C
```

If this fails, stop performance interpretation for that benchmark. Treat it as a correctness/debugging case.

---

## 8. Result Normalization

Raw internal objects must not be compared directly.

The normalized result should remove implementation-specific details such as:

- memory addresses,
- object identity,
- hash-table order,
- insertion order,
- implementation-only pointer IDs.

Conceptually:

```text
Raw Analysis State
       ↓
Remove implementation identity
       ↓
Canonicalize access paths
       ↓
Canonicalize heap/object relationships
       ↓
Deterministic ordering
       ↓
Canonical Result
```

The exact fields are determined by the project's semantic HRA result definition.

---

## 9. Difference Reporting

If B and C differ, generate a structured report:

```text
Benchmark:
Configuration B:
Configuration C:

Result B:
...

Result C:
...

Missing from B:
...

Missing from C:
...

First semantic divergence:
...

Likely component:
- HRA
- adapter
- MDE
- normalization
- propagation
```

The report should make debugging possible without manually rerunning the complete experiment.

---

## 10. Runtime Measurement

Measure the same logical operation in every configuration.

Prefer separate measurements for:

```text
frontend/IR preparation
analysis execution
```

At minimum record:

```text
total_wall_time
analysis_time
```

If frontend work is cached, document that explicitly.

Never compare a configuration that includes frontend time against one that excludes it.

---

## 11. Environment Control

Record:

```text
CPU
RAM
OS
compiler
compiler flags
analysis build
repository commit/version
```

Avoid heavy unrelated workloads during measurements.

If the implementation has a warm-up effect, use a consistent procedure for all configurations.

---

## 12. Number of Runs

Perform multiple runs when runtime variability matters.

Example:

```text
Run 1
Run 2
Run 3
Run 4
Run 5
```

Report at least:

```text
median
```

and, when useful:

```text
minimum
maximum
standard deviation
```

Use the same number of runs for A, B, and C for a given experiment unless a documented practical exception is required.

---

## 13. Runtime Summary

For each benchmark record:

```text
A_time
B_time
C_time
```

Derived quantities:

```text
A_to_B_speedup = A_time / B_time
B_to_C_speedup = B_time / C_time
A_to_C_speedup = A_time / C_time
```

A value greater than 1 means the denominator configuration is faster.

Always report raw times as well.

---

## 14. Runtime Overhead

If C is slower than B:

```text
MDE_overhead =
    (C_time - B_time) / B_time
```

If C is faster:

```text
MDE_saving =
    (B_time - C_time) / B_time
```

Do not force results into a “speedup” narrative.

---

## 15. Memory Measurement

Record:

```text
peak_memory_A
peak_memory_B
peak_memory_C
```

Use the same measurement mechanism for all configurations.

For B versus C:

```text
memory_reduction =
    (B_memory - C_memory) / B_memory
```

If C uses more memory, report the corresponding overhead honestly.

---

## 16. HRA Work Counters

Where practical, expose:

```text
hra_nodes_created
hra_nodes_merged
hra_edges_created
propagation_steps
state_merges
fixpoint_iterations
```

Counters should be documented precisely.

They are evidence for explaining performance, not substitutes for semantic correctness.

---

## 17. Liveness Counters

Recommended:

```text
liveness_iterations
live_set_changes
live_pointer_count
pruned_pointer_uses
dead_pointer_pruning_events
```

Only expose counters that have a precise implementation meaning.

Do not describe a pointer as dead merely because it is unused in one execution. Liveness is a static future-use property.

---

## 18. MDE Counters

Configuration C should record, where applicable:

```text
mde_lookup_requests
mde_hits
mde_misses
candidate_nodes
new_nodes
reused_nodes
```

A useful invariant is:

```text
candidate_nodes = new_nodes + reused_nodes
```

when candidates are defined as every representation insertion attempt. If the implementation uses another definition, document it.

---

## 19. Deduplication Ratio

Use a fixed definition such as:

```text
dedup_ratio =
    reused_nodes / candidate_nodes
```

Example:

```text
candidate_nodes = 1000
reused_nodes = 700

dedup_ratio = 70%
```

Always report the raw counts as well as the percentage.

---

## 20. Logical vs Physical State Counts

Keep this distinction explicit.

### Logical state

A semantic analysis state.

### Physical node

An implementation object used to store or share that state.

MDE can reduce physical duplication without changing logical semantics.

Therefore:

```text
logical_states_B
logical_states_C
```

may be similar while:

```text
physical_nodes_B
physical_nodes_C
```

are different.

Physical node reduction must never be presented as semantic result reduction.

---

## 21. Scaling Experiment

Select at least one parameterized benchmark.

Example:

```text
N = 10
N = 50
N = 100
N = 500
N = 1000
```

For each size, run B and C and record:

```text
N
runtime_B
runtime_C
memory_B
memory_C
candidate_nodes_C
reused_nodes_C
dedup_ratio_C
```

Use identical logical inputs for B and C.

---

## 22. Scaling Interpretation

Do not claim an asymptotic complexity improvement solely from empirical curves.

Instead use evidence-based statements such as:

> As input size increased, C showed lower physical representation growth than B.

or:

> MDE reuse increased with the number of structurally repeated heap regions.

Formal complexity claims require separate theoretical analysis.

---

## 23. Benchmark Execution

For every benchmark:

```text
1. Verify source/version.
2. Verify configuration.
3. Reset counters.
4. Run A.
5. Run B.
6. Run C.
7. Preserve stdout/stderr.
8. Normalize results.
9. Compare B and C.
10. Save raw and derived metrics.
```

B and C must independently analyze the same program input.

---

## 24. Measurement Contamination

Before every measured run:

- remove stale output files,
- reset counters,
- reset analysis state,
- verify the intended binary/version,
- verify the same input,
- avoid accidental reuse of previous analysis results.

If caching is part of the intended experiment, define it explicitly and apply it consistently.

---

## 25. Timeouts

Every benchmark should have a documented timeout.

If a configuration exceeds it:

```text
status = TIMEOUT
```

Record the benchmark, configuration, input size, and useful last-known counters.

Do not turn a timeout into a fabricated runtime.

---

## 26. Out-of-Memory Handling

If memory limits are exceeded:

```text
status = OOM
```

Record:

```text
benchmark
configuration
input size
last available counters
```

Do not treat OOM as a numerical runtime result.

---

## 27. Non-Convergence

If the analysis does not reach its defined fixpoint:

```text
status = NON_CONVERGED
```

Record:

```text
iteration_count
state_count
last checkpoint
```

Investigate whether the cause is an implementation bug, state explosion, equality/canonicalization problem, or analysis design issue.

---

## 28. Crash Handling

Classify failures rather than using a generic “failed” status:

```text
FRONTEND_CRASH
ANALYSIS_CRASH
MDE_CRASH
ASSERTION_FAILURE
INVALID_INPUT
UNSUPPORTED_FEATURE
TIMEOUT
OOM
NON_CONVERGED
```

This distinction is important for research interpretation.

---

## 29. Unsupported Features

If a benchmark contains unsupported constructs:

```text
status = UNSUPPORTED
```

Record the reason, such as:

```text
unsupported union
unsupported pointer arithmetic
unsupported external call
unsupported frontend construct
```

Do not silently approximate unsupported behavior and call the result sound.

---

## 30. Correctness Matrix

The final report should contain:

| Benchmark | A valid | B valid | C valid | B == C |
|---|---|---|---|---|
| B1 | Yes | Yes | Yes | Yes |
| B2 | Yes | Yes | Yes | Yes |
| B3 | ... | ... | ... | ... |

Values must come from actual runs.

---

## 31. Performance Matrix

Recommended:

| Benchmark | A Time | B Time | C Time | A→B | B→C | A→C |
|---|---:|---:|---:|---:|---:|---:|
| B1 | ... | ... | ... | ... | ... | ... |
| B2 | ... | ... | ... | ... | ... | ... |

This separates the liveness effect from the MDE effect.

---

## 32. Memory Matrix

Recommended:

| Benchmark | A Peak | B Peak | C Peak | B→C Change |
|---|---:|---:|---:|---:|
| B1 | ... | ... | ... | ... |
| B2 | ... | ... | ... | ... |

---

## 33. MDE Matrix

Recommended:

| Benchmark | Candidate | New | Reused | Dedup % |
|---|---:|---:|---:|---:|
| B1 | ... | ... | ... | ... |
| B2 | ... | ... | ... | ... |

These values should come directly from configuration C instrumentation.

---

## 34. Statistical Treatment

For a small research project, descriptive statistics are generally appropriate.

At minimum report:

```text
median
```

For noisy measurements:

```text
median
minimum
maximum
standard deviation
```

If many benchmarks are available, geometric mean speedup may be used for an aggregate view.

Do not use aggregation to hide severe regressions on individual benchmarks.

---

## 35. Geometric Mean

For speedups:

```text
GM =
    (s1 × s2 × ... × sn)^(1/n)
```

Only include valid comparable runs.

Timeout, OOM, or unsupported results must not be silently converted into numerical speedups.

---

## 36. Correctness and Performance Separation

The report must have separate sections:

```text
Correctness
Performance
Memory
Deduplication
Scalability
```

Never argue:

> C is correct because it is faster.

Correctness is established by canonical semantic equality.

Performance is established by measured resource usage.

---

## 37. Expected Result Patterns

These are hypotheses, not guaranteed outcomes.

### Pattern 1 — Liveness helps

```text
A > B
```

in runtime or memory.

Possible interpretation:

liveness pruning reduced unnecessary analysis work.

### Pattern 2 — MDE helps

```text
B > C
```

in runtime and/or memory.

Possible interpretation:

structural deduplication reduced representation overhead.

### Pattern 3 — MDE reduces memory but not runtime

Possible because hashing and canonicalization overhead can offset representation savings.

### Pattern 4 — MDE increases runtime

This is a valid result and may indicate that deduplication overhead exceeds saved work.

### Pattern 5 — High deduplication but no speedup

This may indicate that the main bottleneck lies elsewhere.

All of these outcomes should be reported honestly.

---

## 38. Experiment Reproducibility

Every result should be traceable to:

```text
source version
analysis version
configuration
benchmark
input size
hardware
compiler
flags
run number
timestamp
```

These fields should be stored with the result.

---

## 39. Recommended Result Directory

Use a structure such as:

```text
results/
├── B1/
│   ├── metadata.json
│   ├── A/
│   │   ├── runs.json
│   │   └── summary.json
│   ├── B/
│   │   ├── runs.json
│   │   └── summary.json
│   ├── C/
│   │   ├── runs.json
│   │   └── summary.json
│   └── comparison.json
│
├── B2/
│   └── ...
│
└── summary/
    ├── correctness.csv
    ├── runtime.csv
    ├── memory.csv
    ├── mde.csv
    └── scaling.csv
```

Keep raw and derived results distinguishable.

---

## 40. Raw vs Derived Data

### Raw data

Examples:

```text
run_1_time
run_2_time
peak_memory
counter_values
program_output
canonical_result
```

### Derived data

Examples:

```text
median_time
speedup
memory_reduction
dedup_ratio
geometric_mean
```

Never overwrite raw data with derived values.

---

## 41. Experiment Configuration

A machine-readable experiment configuration is recommended.

Conceptually:

```text
experiment:
  benchmark: list_traversal
  input_size: 1000

  configurations:
    - A
    - B
    - C

  repetitions: 5
  timeout_seconds: ...
```

This makes experiments scriptable and reproducible.

---

## 42. Automated Experiment Runner

The project should eventually provide something like:

```text
scripts/run_experiments.py
```

Responsibilities:

1. load benchmark metadata,
2. verify/build the analysis binary,
3. execute A,
4. execute B,
5. execute C,
6. collect exit status,
7. collect timing,
8. collect memory,
9. collect analysis counters,
10. normalize results,
11. compare B/C,
12. save artifacts.

The runner should orchestrate experiments, not implement analysis logic.

---

## 43. Experiment Runner Safety

The runner should:

- fail clearly on missing binaries,
- record exact command lines,
- record exit codes,
- enforce timeouts,
- preserve stdout/stderr,
- avoid accidental overwriting,
- create unique run directories when needed.

A failed experiment must leave enough evidence to diagnose it.

---

## 44. Result Validation

After collection, run an automated validation step.

Check that:

```text
required fields exist
configuration exists
benchmark exists
run count is correct
B/C canonical results exist
B/C comparison was performed
MDE counters are internally consistent
```

Where applicable verify:

```text
candidate = new + reused
```

---

## 45. Visualization Pipeline

Use a separate analysis script, for example:

```text
scripts/analyze_results.py
```

Input:

```text
results/
```

Output:

```text
plots/
tables/
summary/
```

Do not manually copy numbers into presentation tables.

---

## 46. Required Visualizations

At minimum:

### Runtime

B vs C for each benchmark.

### Memory

B vs C for each benchmark.

### Deduplication

Candidate/new/reused nodes.

### Scaling

Runtime against input size.

### Correctness

A compact pass/fail table.

---

## 47. Avoiding Misleading Graphs

Do not:

- truncate axes without explanation,
- hide failed benchmarks,
- remove slow benchmarks,
- compare different input sizes,
- mix units,
- show only favorable cases.

If logarithmic axes are used, state this clearly.

---

## 48. Interpretation Workflow

After all runs:

### Step 1 — Verify experiment integrity

```text
same source?
same configuration definitions?
same machine?
same tool version?
```

### Step 2 — Verify correctness

```text
B == C?
```

### Step 3 — Inspect MDE counters

```text
Is reuse actually occurring?
```

### Step 4 — Inspect performance

```text
Did runtime change?
Did memory change?
```

### Step 5 — Correlate behavior

```text
Does higher reuse correspond to lower physical representation?
Does that correlate with performance?
```

### Step 6 — Identify exceptions

```text
Where did MDE not help?
Where did overhead dominate?
```

### Step 7 — Write conclusions only from observed evidence.

---

## 49. What Not to Claim

Do not claim:

> MDE always improves runtime.

unless the evidence supports that broad statement.

Do not claim:

> MDE reduces the number of logical analysis states.

if only physical representation nodes were reduced.

Do not claim:

> Liveness is exact for all programs.

if the implementation is restricted to a supported subset.

Do not claim:

> The analysis is scalable.

without defining the benchmark scale and observed behavior.

---

## 50. Main Experimental Story

The final research story should follow:

### Step 1 — Baseline

```text
A = HRA
```

shows the original analysis cost.

### Step 2 — Liveness

```text
A → B
```

shows whether dead future uses can be pruned.

### Step 3 — Deduplication

```text
B → C
```

shows whether repeated structural representation can be reused.

### Step 4 — Correctness

```text
B == C
```

shows whether the optimization preserves the normalized result.

### Step 5 — Performance

Measure:

```text
runtime
memory
node construction
reuse
dedup ratio
```

### Step 6 — Interpretation

Explain when, why, and under what workload characteristics MDE helps or does not help.

---

## 51. Example Experimental Walkthrough

Consider:

```text
repeated_heap.c
```

Suppose it constructs many structurally similar list nodes.

Run:

```text
A
B
C
```

Record:

```text
A_time = ...
B_time = ...
C_time = ...

B_memory = ...
C_memory = ...

candidate_nodes = ...
new_nodes = ...
reused_nodes = ...
```

Then verify:

```text
canonical_B == canonical_C
```

If:

```text
candidate = 10000
new = 3000
reused = 7000
```

then:

```text
dedup_ratio = 70%
```

The interpretation should remain evidence-based:

> The benchmark produced substantial structural reuse. MDE reused 70% of candidate representations. The normalized B and C results were equivalent. Runtime and memory effects were then evaluated independently.

---

## 52. Example Failure Walkthrough

If:

```text
B result != C result
```

then:

```text
1. Mark benchmark correctness failure.
2. Preserve B result.
3. Preserve C result.
4. Generate semantic diff.
5. Determine first divergence.
6. Check adapter canonicalization.
7. Check MDE equality/hash.
8. Check state mutation and aliasing.
9. Check propagation.
10. Add a regression test.
11. Fix implementation.
12. Rerun.
```

Do not remove the benchmark simply because it exposed a bug.

---

## 53. Experiment Phases During Development

Experiments should evolve with implementation maturity.

### Phase 1

Tiny correctness tests.

### Phase 2

HRA baseline benchmarks.

### Phase 3

Liveness + HRA validation.

### Phase 4

MDE unit/integration experiments.

### Phase 5

B/C equivalence on the benchmark suite.

### Phase 6

Performance experiments.

### Phase 7

Scaling experiments.

### Phase 8

Final benchmark evaluation.

This keeps performance measurement from distracting from correctness during early development.

---

## 54. Final Results Checklist

Before writing the final report:

- [ ] All benchmark sources are versioned.
- [ ] A/B/C definitions are fixed.
- [ ] Correctness comparison is automated.
- [ ] Canonical results are stored.
- [ ] Runtime measurement is consistent.
- [ ] Memory measurement is consistent.
- [ ] MDE counters are enabled.
- [ ] Dedup ratio definition is fixed.
- [ ] Raw data is preserved.
- [ ] Failed runs are classified.
- [ ] Unsupported benchmarks are documented.
- [ ] No favorable-only benchmark selection.
- [ ] Scaling experiments are reproducible.
- [ ] Plots are generated from raw results.
- [ ] Final claims match the evidence.

---

## 55. Definition of Done

The experimental methodology is complete when:

```text
Input
  ↓
Benchmark
  ↓
A/B/C
  ↓
Correctness
  ↓
Metrics
  ↓
Raw artifacts
  ↓
Derived results
  ↓
Plots/tables
  ↓
Interpretation
```

can be executed repeatedly without manually changing analysis logic or copying numbers between files.

The experiment system must make it possible to answer:

```text
What was tested?
With which configuration?
On which input?
How many times?
What was the result?
Was it semantically equivalent?
How much work was performed?
How much representation was reused?
What happened to runtime?
What happened to memory?
```

---

## 56. Final Experimental Principle

The evaluation must separate three questions:

```text
1. Does the optimization preserve the result?
2. Does it reduce representation/work?
3. Does that reduction actually improve resource usage?
```

They are related, but they are not the same question.

Therefore the strongest evaluation order is:

```text
Correctness
    ↓
Representation/work
    ↓
Runtime/memory
    ↓
Scalability
    ↓
Research interpretation
```

This keeps the MDE-HRA evaluation technically defensible and prevents performance observations from being mistaken for semantic correctness.
