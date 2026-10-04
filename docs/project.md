# MDE-HRA Project --- Detailed Implementation Plan

## 1. Project Overview

**MDE-HRA: Deduplication-Accelerated Heap Reference Analysis for
Scalable, Liveness-Pruned Pointer Analysis**

This project implements and evaluates an integration of:

1.  Liveness-based pruning
2.  Heap Reference Analysis (HRA) using access-path/access-graph
    representations
3.  Multi-level Deduplication Engine (MDE) / LatticeHashForest

The central pipeline is:

``` text
C Program
   ↓
IR / CFG
   ↓
Backward Liveness
   ↓
Live Pointer Information
   ↓
Heap Reference Analysis
   ↓
Access-Path / HRA Representation
   ↓
MDE / LatticeHashForest Deduplication
   ↓
Forward Propagation / Fixpoint
   ↓
Heap-Aware Analysis Result
```

The main contribution is the integration of these components, followed
by empirical evaluation. The project is not intended to implement every
feature of general pointer analysis.

------------------------------------------------------------------------

## 2. Research Question

### Main question

> Can structural deduplication make liveness-pruned heap-reference
> analysis more scalable while preserving the analysis result?

### Supporting questions

-   Does liveness reduce unnecessary analysis work?
-   Can HRA access-path structures be represented through MDE?
-   How much repeated structure exists?
-   How much memory can be saved?
-   Does runtime improve?
-   Does MDE preserve the logical analysis result?
-   Does MDE add benefit after liveness pruning?
-   Does benefit increase with repeated heap structures?

------------------------------------------------------------------------

## 3. Problem Statement

Precise pointer analysis for heap-allocated C/C++ structures can require
detailed access-path representations. Similar or identical heap
structures may appear at multiple program points and contexts.

This creates two problems:

1.  Irrelevant pointer information may continue to be processed.
2.  Useful information may itself contain repeated structural
    representations.

The project therefore combines:

``` text
Liveness pruning
        +
Heap Reference Analysis
        +
Structural deduplication
```

with the goal of reducing memory and analysis cost while preserving the
logical result.

------------------------------------------------------------------------

## 4. Project Scope

### Included

-   C programs
-   pointer variables
-   heap allocation
-   structures containing pointer fields
-   singly-linked structures
-   one or two named fields
-   access paths such as:
    -   `p`
    -   `p->next`
    -   `p->next->next`
    -   `p->next->data`
-   backward pointer liveness
-   access-path-based heap representation
-   structural deduplication
-   forward propagation
-   runtime and memory measurement
-   comparison of deduplicated and non-deduplicated analysis

### Initially excluded

-   unions
-   pointer arithmetic
-   unrestricted casts
-   unrestricted array/pointer combinations
-   the full general HRA model
-   unrestricted context sensitivity
-   arbitrary unsupported C features
-   complete industrial compiler optimization integration

The proposal specifically restricts the initial implementation to
singly-linked structures and one or two named fields.

------------------------------------------------------------------------

# 5. Core Concepts

## 5.1 Liveness

Liveness answers:

> Which pointer information can still affect future computation?

It is a backward analysis over the CFG.

Conceptually:

``` text
CFG
 ↓
Backward data-flow analysis
 ↓
LIVE-IN / LIVE-OUT at program points
```

------------------------------------------------------------------------

## 5.2 HRA

HRA answers:

> What heap locations can be reached through a pointer/access path?

Example:

``` text
p -> next -> next -> data
```

corresponds conceptually to:

``` text
p.next.next.data
```

------------------------------------------------------------------------

## 5.3 MDE

MDE answers:

> Are we storing the same structural information repeatedly?

For example:

``` text
p -> next -> data
q -> next -> data
r -> next -> data
```

contains the repeated suffix:

``` text
next -> data
```

The intended representation is shared rather than independently
recreated.

------------------------------------------------------------------------

# 6. High-Level Architecture

``` text
                 Source C Program
                        |
                        v
                Frontend / IR
                        |
                        v
                 Program CFG
                        |
                        v
               Backward Liveness
                        |
                        v
                 Live Pointers
                        |
                        v
                 HRA Construction
                        |
                        v
              Access-Path Structure
                        |
                        v
              MDE Deduplication
                        |
                        v
              Forward Propagation
                        |
                        v
                    Fixpoint
                        |
                        v
                  Query Results
                        |
                        v
          Performance / Correctness Data
```

MDE must optimize the representation and repeated work without changing
the logical meaning of the analysis.

------------------------------------------------------------------------

# 7. Implementation Roadmap

## Phase 0 --- Definition and Setup

### Tasks

-   Freeze implementation scope.
-   Decide supported C constructs.
-   Decide compiler/IR infrastructure.
-   Study existing MDE/LatticeHashForest implementation.
-   Study HRA representation.
-   Study liveness-based pointer-analysis implementation.
-   Create repository structure.
-   Establish build system.
-   Establish initial test framework.
-   Document assumptions.

### Deliverable

A buildable project skeleton and a frozen scope.

------------------------------------------------------------------------

# 8. Phase 1 --- Frontend and IR

## Objective

Convert supported C programs into a representation suitable for
analysis.

### Tasks

-   Process source C programs into the selected IR.
-   Obtain the CFG.
-   Identify basic blocks and program points.
-   Identify pointer operations.
-   Identify allocations.
-   Identify structure-field accesses.
-   Identify pointer definitions and uses.

### Deliverable

A program representation usable by liveness and HRA.

------------------------------------------------------------------------

# 9. Phase 2 --- Baseline Heap Representation

## Objective

Create the non-deduplicated representation first.

### Tasks

-   Define pointer representation.
-   Define heap-object representation.
-   Define field representation.
-   Define access-path representation.
-   Define graph/node representation.
-   Define propagation operations.
-   Define merge/union behavior.
-   Define query behavior.

Example:

``` text
p -> next -> next -> data
```

must be representable before MDE is introduced.

### Deliverable

A working baseline representation without MDE.

------------------------------------------------------------------------

# 10. Phase 3 --- Scoped HRA

## Objective

Implement the selected subset of Heap Reference Analysis.

### Tasks

-   Implement access-path roots.
-   Implement field transitions.
-   Implement path extension.
-   Implement HRA nodes/graphs.
-   Implement merging.
-   Implement propagation.
-   Implement heap queries.
-   Test simple linked structures.
-   Test repeated paths.
-   Test expected results manually.

### Deliverable

A working non-deduplicated HRA baseline.

------------------------------------------------------------------------

# 11. Phase 4 --- Backward Liveness

## Objective

Determine which pointer information is relevant at each program point.

Conceptually:

``` text
LIVE-IN  = USE ∪ (LIVE-OUT - DEF)
LIVE-OUT = union of successor LIVE-IN sets
```

The exact pointer-specific treatment must follow the selected liveness
formulation.

### Tasks

-   Use the CFG.
-   Identify pointer uses and definitions.
-   Initialize live sets.
-   Propagate liveness backward.
-   Iterate to fixed point.
-   Store live information at program points.
-   Test branches.
-   Test loops.
-   Test dead pointer assignments.

### Deliverable

A standalone liveness module.

------------------------------------------------------------------------

# 12. Phase 5 --- Liveness + HRA

## Objective

Make HRA process only relevant pointer information.

Pipeline:

``` text
Program
   ↓
CFG
   ↓
Backward Liveness
   ↓
Live Pointers
   ↓
HRA
```

### Tasks

-   Connect liveness to HRA.
-   Prune dead pointers from further processing.
-   Preserve required live heap information.
-   Test branches and loops.
-   Compare with non-pruned HRA.
-   Verify required query results.

### Deliverable

A working liveness-pruned HRA baseline.

------------------------------------------------------------------------

# 13. Phase 6 --- MDE Study and Adapter Design

## Objective

Define the interface:

``` text
HRA Representation
        ↓
HRA → MDE Adapter
        ↓
MDE Representation
```

### Tasks

-   Study MDE/LatticeHashForest data model.
-   Determine required MDE structures.
-   Study nested/recursive representation.
-   Determine equality and reuse behavior.
-   Identify required construction operations.
-   Identify required union/set operations.
-   Design the HRA-to-MDE adapter.
-   Define deduplication statistics.

### Deliverable

A documented HRA/MDE interface design.

------------------------------------------------------------------------

# 14. Phase 7 --- MDE-HRA Representation

## Objective

Represent HRA access-path structures using MDE.

Example:

``` text
Before:

p -> next -> data
q -> next -> data
r -> next -> data

After:

p ──┐
q ──┼──> next -> data
r ──┘
```

### Tasks

-   Define HRA-MDE node representation.
-   Represent roots.
-   Represent fields.
-   Represent nested structures.
-   Implement node reuse.
-   Implement structural equality handling.
-   Integrate required set operations.
-   Count constructed nodes.
-   Count reused nodes.
-   Validate sharing.

### Deliverable

An MDE-backed HRA representation.

------------------------------------------------------------------------

# 15. Phase 8 --- Full Integration

## Final pipeline

``` text
C Program
   ↓
IR / CFG
   ↓
Liveness
   ↓
Live Pointer Set
   ↓
HRA
   ↓
MDE Deduplication
   ↓
Forward Propagation
   ↓
Fixpoint
   ↓
Final Result
```

### Tasks

-   Connect all modules.
-   Define analysis state.
-   Implement propagation.
-   Implement merging at control-flow joins.
-   Implement convergence/fixpoint logic.
-   Support the selected heap operations.
-   Produce per-pointer/per-program-point results.

### Deliverable

The complete MDE-HRA implementation.

------------------------------------------------------------------------

# 16. Phase 9 --- Correctness Testing

The implementation must support at least three configurations:

``` text
A. HRA

B. Liveness + HRA

C. Liveness + HRA + MDE
```

The most important comparison for the MDE contribution is:

``` text
B vs C
```

because it isolates the effect of deduplication.

### Test cases

1.  Simple pointer
2.  Pointer copy
3.  Single field access
4.  Multi-field access path
5.  Repeated structures
6.  Branches
7.  Loops
8.  Dead pointers
9.  Control-flow join points
10. Combined linked-structure example

### Correctness criterion

For supported inputs:

``` text
Logical Result without MDE
              ==
Logical Result with MDE
```

Internal node IDs are not considered semantic results.

------------------------------------------------------------------------

# 17. Phase 10 --- Instrumentation

Measure:

## Runtime

Total analysis time.

## Peak memory

Maximum memory consumed during analysis.

## Nodes constructed

Number of structural nodes/representations requested.

## Nodes reused

Number of existing structures reused.

## Deduplication ratio

The proposal defines the intended form as:

``` text
(Nodes Constructed - Nodes Reused)
/
Nodes Constructed
```

This metric definition must be frozen before final experiments.

## Result equivalence

Record whether MDE and non-MDE results match.

------------------------------------------------------------------------

# 18. Phase 11 --- Benchmark Selection

The proposal targets approximately **5--8 open-source C programs** with
heap-heavy data structures.

### Selection criteria

Prefer programs containing:

-   dynamic allocation;
-   structures;
-   pointer fields;
-   linked structures;
-   repeated access paths;
-   enough complexity for measurable analysis state.

### Categories

-   small debugging benchmarks;
-   medium benchmarks;
-   larger benchmarks;
-   high-repetition synthetic/real benchmarks.

Unsupported language features should either be avoided or explicitly
documented.

------------------------------------------------------------------------

# 19. Phase 12 --- Experimental Configurations

For each benchmark:

### Experiment A

``` text
HRA
```

Purpose: baseline.

### Experiment B

``` text
Liveness + HRA
```

Purpose: measure liveness pruning.

### Experiment C

``` text
Liveness + HRA + MDE
```

Purpose: measure the proposed integration.

Comparison:

``` text
                    Runtime    Memory    Dedup    Result
HRA                    X          X        -        R
Liveness + HRA         X          X        -        R
Liveness + HRA + MDE   X          X        X        R
```

------------------------------------------------------------------------

# 20. Phase 13 --- Evaluation

For each benchmark collect:

``` text
Benchmark
Program size
Heap-related operations
Runtime
Peak memory
Nodes constructed
Nodes reused
Dedup ratio
Result equivalence
```

### Main comparisons

#### Liveness contribution

``` text
HRA
vs.
Liveness + HRA
```

#### MDE contribution

``` text
Liveness + HRA
vs.
Liveness + HRA + MDE
```

#### Overall improvement

``` text
HRA
vs.
Liveness + HRA + MDE
```

------------------------------------------------------------------------

# 21. Phase 14 --- Result Analysis

The results must answer:

### RQ1

Does liveness reduce unnecessary analysis information?

### RQ2

How much structural repetition remains after liveness pruning?

### RQ3

Does MDE reduce memory?

### RQ4

Does MDE reduce runtime?

### RQ5

Does MDE preserve the logical analysis result?

### RQ6

Does MDE provide additional benefit beyond liveness?

### RQ7

Which types of heap structures benefit most?

Do not assume numerical improvements before experiments.

------------------------------------------------------------------------

# 22. Repository Structure

``` text
mde-hra/
│
├── README.md
├── PROJECT.md
├── CMakeLists.txt
│
├── docs/
│   ├── architecture.md
│   ├── design.md
│   ├── assumptions.md
│   └── experiments.md
│
├── include/
│   ├── frontend/
│   ├── liveness/
│   ├── hra/
│   ├── mdehra/
│   ├── solver/
│   └── instrumentation/
│
├── src/
│   ├── frontend/
│   ├── liveness/
│   ├── hra/
│   ├── mdehra/
│   ├── solver/
│   └── instrumentation/
│
├── tests/
│   ├── liveness/
│   ├── hra/
│   ├── mde/
│   ├── integration/
│   └── regression/
│
├── benchmarks/
│   ├── basic/
│   ├── linked_list/
│   ├── repeated_paths/
│   └── real_programs/
│
├── scripts/
│   ├── build.sh
│   ├── run_tests.sh
│   └── run_benchmarks.sh
│
└── results/
    ├── raw/
    ├── processed/
    ├── tables/
    └── figures/
```

------------------------------------------------------------------------

# 23. Module Responsibilities

## Frontend

C source → IR → CFG/program representation.

## Liveness

CFG + USE/DEF → LIVE-IN/LIVE-OUT.

## HRA

Live pointer information → heap access paths.

## MDE-HRA

HRA structures → deduplicated structures.

## Solver

Propagation, merging, convergence and queries.

## Instrumentation

Runtime, memory, node counts, reuse counts, dedup ratio and correctness
comparison.

------------------------------------------------------------------------

# 24. Interface Design

The desired dependency direction is:

``` text
Frontend
   |
   | CFG
   v
Liveness
   |
   | LiveSet
   v
HRA
   |
   | HRA State
   v
MDE-HRA
   |
   | Deduplicated State
   v
Solver
   |
   v
Results
```

Avoid direct coupling between unrelated modules.

For example:

-   Liveness should not know MDE internals.
-   HRA should not depend on benchmark-specific code.
-   Instrumentation should observe the system rather than change its
    semantics.

------------------------------------------------------------------------

# 25. Baseline and Ablation Controls

The implementation should allow:

``` text
USE_LIVENESS = false
USE_MDE      = false
```

for baseline HRA,

``` text
USE_LIVENESS = true
USE_MDE      = false
```

for liveness-pruned HRA,

and:

``` text
USE_LIVENESS = true
USE_MDE      = true
```

for the proposed method.

This creates a clean ablation study.

------------------------------------------------------------------------

# 26. Testing Strategy

## Unit tests

-   live-set operations;
-   access-path construction;
-   field extension;
-   HRA node operations;
-   MDE node construction;
-   structural reuse.

## Component tests

-   Liveness alone;
-   HRA alone;
-   MDE representation alone.

## Integration tests

-   Liveness + HRA;
-   HRA + MDE;
-   Liveness + HRA + MDE.

## Benchmark tests

Complete pipeline on selected programs.

------------------------------------------------------------------------

# 27. Debugging Order

When a result is incorrect, debug in this order:

``` text
Source
  ↓
IR
  ↓
CFG
  ↓
Liveness
  ↓
HRA
  ↓
Liveness + HRA
  ↓
MDE representation
  ↓
MDE result
  ↓
Instrumentation
```

Do not debug the complete system as one large unit.

------------------------------------------------------------------------

# 28. Risk Management

## Risk: MDE is difficult to integrate

Fallback:

-   design a clean adapter;
-   if necessary, use a lightweight custom hash-consing implementation
    based on the relevant published design.

## Risk: Existing liveness implementation cannot be reused

Fallback:

-   implement a minimal self-contained liveness-pruned solver for the
    restricted scope.

## Risk: Full HRA is too complex

Mitigation:

-   enforce singly-linked/limited-field scope.

## Risk: Little repeated structure exists

Mitigation:

-   include high-repetition synthetic tests in addition to real
    benchmarks.

## Risk: MDE changes the result

Mitigation:

-   stop performance evaluation until equivalence tests pass.

## Risk: Benchmarks contain unsupported constructs

Mitigation:

-   select another benchmark, restrict the test case, or document the
    limitation.

------------------------------------------------------------------------

# 29. Four-Person Work Division

## Track A --- Liveness

-   CFG-level liveness
-   live-set representation
-   liveness tests

## Track B --- HRA

-   access-path representation
-   heap modeling
-   HRA operations
-   HRA tests

## Track C --- MDE

-   MDE/LatticeHashForest study
-   HRA-MDE adapter
-   structural sharing
-   deduplication statistics

## Track D --- Infrastructure/Evaluation

-   frontend/IR
-   build system
-   benchmark framework
-   measurement and result processing

All four members should participate in final integration and result
validation.

------------------------------------------------------------------------

# 30. Milestones

### M1 --- Setup

Build environment, repository and scope ready.

### M2 --- Frontend

Supported C programs can be represented for analysis.

### M3 --- Baseline HRA

Basic heap access paths work.

### M4 --- Liveness

Backward liveness reaches fixed point and passes tests.

### M5 --- Liveness + HRA

Dead pointer information is pruned correctly.

### M6 --- MDE Adapter

HRA structures can be represented and shared through MDE.

### M7 --- Full MDE-HRA

End-to-end analysis works.

### M8 --- Correctness

MDE ON/OFF results match.

### M9 --- Benchmarking

Benchmark runner and instrumentation work.

### M10 --- Evaluation

All experiments, tables and figures are complete.

------------------------------------------------------------------------

# 31. Definition of Done

## Functional

-   [ ] C input supported within defined scope
-   [ ] CFG/program representation works
-   [ ] Liveness works
-   [ ] HRA works
-   [ ] MDE integration works
-   [ ] Propagation works
-   [ ] Final queries work

## Correctness

-   [ ] Unit tests pass
-   [ ] Integration tests pass
-   [ ] Branch tests pass
-   [ ] Loop tests pass
-   [ ] Dead-pointer tests pass
-   [ ] MDE ON/OFF results match

## Performance

-   [ ] Runtime measured
-   [ ] Peak memory measured
-   [ ] Nodes constructed measured
-   [ ] Nodes reused measured
-   [ ] Dedup ratio measured

## Evaluation

-   [ ] 5--8 benchmarks selected
-   [ ] HRA baseline evaluated
-   [ ] Liveness + HRA evaluated
-   [ ] MDE-HRA evaluated
-   [ ] Results reproducible
-   [ ] Tables generated
-   [ ] Graphs generated

## Research

-   [ ] Main research question answered
-   [ ] Liveness contribution isolated
-   [ ] MDE contribution isolated
-   [ ] Correctness discussed
-   [ ] Limitations documented
-   [ ] Threats to validity documented
-   [ ] Novelty claim supported by implementation/evidence

------------------------------------------------------------------------

# 32. Development Sequence

``` text
1. Freeze scope
       ↓
2. Set up repository
       ↓
3. Build frontend/IR
       ↓
4. Build baseline representation
       ↓
5. Implement HRA
       ↓
6. Test HRA
       ↓
7. Implement liveness
       ↓
8. Integrate liveness + HRA
       ↓
9. Test liveness-pruned HRA
       ↓
10. Study MDE
       ↓
11. Build HRA → MDE adapter
       ↓
12. Test deduplication
       ↓
13. Integrate full pipeline
       ↓
14. Validate result equivalence
       ↓
15. Add instrumentation
       ↓
16. Select benchmarks
       ↓
17. Run experiments
       ↓
18. Analyze results
       ↓
19. Document limitations
       ↓
20. Final report
```

------------------------------------------------------------------------

# 33. Expected Research Narrative

### Problem

Heap-aware pointer analysis can become expensive because it processes
detailed heap information and may create repeated structures.

### Improvement 1 --- Liveness

Remove information that is no longer relevant.

``` text
Less unnecessary analysis
```

### Remaining problem

Useful information can still be structurally repetitive.

``` text
p -> next -> data
q -> next -> data
r -> next -> data
```

### Improvement 2 --- MDE

Share repeated structures.

``` text
        next -> data
       /      |            p       q       r
```

### Experimental question

Determine whether:

``` text
Liveness + HRA + MDE
```

is more scalable than:

``` text
Liveness + HRA
```

while preserving the same logical result.

------------------------------------------------------------------------

# 34. What Must Not Be Assumed

The project must not claim a specific speedup or memory reduction before
experiments.

Previous MDE results are motivation and prior work. They are not
guaranteed results for this HRA integration.

All final numerical claims must come from the implemented system and
benchmark experiments.

------------------------------------------------------------------------

# 35. Immediate Next Steps

Before substantial coding:

1.  Freeze supported scope.
2.  Freeze the three experimental configurations.
3.  Select compiler/IR infrastructure.
4.  Identify exact MDE implementation to use.
5.  Identify HRA implementation/reference to study.
6.  Identify liveness implementation/reference to study.
7.  Create repository.
8.  Define module interfaces.
9.  Define correctness comparison.
10. Define benchmark methodology.
11. Start with baseline HRA.

------------------------------------------------------------------------

# 36. Final Implementation Goal

> **Build a scoped, liveness-pruned Heap Reference Analysis for C
> programs, represent repeated heap access-path structures through
> MDE-based structural sharing, and experimentally determine whether the
> integration improves memory and runtime without changing the analysis
> result.**

------------------------------------------------------------------------

## Project References

The implementation plan is based on the project proposal, technical
synopsis and the following principal works identified in the project:

1.  A. Ghorui, A. Raste, U. P. Khedker --- *Points-to Analysis Using
    MDE: A Multi-level Deduplication Engine for Repetitive Data and
    Operations*, 2026.
2.  A. Ghorui, U. P. Khedker --- *LatticeHashForest: An Efficient Data
    Structure for Repetitive Data and Operations*, 2025.
3.  U. P. Khedker, A. Sanyal, A. Karkare --- *Heap Reference Analysis
    Using Access Graphs*, ACM TOPLAS, 2007.
4.  V. Kanvar, U. P. Khedker --- *Which Part of the Heap is Useful?
    Improving Heap Liveness Analysis*, 2024.
5.  U. P. Khedker, A. Mycroft, P. S. Rawat --- *Liveness-Based Pointer
    Analysis*, SAS 2012.
6.  M. Barbar, Y. Sui --- *Hash Consed Points-To Sets*, SAS 2021.
7.  M. Barbar, Y. Sui, S. Chen --- *Object Versioning for Flow-Sensitive
    Pointer Analysis*, CGO 2021.
8.  J. Zhang, X. Cheng, Y. Lei --- *Flow Sensitivity Without Control
    Flow Graph*, 2026.
9.  U. P. Khedker, V. Kanvar --- *Generalizing the Liveness Based
    Points-to Analysis*, 2014.
10. P. Gharat, U. P. Khedker, A. Mycroft --- *Generalized Points-to
    Graphs*, 2020.
11. S. Jaiswal, U. P. Khedker, S. Chakraborty --- *Bidirectionality in
    Flow-Sensitive Demand-Driven Analysis*, 2020.
12. Y. Sui, Y. Li, J. Xue --- *Query-Directed Adaptive Heap Cloning for
    Optimizing Compilers*, CGO 2013.
13. T. Chen, J. Lin, W.-C. Hsu, P.-C. Yew --- *On the Impact of Naming
    Methods for Heap-Oriented Pointers in C Programs*, 2002.
