# Benchmark Suite and Evaluation Programs

## 1. Purpose

This document defines the benchmark suite for the MDE-HRA project.

The benchmark suite is not merely a collection of programs used to demonstrate that the implementation runs. It is part of the experimental methodology. The selected programs must expose the exact behaviors that the project is designed to study:

- heap references,
- pointer propagation,
- repeated heap structures,
- pointer liveness,
- dead-pointer regions,
- repeated or structurally equivalent analysis states,
- opportunities for MDE/LatticeHashForest deduplication,
- and scalability pressure.

The benchmark suite must therefore support both **correctness validation** and **performance evaluation**.

The main research comparison is:

```text
Configuration B:
Liveness + HRA

versus

Configuration C:
Liveness + HRA + MDE
```

The expected semantic invariant is:

```text
CanonicalResult(B) == CanonicalResult(C)
```

Performance differences are then measured independently.

---

## 2. Research Questions Supported by the Benchmarks

The benchmark suite must help answer the following questions.

### RQ1 — Does liveness reduce unnecessary HRA work?

Compare:

```text
A = HRA
B = Liveness + HRA
```

Measure:

- runtime,
- peak memory,
- number of analysis states,
- number of HRA nodes,
- number of propagated states,
- and any other implementation-level work counters.

Expected interpretation:

If liveness is effective, B should avoid work associated with pointers that cannot affect future program behavior.

This is not assumed in advance. It must be measured.

### RQ2 — Does MDE reduce duplicated representation?

Compare:

```text
B = Liveness + HRA
C = Liveness + HRA + MDE
```

Measure:

- logical HRA states,
- physical representation nodes,
- newly constructed nodes,
- reused nodes,
- deduplication ratio,
- runtime,
- and memory.

The key question is whether structurally equivalent states can be represented more compactly.

### RQ3 — Does MDE preserve analysis semantics?

For every supported benchmark:

```text
CanonicalResult(B)
        ==
CanonicalResult(C)
```

If the results differ, the implementation must treat the difference as a correctness failure or an explicitly documented unsupported behavior. It must not be hidden by the benchmark report.

### RQ4 — Where does deduplication help most?

The benchmark suite should contain programs with different levels of structural repetition.

This allows the experiment to distinguish:

```text
low repetition
      ↓
moderate repetition
      ↓
high repetition
```

rather than presenting only a single favorable case.

### RQ5 — How does program structure affect scalability?

The suite should vary characteristics such as:

- number of heap objects,
- chain depth,
- branching,
- repeated allocation patterns,
- pointer assignments,
- loops,
- dead pointer regions,
- and repeated access paths.

---

## 3. Benchmark Selection Principles

Benchmark selection must follow explicit rules.

### 3.1 Relevance

A benchmark must contain at least one feature relevant to pointer/heap analysis.

Examples:

- `malloc`/`free`,
- structure allocation,
- linked structures,
- pointer assignment,
- pointer dereference,
- field traversal,
- loops over heap structures,
- aliases,
- or repeated heap construction.

### 3.2 Diversity

Do not select eight programs that exercise the same behavior.

The suite should contain different structural patterns.

### 3.3 Controlled Complexity

The benchmark must be understandable enough that a small reference result can be established.

For the first implementation, very large applications should not be used as the primary debugging benchmarks.

### 3.4 Reproducibility

Each benchmark must have:

- a stable source file,
- deterministic input where practical,
- a documented build command,
- a documented analysis command,
- expected configuration,
- and a recorded result.

### 3.5 No Cherry-Picking

Benchmarks must not be selected only because they produce good speedups.

Programs with little or no deduplication opportunity are valuable because they establish where MDE does not help.

### 3.6 Supported Feature Boundary

Initial benchmarks should stay inside the project's first implementation scope:

- C programs,
- singly-linked heap structures,
- one or two named fields where possible,
- ordinary pointer assignments,
- field dereferences,
- loops,
- branches,
- and dynamic allocation.

Avoid making unsupported language features central to the first benchmark suite.

Initially avoid:

- unions,
- complex pointer arithmetic,
- arbitrary casts,
- concurrency,
- inline assembly,
- highly compiler-specific extensions,
- and very complicated interprocedural behavior unless explicitly supported.

---

## 4. Recommended Initial Benchmark Categories

The first suite should contain approximately **5–8 open-source or openly available C programs**, supplemented during development by tiny synthetic programs.

A useful structure is:

| Category | Purpose |
|---|---|
| Tiny linked-list program | Basic correctness |
| Linked-list traversal | Access-path propagation |
| List insertion/deletion | Pointer updates and liveness |
| Repeated list construction | Structural duplication |
| Loop-heavy heap program | Fixpoint/scalability |
| Branch-heavy pointer program | CFG/liveness interaction |
| Multiple similar heap regions | MDE reuse opportunity |
| Larger realistic C program | External validity |

The final selection should be based on the actual supported frontend and HRA implementation.

---

## 5. Development Benchmarks vs Evaluation Benchmarks

These must be separated.

### 5.1 Development benchmarks

Small hand-written programs are used to debug the implementation.

They should be tiny enough that the expected behavior can be reasoned about manually.

Examples:

```c
struct Node {
    int data;
    struct Node *next;
};

int main() {
    struct Node *p = malloc(sizeof(struct Node));
    p->next = 0;
    return 0;
}
```

or:

```c
struct Node *p = malloc(sizeof(struct Node));
struct Node *q = p;
q->next = 0;
```

These programs are not necessarily part of the final performance table.

### 5.2 Evaluation benchmarks

These are the 5–8 programs selected for the final experiments.

They should be stable, representative, and sufficiently different from each other.

---

## 6. Required Benchmark Feature Matrix

Every final benchmark must be documented using a feature matrix.

Recommended format:

| Benchmark | Heap allocation | Linked structure | Field access | Loop | Branch | Pointer reassignment | Repetition | Dead-pointer opportunity |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| B1 | Yes | Yes | Yes | No/Low | Low | Yes | Low | Yes |
| B2 | Yes | Yes | Yes | Yes | Low | Yes | Medium | Yes |
| B3 | Yes | Yes | Yes | Yes | Medium | Yes | Medium | Yes |
| B4 | Yes | Yes | Yes | Yes | High | Yes | High | Yes |
| B5 | Yes | Yes | Yes | Yes | Medium | Yes | High | Yes |
| B6 | Yes | Yes/Partial | Yes | Yes | Medium | Yes | Medium | Yes |
| B7 | Yes | Yes | Yes | Yes | High | Yes | High | Yes |
| B8 | Yes | Yes/Partial | Yes | Yes | High | Yes | Medium/High | Yes |

The values must be filled from actual source inspection. They must never be invented simply to complete the table.

---

## 7. Benchmark Metadata

Every benchmark should have a metadata record.

Suggested fields:

```text
benchmark_id
name
source
license
source_commit_or_version
file_list
build_command
analysis_command
input_description
expected_exit_status
supported_features
unsupported_features
heap_structure
number_of_functions
approximate_LOC
allocation_sites
free_sites
pointer_assignment_sites
field_access_sites
loop_count
branch_count
```

Additional experiment metadata:

```text
configuration
timeout
hardware
compiler_version
analysis_version
git_commit
run_number
```

This makes the experiment reproducible.

---

## 8. Benchmark Scaling Parameters

Where possible, a benchmark should expose a controlled size parameter.

For example:

```text
N = 10
N = 100
N = 1000
N = 5000
```

For a linked-list benchmark:

```text
create N nodes
traverse N nodes
```

For repeated heap construction:

```text
construct N structurally similar regions
```

This is particularly useful for studying scalability.

The same source program can therefore produce several benchmark instances:

```text
list_10
list_100
list_1000
list_5000
```

This should only be done where the frontend and analysis can process the generated programs consistently.

---

## 9. Benchmark Families

### 9.1 Family A — Minimal heap correctness

Purpose:

- verify allocation,
- verify structure creation,
- verify pointer assignment,
- verify field access,
- verify result normalization.

Typical pattern:

```text
allocation
  ↓
pointer assignment
  ↓
field write
  ↓
field read
```

This family is primarily for correctness, not speedup.

### 9.2 Family B — Linked-list traversal

Purpose:

- access-path propagation,
- repeated `next` traversal,
- loop behavior,
- liveness across loop boundaries.

Pattern:

```text
head
 ↓
head->next
 ↓
head->next->next
 ↓
...
```

This is a core benchmark family because singly-linked structures are within the initial scope.

### 9.3 Family C — Pointer reassignment

Purpose:

- demonstrate why liveness matters,
- create pointers whose future usefulness changes,
- test kill/gen behavior.

Pattern:

```text
p = allocation
q = p
use(q)

p = another_allocation

use(p)
```

The implementation should distinguish pointers that remain relevant from pointers whose future use has ended.

### 9.4 Family D — Repeated heap construction

Purpose:

- create structurally similar HRA states,
- expose MDE deduplication,
- measure node reuse.

Pattern:

```text
allocate A
initialize A

allocate B
initialize B

allocate C
initialize C
```

The physical addresses may differ while the structural representation can be equivalent.

This is a key family for MDE evaluation.

### 9.5 Family E — Loop-heavy heap programs

Purpose:

- stress fixpoint computation,
- expose repeated states,
- test convergence,
- measure work reduction.

Pattern:

```text
while (...) {
    pointer traversal
    heap update
}
```

This family should be tested at multiple sizes if practical.

### 9.6 Family F — Branch-heavy programs

Purpose:

- test CFG joins,
- liveness merge,
- HRA state merge,
- MDE canonicalization after different paths converge.

Pattern:

```text
if (condition) {
    build/use heap region A
} else {
    build/use heap region B
}

use common pointer
```

This is important because deduplication must work correctly across control-flow joins.

### 9.7 Family G — Mixed realistic programs

Purpose:

- validate behavior outside synthetic examples,
- provide external validity,
- test implementation assumptions.

These should be selected only after the supported feature set is stable.

---

## 10. Primary Experimental Matrix

Every final benchmark should be run under all three configurations.

| Configuration | Liveness | HRA | MDE |
|---|---:|---:|---:|
| A | No | Yes | No |
| B | Yes | Yes | No |
| C | Yes | Yes | Yes |

The primary comparisons are:

```text
A vs B
```

to study liveness, and:

```text
B vs C
```

to study MDE.

The overall comparison:

```text
A vs C
```

may also be reported, but it should not replace the controlled B-vs-C comparison.

---

## 11. Metrics Collected Per Benchmark

At minimum collect:

### Correctness

```text
canonical_result_A
canonical_result_B
canonical_result_C
B_equals_C
```

### Runtime

```text
wall_clock_time
analysis_time
```

### Memory

```text
peak_memory
```

### HRA work

```text
hra_nodes_created
hra_states_created
propagation_steps
fixpoint_iterations
```

### Liveness

```text
liveness_iterations
live_set_updates
```

### MDE

```text
candidate_nodes
new_nodes
reused_nodes
dedup_hits
dedup_misses
dedup_ratio
```

A useful definition is:

```text
dedup_ratio =
    reused_nodes / candidate_nodes
```

The exact definition must remain fixed across all experiments.

If the implementation uses a different denominator, document it explicitly.

---

## 12. Runtime Reporting

Runtime should be reported consistently.

For each configuration:

```text
Run 1
Run 2
...
Run K
```

Use a consistent number of repetitions.

For example:

```text
K = 5
```

if runtime is stable enough to justify it.

Report:

- median runtime,
- minimum runtime if useful,
- and variability when meaningful.

Do not compare one warm run of one configuration with one cold run of another.

Control:

- compiler version,
- optimization settings,
- machine,
- input size,
- environment,
- and analysis build.

---

## 13. Memory Reporting

Memory should be measured using the same mechanism for all configurations.

The important comparison is:

```text
B peak memory
vs
C peak memory
```

A lower number of logical HRA states does not automatically imply lower memory.

The MDE representation can have its own metadata and hash structures.

Therefore memory must be measured rather than inferred.

---

## 14. Deduplication Reporting

For configuration C, report enough information to show whether deduplication is actually happening.

Example:

```text
Candidate representations: 1000
New representations:        300
Reused representations:     700
Dedup ratio:                70%
```

Also report:

```text
B logical states: 1000
C logical states: 1000
C physical nodes: 300
```

if those are the actual semantics of the implementation.

This distinction is essential:

```text
logical state count
        !=
physical representation count
```

Do not call a reduction in physical nodes a reduction in the analysis result.

---

## 15. Correctness Evaluation

Correctness must be evaluated independently from performance.

For every benchmark:

```text
result_B = normalize(run(B))
result_C = normalize(run(C))

assert result_B == result_C
```

The comparison should be based on canonical semantic content.

It must not depend on:

- pointer addresses,
- allocation order,
- hash-table ordering,
- object identity in the implementation,
- or traversal order.

If the results differ:

1. preserve both outputs,
2. generate a difference report,
3. identify the first divergent semantic state if possible,
4. classify the failure,
5. do not report the benchmark as successfully optimized.

---

## 16. Negative and Boundary Benchmarks

The suite should contain cases where MDE has little opportunity.

Example:

```text
A
B
C
```

where the heap states are structurally different.

This is useful because it prevents the evaluation from assuming that every state should be deduplicated.

Expected behavior may be:

```text
dedup_ratio ≈ 0
```

and that is a valid result.

Similarly, include cases with:

- different field structures,
- different pointer targets,
- different live-pointer sets,
- and different control-flow states.

The deduplication mechanism must not incorrectly merge these.

---

## 17. Synthetic Benchmarks for Specific Bugs

In addition to final benchmarks, maintain a regression corpus.

Recommended files:

```text
tests/programs/
    basic_alloc.c
    alias.c
    dead_pointer.c
    live_pointer.c
    list_traversal.c
    list_branch.c
    repeated_heap.c
    no_dedup.c
    dedup_equivalent.c
    dedup_non_equivalent.c
    loop_heap.c
```

Each program should target one behavior.

When a bug is fixed, add a small regression program if possible.

This is much better than relying only on a large benchmark to reproduce the bug.

---

## 18. Open-Source Benchmark Selection Procedure

For each candidate open-source program:

### Step 1 — Source inspection

Determine:

- language,
- build system,
- dependencies,
- heap usage,
- pointer usage,
- supported constructs.

### Step 2 — Feature classification

Mark:

```text
supported
partially supported
unsupported
```

Do not hide unsupported constructs.

### Step 3 — Minimal extraction

If the full project is too large, identify a representative source file or small executable component.

The extraction must preserve the behavior relevant to the experiment.

### Step 4 — Frontend validation

Run the program through the project's frontend/IR generation.

If the frontend cannot process it, it should not enter the primary benchmark table until the limitation is understood.

### Step 5 — Baseline HRA

Run configuration A.

Verify:

- termination,
- sensible result,
- no unexpected crashes.

### Step 6 — Liveness + HRA

Run configuration B.

Verify:

```text
B is semantically valid
```

### Step 7 — MDE integration

Run configuration C.

Verify:

```text
B result == C result
```

### Step 8 — Performance experiment

Only after correctness is established should timing/memory/dedup measurements become part of the final evaluation.

---

## 19. Benchmark Inclusion Criteria

A program becomes a final benchmark only if:

- it can be processed by the intended toolchain,
- it exercises a relevant analysis behavior,
- its source/version is recorded,
- its build/analysis procedure is reproducible,
- configuration A completes,
- configuration B completes,
- configuration C completes,
- and B/C results can be compared semantically.

If C is not yet implemented, the program can remain a development benchmark but should not be included in final MDE performance claims.

---

## 20. Benchmark Exclusion Criteria

Exclude a candidate from the primary evaluation if:

- it relies on unsupported language constructs that dominate the program,
- it requires nondeterministic external behavior,
- it cannot be reproduced,
- it cannot be processed consistently,
- it terminates differently under configurations for reasons unrelated to the analysis,
- or its result cannot be normalized reliably.

Document exclusions.

Do not silently remove difficult benchmarks after seeing their performance.

---

## 21. Benchmark Ordering During Development

Use a staged progression.

### Stage 1

```text
basic_alloc.c
```

Goal:

- frontend,
- CFG,
- HRA basics.

### Stage 2

```text
alias.c
dead_pointer.c
live_pointer.c
```

Goal:

- liveness,
- pointer binding,
- semantic result.

### Stage 3

```text
list_traversal.c
list_branch.c
```

Goal:

- loops,
- branches,
- joins,
- fixpoint.

### Stage 4

```text
repeated_heap.c
dedup_equivalent.c
dedup_non_equivalent.c
```

Goal:

- MDE adapter,
- canonicalization,
- deduplication correctness.

### Stage 5

Open-source programs.

Goal:

- scalability,
- robustness,
- external validity.

---

## 22. Recommended Final Evaluation Table

The final paper/report should contain a table similar to:

| Benchmark | LOC | Heap objects | A Time | B Time | C Time | B Mem | C Mem | Candidate Nodes | Reused | Dedup % | B=C |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| B1 | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | Yes |
| B2 | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | Yes |
| B3 | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | Yes |
| B4 | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | Yes |
| B5 | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | Yes |

Do not fill values until actual experiments have been performed.

---

## 23. Recommended Graphs

The benchmark suite should support the following visualizations.

### Runtime

```text
Benchmark
   |
   +--- A
   +--- B
   +--- C
```

### Memory

```text
Benchmark
   |
   +--- B
   +--- C
```

### Deduplication

```text
Benchmark
   |
   +--- candidate
   +--- new
   +--- reused
```

### Scalability

```text
Input size
    |
    +--- runtime(B)
    +--- runtime(C)
```

### Correctness

A simple table is often clearer:

```text
Benchmark | B == C
----------|------
B1        | Yes
B2        | Yes
...
```

Correctness should not be represented only by a performance graph.

---

## 24. Benchmark Reproducibility Record

Every experiment should save:

```text
results/
    benchmark_name/
        metadata.json
        config_A.json
        config_B.json
        config_C.json
        result_A.json
        result_B.json
        result_C.json
        canonical_A.json
        canonical_B.json
        canonical_C.json
        difference_B_C.txt
        logs/
```

If a run has no difference, `difference_B_C.txt` can contain:

```text
NO DIFFERENCE
```

This creates an auditable experiment trail.

---

## 25. Avoiding Invalid Comparisons

Never compare:

```text
different input sizes
different compiler settings
different source versions
different machine conditions
different timeout policies
```

as if they were a valid A/B/C experiment.

All three configurations must process the same logical input.

The only intended difference should be the analysis configuration.

---

## 26. Performance Interpretation Rules

The benchmark results must be interpreted carefully.

### If B is faster than A

Possible interpretation:

```text
liveness pruning reduced unnecessary analysis work
```

provided the counters support this explanation.

### If C is faster than B

Possible interpretation:

```text
MDE reduced representation overhead or repeated structural construction
```

but this should be supported by:

- reuse counts,
- dedup ratio,
- node counts,
- and memory/runtime measurements.

### If C is slower than B

This is still a meaningful result.

Possible explanation:

```text
deduplication overhead exceeded the savings
```

The experiment should report this rather than treating it as a failed benchmark.

### If C has fewer physical nodes but higher runtime

That is possible.

It means representation compression occurred, but hashing/canonicalization overhead may dominate.

This is precisely why multiple metrics are necessary.

---

## 27. Scaling Study

At least one benchmark family should ideally support increasing input size.

Example:

```text
N = 10
N = 100
N = 1000
N = 5000
```

For each N:

```text
run B
run C
```

Record:

```text
runtime
memory
logical states
physical nodes
dedup ratio
```

Then examine whether MDE changes the growth trend.

Do not claim asymptotic improvement from a small number of data points alone. Describe the observed trend.

---

## 28. Final Benchmark Selection Strategy

The final suite should approximately contain:

1. **one minimal correctness benchmark**
2. **one linked-list traversal benchmark**
3. **one pointer/liveness benchmark**
4. **one branch/merge benchmark**
5. **one repeated-heap benchmark**
6. **one loop-heavy benchmark**
7. **one high-dedup benchmark**
8. **one realistic open-source benchmark**

If only five benchmarks are practical, prioritize:

```text
minimal correctness
linked-list traversal
liveness-sensitive
repeated-heap/high-dedup
realistic open-source
```

The exact final programs must be selected only after source inspection and frontend compatibility testing.

---

## 29. Relationship to the Research Claim

The benchmark suite must allow the final conclusion to be phrased narrowly and defensibly.

The intended claim is not:

> MDE makes all pointer analysis faster.

The intended evaluation is closer to:

> For the supported heap-reference analysis workloads, liveness pruning and structural deduplication can be evaluated for their effect on analysis work, memory usage, and representation size while preserving the normalized analysis result.

The benchmark suite exists to test that claim rather than assume it.

---

## 30. Definition of Done for the Benchmark Phase

The benchmark phase is complete when:

- [ ] 5–8 final candidate programs are selected
- [ ] each program has a source/version record
- [ ] each program has a feature classification
- [ ] build and analysis commands are documented
- [ ] supported/unsupported features are recorded
- [ ] all benchmarks pass configuration A
- [ ] all benchmarks pass configuration B
- [ ] all supported benchmarks pass configuration C
- [ ] B/C semantic equivalence is checked
- [ ] runtime is measured consistently
- [ ] peak memory is measured consistently
- [ ] MDE reuse/dedup counters are collected
- [ ] scaling parameters are defined where applicable
- [ ] raw result files are preserved
- [ ] benchmark exclusions are documented
- [ ] no benchmark was removed solely because it produced unfavorable results
- [ ] final tables and graphs can be generated from raw data

---

## 31. Final Principle

The benchmark suite should be treated as an experimental instrument.

It must answer:

```text
What did we run?
Why did we run it?
What behavior does it exercise?
What did A do?
What did B do?
What did C do?
Did B and C preserve the same result?
How much work was reduced?
How much representation was reused?
Did runtime improve?
Did memory improve?
Where did MDE not help?
```

If these questions can be answered from the stored benchmark artifacts, the evaluation is reproducible and scientifically useful.
