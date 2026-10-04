# MDE-HRA Architecture

> **Detailed system architecture for the MDE-HRA project**

---

## 1. Purpose

This document defines the **system-level architecture** of the MDE-HRA implementation.

It explains:

- the major components;
- the responsibility of each component;
- how information flows between components;
- which components are independent;
- where liveness is integrated;
- where HRA is integrated;
- where MDE is integrated;
- how the three experimental configurations are constructed;
- how correctness is preserved;
- how instrumentation is separated from analysis semantics.

This document describes **architecture**, not the detailed implementation of every algorithm.

Detailed data structures and algorithms belong in:

```text
docs/design.md
```

Detailed phase instructions belong in:

```text
phase-*.md
```

---

# 2. Architectural Goal

The system must support a scalable heap-reference analysis pipeline in which:

1. the input C program is represented in an analyzable form;
2. control flow is constructed;
3. liveness is computed backward;
4. HRA analyzes relevant heap references;
5. MDE provides structural deduplication;
6. the analysis reaches a stable result;
7. the result can be compared with a non-MDE implementation;
8. performance and memory effects can be measured.

The architecture must make it possible to answer:

> Does MDE reduce the cost of representing and processing repeated heap-reference structures without changing the logical result?

---

# 3. High-Level Architecture

The complete conceptual architecture is:

```text
                         C Program
                            |
                            v
                  +----------------------+
                  | Frontend / Parser    |
                  +----------------------+
                            |
                            v
                  +----------------------+
                  | Program / IR Model   |
                  +----------------------+
                            |
                            v
                  +----------------------+
                  | CFG Construction     |
                  +----------------------+
                            |
              +-------------+-------------+
              |                           |
              v                           v
     +------------------+       +------------------+
     | Backward         |       | Program Facts   |
     | Liveness         |       | USE / DEF etc.  |
     +------------------+       +------------------+
              |
              v
     +------------------+
     | Live Pointer     |
     | Information      |
     +------------------+
              |
              v
     +------------------+
     | Heap Reference   |
     | Analysis (HRA)   |
     +------------------+
              |
              v
     +------------------+
     | HRA Representation|
     +------------------+
              |
              v
     +------------------+
     | MDE Adapter      |
     +------------------+
              |
              v
     +------------------+
     | MDE / LHF        |
     | Deduplicated     |
     | Representation   |
     +------------------+
              |
              v
     +------------------+
     | Forward           |
     | Propagation /     |
     | Fixpoint          |
     +------------------+
              |
              v
     +------------------+
     | Logical Analysis |
     | Result            |
     +------------------+
              |
       +------+------+
       |             |
       v             v
+-------------+ +----------------+
| Correctness | | Instrumentation|
| Comparison  | | / Metrics      |
+-------------+ +----------------+
```

---

# 4. Core Architectural Principle

The most important architectural rule is:

> **Semantic analysis and representation optimization must remain separable.**

Conceptually:

```text
                SEMANTIC LAYER
                      |
       +--------------+--------------+
       |                             |
   Liveness                         HRA
       |                             |
       +--------------+--------------+
                      |
                Logical State
                      |
                      v
              REPRESENTATION
                   LAYER
                      |
                     MDE
                      |
                      v
             Optimized Storage
```

MDE should improve how repeated information is represented or shared.

It must not redefine what that information means.

---

# 5. Major Architectural Layers

The system is divided into the following layers:

```text
Layer 1  — Input / Frontend
Layer 2  — Program Representation
Layer 3  — Control Flow
Layer 4  — Static Liveness
Layer 5  — Heap Reference Analysis
Layer 6  — HRA Representation
Layer 7  — MDE Representation
Layer 8  — Propagation / Fixpoint
Layer 9  — Result Normalization
Layer 10 — Instrumentation / Evaluation
```

Each layer has a specific responsibility.

---

# 6. Layer 1 — Input / Frontend

## Responsibility

Accept the supported C program and produce the program representation required by later analysis stages.

Conceptually:

```text
C source
   ↓
Frontend
   ↓
Analyzable program representation
```

The frontend is not responsible for:

- liveness;
- HRA;
- MDE;
- experimental comparison.

---

## Architectural Boundary

The frontend should expose program information through a stable interface.

Later components should not need to know how the C source was parsed internally.

---

# 7. Layer 2 — Program Representation

This layer stores the program information needed by analysis.

Possible information includes:

```text
Functions
Basic blocks
Statements / instructions
Variables
Pointer operations
Memory operations
Allocations
Field accesses
Uses
Definitions
```

The exact IR depends on the selected implementation environment.

The architecture does not require a particular IR representation at this level.

---

# 8. Layer 3 — Control-Flow Graph

The CFG provides the structure required by static analyses.

Conceptually:

```text
        Entry
          |
          v
         B1
        /  \
       v    v
      B2    B3
       \    /
        v  v
         B4
          |
          v
         Exit
```

Each node represents a program point or basic block.

Edges represent possible control-flow transitions.

---

## 8.1 CFG Responsibilities

The CFG must support:

- successors;
- predecessors;
- entry;
- exit;
- traversal;
- branch structure;
- loop structure;
- program-point identification.

---

## 8.2 Why CFG Comes Before Liveness

Liveness is backward dataflow analysis.

Therefore:

```text
Liveness
    requires
CFG
```

The CFG provides the successor relationships needed to compute:

```text
LIVE-OUT
```

from successor information.

---

# 9. Layer 4 — Backward Liveness

Liveness is a separate static-analysis component.

Its conceptual input is:

```text
CFG
+
USE
+
DEF
```

and its output is:

```text
LIVE-IN
LIVE-OUT
```

for relevant program points.

---

## 9.1 Direction

Liveness is computed backward:

```text
Exit
  ↑
  |
Block
  ↑
  |
Entry
```

This is fundamentally different from the forward propagation used by HRA.

---

## 9.2 Conceptual Equations

The general form is:

```text
LIVE-IN  = USE ∪ (LIVE-OUT - DEF)

LIVE-OUT = union of LIVE-IN of successors
```

The exact pointer-specific formulation used by the project must be documented in the detailed design.

---

## 9.3 Liveness Output Boundary

Liveness should expose a logical interface such as:

```text
getLiveIn(programPoint)
getLiveOut(programPoint)
```

The HRA component should consume the resulting live information rather than accessing liveness internals.

---

# 10. Layer 5 — Heap Reference Analysis

HRA is the main semantic analysis component concerned with heap references.

Its role is to determine relevant heap-reference/access-path information according to the project's defined scope.

Example access paths:

```text
p
p.next
p.next.next
p.next.data
```

---

## 10.1 HRA Inputs

HRA conceptually receives:

```text
Program representation
CFG
Pointer operations
Heap operations
Field accesses
Optional live-pointer information
```

---

## 10.2 HRA Outputs

HRA produces a representation of heap references such as:

```text
Root pointer
     |
    next
     |
    next
     |
    data
```

which corresponds conceptually to:

```text
p.next.next.data
```

---

# 11. HRA Independence

HRA must first work without MDE.

This creates:

```text
Baseline HRA
```

which serves as:

- a correctness reference;
- a debugging reference;
- an experimental baseline.

The architecture therefore intentionally supports:

```text
HRA → Result
```

without:

```text
HRA → MDE
```

---

# 12. Layer 6 — HRA Representation

The semantic HRA state should be separated from the physical representation used to store it.

Conceptually:

```text
HRA semantic state
        |
        v
HRA representation
```

The representation may contain:

- nodes;
- edges;
- access-path labels;
- root information;
- field information;
- sharing information.

The exact structures are specified in `design.md`.

---

# 13. Layer 7 — MDE Representation

MDE is introduced at the representation layer.

Conceptually:

```text
HRA structure
      |
      v
MDE Adapter
      |
      v
MDE / LatticeHashForest
```

The MDE layer is responsible for detecting/reusing repeated structures according to the selected MDE implementation.

---

# 14. Why an Adapter Is Needed

The HRA representation and MDE representation should not be tightly coupled.

Instead:

```text
HRA
 |
 v
HRA Representation
 |
 v
MDE Adapter
 |
 v
MDE Representation
```

The adapter translates the HRA representation into the representation expected by MDE.

This gives three benefits:

### Benefit 1 — Isolation

MDE can be tested separately.

### Benefit 2 — Replaceability

A different deduplication representation can potentially be tested without rewriting HRA.

### Benefit 3 — Experimental comparison

The same HRA semantics can be executed with:

```text
MDE OFF
```

and:

```text
MDE ON
```

---

# 15. MDE Must Not Become the Semantic Owner

The following architecture should be avoided:

```text
MDE
 |
 +-- decides pointer semantics
 +-- decides liveness
 +-- decides HRA meaning
 +-- decides query semantics
```

Instead:

```text
Liveness → semantic liveness
HRA      → heap-reference semantics
MDE      → structural representation/sharing
Solver   → propagation/fixpoint
```

Each component owns a clearly defined responsibility.

---

# 16. Layer 8 — Forward Propagation / Fixpoint

The analysis requires propagation until the state stabilizes according to the selected HRA formulation.

Conceptually:

```text
Initial State
     |
     v
Transfer
     |
     v
Merge
     |
     v
New State
     |
     +------ changes? ------+
     |                      |
    yes                    no
     |                      |
     +----> repeat          v
                         Fixpoint
```

---

## 16.1 Fixpoint Responsibility

The solver/fixpoint layer manages:

- worklist or equivalent iteration;
- propagation;
- merging;
- convergence;
- termination.

It should not contain benchmark-specific logic.

---

# 17. Forward vs Backward Analyses

The architecture deliberately contains two different directions:

```text
Liveness:
Backward
```

and:

```text
HRA propagation:
Forward / fixpoint-oriented
```

These must not be conceptually mixed.

The combined pipeline is:

```text
CFG
 |
 +----> Backward Liveness
 |             |
 |             v
 |        Live information
 |             |
 +-------------+
               |
               v
              HRA
               |
               v
          Forward propagation
```

---

# 18. Layer 9 — Result Normalization

Different configurations may use different internal representations.

Therefore, final results must be converted to a common logical form.

Conceptually:

```text
Baseline HRA result
        |
        v
Normalization
        |
        v
Canonical Result
```

and:

```text
MDE-HRA result
        |
        v
Normalization
        |
        v
Canonical Result
```

Then:

```text
Canonical Result A
        ==
Canonical Result C
```

can be checked.

---

# 19. Representation Independence

The correctness comparison must not depend on:

```text
object addresses
hash-table locations
internal node IDs
allocation order
MDE-specific IDs
```

For example:

```text
HRA:

Node 1 --next--> Node 2
```

and:

```text
MDE:

Node 17 --next--> Node 42
```

can represent the same logical result.

The comparison should operate on their semantic meaning.

---

# 20. Layer 10 — Instrumentation

Instrumentation is an observation layer.

It should collect information such as:

```text
runtime
peak memory
nodes constructed
nodes reused
deduplication ratio
result equivalence
```

Instrumentation should be logically separated from the semantic analysis.

---

# 21. Instrumentation Architecture

Prefer:

```text
                Analysis
                   |
          +--------+--------+
          |                 |
          v                 v
      Logical Result    Metrics Events
                            |
                            v
                       Collector
                            |
                            v
                       Result File
```

rather than embedding experiment-specific output throughout the semantic code.

---

# 22. Complete Data Flow

The complete flow is:

```text
+------------------+
| C Program        |
+------------------+
         |
         v
+------------------+
| Frontend         |
+------------------+
         |
         v
+------------------+
| Program Model    |
+------------------+
         |
         v
+------------------+
| CFG              |
+------------------+
         |
         +----------------------+
         |                      |
         v                      v
+------------------+   +------------------+
| USE / DEF        |   | Pointer / Heap   |
| Information      |   | Operations       |
+------------------+   +------------------+
         |                      |
         v                      |
+------------------+            |
| Liveness         |            |
| Analysis         |            |
+------------------+            |
         |                      |
         v                      |
+------------------+            |
| Live Pointer     |            |
| Information      |            |
+------------------+            |
         |                      |
         +----------+-----------+
                    |
                    v
          +------------------+
          | HRA              |
          +------------------+
                    |
                    v
          +------------------+
          | HRA Representation|
          +------------------+
                    |
                    v
          +------------------+
          | MDE Adapter      |
          +------------------+
                    |
                    v
          +------------------+
          | MDE / LHF        |
          +------------------+
                    |
                    v
          +------------------+
          | Fixpoint Solver  |
          +------------------+
                    |
                    v
          +------------------+
          | Logical Result   |
          +------------------+
             |            |
             v            v
       +----------+   +-------------+
       | Normalize|   | Instrument  |
       +----------+   +-------------+
             |
             v
       +-------------+
       | Comparison  |
       +-------------+
```

---

# 23. Configuration A Architecture — HRA

Configuration A disables liveness pruning and MDE.

```text
C
 ↓
Frontend
 ↓
IR / CFG
 ↓
HRA
 ↓
HRA Representation
 ↓
Solver
 ↓
Result
```

This is the baseline.

---

## Purpose

It answers:

> What does the baseline HRA cost and produce?

It also provides a debugging reference for later configurations.

---

# 24. Configuration B Architecture — Liveness + HRA

Configuration B adds backward liveness.

```text
C
 ↓
Frontend
 ↓
IR / CFG
 ↓
Liveness
 ↓
Live Pointer Information
 ↓
HRA
 ↓
HRA Representation
 ↓
Solver
 ↓
Result
```

---

## Purpose

It answers:

> How much does liveness pruning change the analysis?

---

# 25. Configuration C Architecture — Liveness + HRA + MDE

Configuration C adds structural deduplication.

```text
C
 ↓
Frontend
 ↓
IR / CFG
 ↓
Liveness
 ↓
Live Pointer Information
 ↓
HRA
 ↓
HRA Representation
 ↓
MDE Adapter
 ↓
MDE / LHF
 ↓
Solver
 ↓
Result
```

---

## Purpose

It answers:

> Does MDE improve the liveness-pruned HRA representation and scalability?

---

# 26. Configuration Comparison

| Component | A: HRA | B: Liveness + HRA | C: Liveness + HRA + MDE |
|---|---|---|---|
| Frontend | Yes | Yes | Yes |
| IR | Yes | Yes | Yes |
| CFG | Yes | Yes | Yes |
| Liveness | No | Yes | Yes |
| HRA | Yes | Yes | Yes |
| HRA Representation | Yes | Yes | Yes |
| MDE | No | No | Yes |
| Fixpoint | Yes | Yes | Yes |
| Result normalization | Yes | Yes | Yes |
| Instrumentation | Yes | Yes | Yes |

---

# 27. Experimental Isolation Architecture

The architecture intentionally keeps the following paths separate:

```text
A
HRA
```

```text
B
Liveness → HRA
```

```text
C
Liveness → HRA → MDE
```

This allows:

```text
A vs B
```

to estimate the effect of liveness, and:

```text
B vs C
```

to estimate the additional effect of MDE.

---

# 28. Dependency Graph

The conceptual dependency graph is:

```text
Frontend
   |
   v
Program Model
   |
   v
CFG
   |
   +----------+
   |          |
   v          v
Liveness    HRA Facts
   |          |
   +----+-----+
        |
        v
       HRA
        |
        v
 HRA Representation
        |
        v
 MDE Adapter
        |
        v
 MDE Representation
        |
        v
     Solver
        |
        v
     Result
```

The key property is that:

```text
MDE does not become a prerequisite for HRA.
```

---

# 29. Failure Isolation

The architecture should make failures localizable.

Examples:

## Incorrect liveness

Inspect:

```text
CFG
USE/DEF
LIVE-IN
LIVE-OUT
```

without involving MDE.

---

## Incorrect HRA

Disable MDE and inspect:

```text
HRA representation
```

---

## Incorrect MDE result

Compare:

```text
HRA result before MDE
```

with:

```text
normalized MDE result
```

---

## Performance problem

Compare:

```text
A
B
C
```

to determine which layer introduces the cost.

---

# 30. State Ownership

Each state category should have a clear owner.

| State | Owner |
|---|---|
| Source program | Frontend/Input |
| IR | Program Model |
| CFG | CFG module |
| USE/DEF | Program analysis facts |
| LIVE-IN/OUT | Liveness |
| HRA semantic state | HRA |
| HRA representation | HRA representation layer |
| Deduplicated representation | MDE |
| Fixpoint/worklist state | Solver |
| Canonical result | Result normalization |
| Metrics | Instrumentation |

No component should silently mutate another component's internal state.

---

# 31. Semantic vs Representation State

This distinction is central.

### Semantic state

Answers:

> What does the analysis know?

Examples:

```text
p may refer to object X
p.next may reach object Y
```

### Representation state

Answers:

> How is that information stored?

Examples:

```text
Node 42
Hash key
Shared suffix
MDE node
```

The architecture allows representation state to change while semantic state remains equivalent.

---

# 32. Ownership of Semantic Decisions

Semantic decisions belong to the analysis.

Examples:

```text
Is this pointer live?
Does this access path exist?
What heap reference is represented?
When should states be merged?
```

These decisions must not be hidden inside an optimization mechanism.

MDE may answer:

```text
Can an equivalent structure be reused?
```

but should not independently decide:

```text
What does this pointer mean?
```

---

# 33. MDE Integration Boundary

The intended integration boundary is:

```text
               HRA
                |
                v
       Logical / Structural
             State
                |
                v
          MDE Adapter
                |
                v
      Deduplicated Representation
```

This boundary should be explicit in the implementation.

---

# 34. Optional MDE Path

The architecture must support:

```text
HRA → Solver
```

and:

```text
HRA → MDE → Solver
```

without requiring two separate HRA implementations.

This is important for controlled experimentation.

---

# 35. Configuration Selection

The runtime/configuration layer should conceptually expose:

```text
HRA_ONLY
LIVENESS_HRA
LIVENESS_HRA_MDE
```

The exact CLI/API will be defined later.

Configuration selection should not require source-code modifications.

---

# 36. Common Infrastructure

The following infrastructure should be shared across configurations:

```text
Frontend
IR
CFG
Input loading
Benchmark loading
Result normalization
Experiment metadata
Instrumentation framework
```

This reduces experimental bias caused by different execution pipelines.

---

# 37. What Must Not Be Duplicated

Avoid implementing:

```text
HRA_A
HRA_B
HRA_C
```

for the three configurations.

Instead use:

```text
One HRA implementation
        |
        +---- MDE disabled
        |
        +---- MDE enabled
```

and:

```text
Liveness disabled
```

or:

```text
Liveness enabled
```

through configuration.

---

# 38. Architectural Invariants

The implementation must preserve the following invariants.

## Invariant 1 — Configuration independence

Disabling MDE must not require a different HRA algorithm.

---

## Invariant 2 — Liveness independence

Liveness should be independently executable and inspectable.

---

## Invariant 3 — Semantic equivalence

For supported programs:

```text
B.logicalResult == C.logicalResult
```

---

## Invariant 4 — Result normalization

Representation-specific identifiers must not affect semantic comparison.

---

## Invariant 5 — Instrumentation neutrality

Instrumentation must not intentionally alter analysis semantics.

---

## Invariant 6 — Scope control

Unsupported program features must not silently be treated as supported.

---

# 39. Error Handling Architecture

Errors should identify the layer where they occur.

Examples:

```text
FrontendError
CFGError
LivenessError
HRAError
MDEError
SolverError
ResultNormalizationError
BenchmarkError
```

The exact exception/error mechanism depends on the implementation language.

---

# 40. Unsupported Feature Handling

If the input contains an unsupported construct:

```text
detect
  ↓
classify
  ↓
report
  ↓
skip / reject according to configured policy
```

Do not silently produce a potentially incorrect result.

The benchmark framework should record unsupported cases explicitly.

---

# 41. Determinism

Where practical, the analysis should produce deterministic logical results.

Internal representation ordering may differ, but normalized results should be stable.

This is especially important for:

- regression tests;
- result comparison;
- benchmark experiments.

---

# 42. Performance Architecture

Performance measurement should distinguish:

```text
Frontend cost
CFG construction
Liveness
HRA
MDE
Solver
Result normalization
```

Where feasible, timing should be available for major stages.

This helps answer whether a performance change comes from:

```text
liveness
MDE
solver
representation
```

rather than only observing total runtime.

---

# 43. Memory Architecture

Memory measurement should eventually distinguish:

```text
baseline memory
HRA representation memory
MDE representation memory
auxiliary analysis memory
```

The exact measurement technique will be defined in the experimentation documentation.

---

# 44. Architectural Risks

## Risk 1 — Tight coupling

If HRA directly depends on MDE internals, experimental isolation becomes difficult.

### Mitigation

Use an adapter/interface boundary.

---

## Risk 2 — Semantic corruption

If deduplication changes graph meaning, results may change.

### Mitigation

Use normalized-result equivalence tests.

---

## Risk 3 — Liveness bugs hidden by HRA

Incorrect liveness may appear as an HRA issue.

### Mitigation

Test liveness independently.

---

## Risk 4 — Instrumentation changes behavior

Heavy instrumentation can distort runtime.

### Mitigation

Separate instrumentation and document measurement methodology.

---

## Risk 5 — Benchmark-specific behavior

A benchmark may accidentally receive special handling.

### Mitigation

Keep benchmark-independent analysis logic.

---

# 45. Development Architecture

Implementation should follow:

```text
Architecture
    ↓
Interfaces
    ↓
Component
    ↓
Component Tests
    ↓
Integration
    ↓
Integration Tests
    ↓
Instrumentation
    ↓
Benchmark
```

Do not jump directly from architecture to large benchmarks.

---

# 46. Suggested Repository-Level Architecture

A conceptual repository layout is:

```text
project-root/
│
├── project.md
├── skill.md
│
├── docs/
│   ├── architecture.md
│   ├── design.md
│   ├── testing.md
│   ├── benchmarks.md
│   ├── experiments.md
│   ├── development.md
│   └── decisions.md
│
├── phases/
│   ├── phase-0-setup.md
│   ├── phase-1-frontend.md
│   ├── phase-2-baseline.md
│   ├── phase-3-hra.md
│   ├── phase-4-liveness.md
│   ├── phase-5-integration.md
│   ├── phase-6-mde-study.md
│   ├── phase-7-mde-hra.md
│   ├── phase-8-validation.md
│   ├── phase-9-instrumentation.md
│   ├── phase-10-benchmarks.md
│   └── phase-11-experiments.md
│
├── src/
├── include/
├── tests/
├── benchmarks/
├── scripts/
└── results/
```

The exact directory structure may change once the implementation environment is finalized.

---

# 47. Architecture Evolution Rule

This architecture is the starting design.

If implementation evidence shows that a component boundary is inadequate:

1. document the problem;
2. propose the change;
3. evaluate impact;
4. update `architecture.md`;
5. update affected design/phase documents;
6. record the decision;
7. then modify implementation.

Do not let architecture drift silently.

---

# 48. Final Architecture

The intended final conceptual system is:

```text
                         +----------------+
                         |    C Program   |
                         +----------------+
                                  |
                                  v
                         +----------------+
                         |    Frontend    |
                         +----------------+
                                  |
                                  v
                         +----------------+
                         | Program Model  |
                         +----------------+
                                  |
                                  v
                         +----------------+
                         |      CFG       |
                         +----------------+
                                  |
                    +-------------+-------------+
                    |                           |
                    v                           v
             +-------------+             +-------------+
             |  Liveness   |             | Program     |
             |  Analysis   |             | Facts       |
             +-------------+             +-------------+
                    |                           |
                    v                           |
             +-------------+                    |
             | Live Pointer|                    |
             | Information |                    |
             +-------------+                    |
                    |                           |
                    +-------------+-------------+
                                  |
                                  v
                         +----------------+
                         |      HRA       |
                         +----------------+
                                  |
                                  v
                         +----------------+
                         | HRA State /    |
                         | Representation |
                         +----------------+
                                  |
                         MDE ENABLED?
                           /        \
                         NO          YES
                         |            |
                         |            v
                         |     +-------------+
                         |     | MDE Adapter |
                         |     +-------------+
                         |            |
                         |            v
                         |     +-------------+
                         |     | MDE / LHF   |
                         |     +-------------+
                         |            |
                         +------+-----+
                                |
                                v
                       +------------------+
                       | Fixpoint Solver  |
                       +------------------+
                                |
                                v
                       +------------------+
                       | Logical Result   |
                       +------------------+
                           |          |
                           v          v
                    +----------+ +-----------+
                    | Normalize| | Metrics   |
                    +----------+ +-----------+
                           |
                           v
                    +-------------+
                    | Comparison  |
                    +-------------+
```

---

# 49. Architectural Summary

The architecture can be reduced to seven essential ideas:

```text
1. Build a stable program/CFG representation.

2. Compute liveness independently and backward.

3. Feed live information into HRA.

4. Keep HRA semantics independent of MDE.

5. Insert MDE at the representation/deduplication boundary.

6. Compare normalized logical results between MDE and non-MDE configurations.

7. Measure performance and memory only after correctness is established.
```

The central architectural relationship is:

```text
             Liveness
                 |
                 v
              HRA
                 |
          +------+------+
          |             |
        No MDE        MDE
          |             |
          v             v
      HRA State     Deduplicated
                       State
          |             |
          +------+------+
                 |
                 v
              Solver
                 |
                 v
          Logical Result
```

This structure provides the foundation for the detailed technical design in `design.md` and the subsequent phase-by-phase implementation documents.
