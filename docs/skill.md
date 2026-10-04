# MDE-HRA Implementation Skill

> **Master implementation instructions for the MDE-HRA project**

---

## 0. Document Purpose

This document defines **how the MDE-HRA project must be implemented**.

It is not a phase-specific implementation guide and it is not a replacement for the project roadmap.

Its purpose is to establish:

- the engineering rules;
- the research rules;
- the implementation boundaries;
- the module boundaries;
- the development workflow;
- the correctness requirements;
- the testing discipline;
- the documentation discipline;
- the behavior expected from a developer or AI coding agent.

The detailed implementation of each phase will be documented separately in phase-specific Markdown files.

---

# 1. Project Identity

## 1.1 Project Name

**MDE-HRA**

Expanded concept:

> **Deduplication-Accelerated Heap Reference Analysis for Scalable, Liveness-Pruned Pointer Analysis**

---

## 1.2 Central Idea

The project combines three major ideas:

```text
Liveness
    +
Heap Reference Analysis (HRA)
    +
Multi-level Deduplication Engine (MDE)
```

The intended analysis pipeline is:

```text
C Program
    ↓
Program Representation / IR
    ↓
CFG
    ↓
Backward Liveness
    ↓
Live Pointer Information
    ↓
Heap Reference Analysis
    ↓
Access-Path / Heap Representation
    ↓
MDE Structural Deduplication
    ↓
Forward Propagation / Fixpoint
    ↓
Analysis Result
```

The implementation must preserve the distinction between:

1. **what the analysis means**, and
2. **how the analysis information is represented and stored**.

MDE is primarily intended to improve representation and repeated structural processing.

---

# 2. Research Question

The implementation must remain aligned with the project's central research question:

> **Can structural deduplication make liveness-pruned heap-reference analysis more scalable while preserving the analysis result?**

The implementation should therefore make it possible to separately evaluate:

```text
HRA
```

versus:

```text
Liveness + HRA
```

versus:

```text
Liveness + HRA + MDE
```

This separation is mandatory because otherwise the contribution of liveness and the contribution of MDE cannot be isolated.

---

# 3. Research Objectives

The implementation should enable the project to investigate:

### Objective 1 — Liveness

Determine whether liveness pruning reduces the amount of pointer/heap information that needs to be processed.

### Objective 2 — HRA

Represent heap references using access-path/access-graph concepts appropriate for the defined project scope.

### Objective 3 — MDE

Determine whether repeated HRA structures can be represented using MDE-based structural sharing/deduplication.

### Objective 4 — Correctness

Determine whether MDE changes the logical analysis result.

The desired condition is:

```text
Result(Liveness + HRA)
        ==
Result(Liveness + HRA + MDE)
```

for supported programs.

### Objective 5 — Performance

Measure:

- runtime;
- peak memory;
- structural nodes/representations constructed;
- structures reused;
- deduplication ratio.

---

# 4. Source-of-Truth Hierarchy

The project will contain multiple documentation files.

Use them in this order:

```text
project.md
    ↓
skill.md
    ↓
architecture.md
    ↓
design.md
    ↓
testing.md
    ↓
phase-specific MD
    ↓
implementation
```

More specifically:

## `project.md`

Defines:

- overall project;
- scope;
- roadmap;
- milestones;
- research objective;
- overall evaluation plan.

## `skill.md`

Defines:

- implementation rules;
- developer/agent behavior;
- engineering constraints;
- correctness rules;
- documentation rules.

## `architecture.md`

Defines:

- system-level components;
- information flow;
- module boundaries;
- dependencies.

## `design.md`

Defines:

- detailed data structures;
- interfaces;
- algorithms;
- state representation;
- integration design.

## `testing.md`

Defines:

- test levels;
- correctness criteria;
- test cases;
- regression strategy.

## Phase MDs

Define:

- exact work for one phase;
- prerequisites;
- inputs;
- outputs;
- implementation tasks;
- tests;
- completion criteria.

If a phase document conflicts with a higher-level document, **do not silently choose one**. Record the conflict and resolve it before continuing.

---

# 5. Implementation Philosophy

The project must be developed incrementally.

Do **not** begin by implementing:

```text
Liveness + HRA + MDE + solver + benchmarking
```

as one large feature.

Instead:

```text
Foundation
    ↓
Baseline
    ↓
Analysis
    ↓
Integration
    ↓
Optimization
    ↓
Validation
    ↓
Evaluation
```

The core rule is:

> **Every layer must work sufficiently well before the next layer is added.**

---

# 6. Mandatory Implementation Order

The implementation should follow this order unless a documented architectural decision changes it.

```text
Phase 0
Project / Environment Setup
        ↓
Phase 1
Frontend / IR / CFG
        ↓
Phase 2
Baseline Heap / Access-Path Representation
        ↓
Phase 3
Baseline HRA
        ↓
Phase 4
Backward Liveness
        ↓
Phase 5
Liveness + HRA
        ↓
Phase 6
MDE Study / Adapter Design
        ↓
Phase 7
MDE-HRA Representation
        ↓
Phase 8
Full Integration
        ↓
Phase 9
Correctness / Soundness Validation
        ↓
Phase 10
Instrumentation
        ↓
Phase 11
Benchmark Selection
        ↓
Phase 12
Experimental Execution
        ↓
Phase 13
Evaluation / Analysis
```

The detailed instructions for each phase belong in separate phase files.

---

# 7. Scope Discipline

The first implementation is intentionally scoped.

## 7.1 Supported Scope

The initial system should focus on:

- C programs;
- pointer variables;
- heap allocation;
- structures containing pointer fields;
- singly-linked structures;
- one or two relevant named fields;
- access paths such as:

```text
p
p.next
p.next.next
p.next.data
```

- liveness;
- HRA;
- MDE structural sharing;
- forward propagation;
- result comparison;
- performance measurement.

---

## 7.2 Explicit Initial Exclusions

Do not automatically add:

- unions;
- pointer arithmetic;
- unrestricted pointer casts;
- arbitrary unsupported C constructs;
- unrestricted heap shapes;
- complete general-purpose HRA;
- full context-sensitive pointer analysis;
- complete industrial compiler optimization support.

If a new feature becomes necessary, record it as a scope decision.

---

# 8. Scope Expansion Rule

If implementation reveals a potentially useful feature outside the current scope:

### Step 1

Record the feature.

### Step 2

Determine whether the current project actually requires it.

### Step 3

Check whether the feature affects:

- semantics;
- architecture;
- benchmark selection;
- correctness;
- evaluation.

### Step 4

Record the decision in:

```text
docs/decisions.md
```

### Step 5

Only then implement it.

Do not allow accidental scope expansion.

---

# 9. Three Required Analysis Configurations

The implementation must provide three logically distinguishable modes.

---

## Configuration A — Baseline HRA

```text
Program
  ↓
IR / CFG
  ↓
HRA
  ↓
Result
```

Purpose:

- establish baseline behavior;
- validate HRA independently;
- provide comparison data.

---

## Configuration B — Liveness + HRA

```text
Program
  ↓
IR / CFG
  ↓
Liveness
  ↓
Live Pointer Information
  ↓
HRA
  ↓
Result
```

Purpose:

- measure the effect of liveness pruning.

---

## Configuration C — Liveness + HRA + MDE

```text
Program
  ↓
IR / CFG
  ↓
Liveness
  ↓
Live Pointer Information
  ↓
HRA
  ↓
MDE
  ↓
Result
```

Purpose:

- evaluate the proposed integration.

---

# 10. Main Experimental Isolation Rule

The main MDE comparison must be:

```text
Liveness + HRA
        VS
Liveness + HRA + MDE
```

because both configurations use the same liveness strategy.

This isolates the effect of MDE as much as possible.

The broader comparison:

```text
HRA
        VS
Liveness + HRA
        VS
Liveness + HRA + MDE
```

is used to understand the combined effect.

---

# 11. Architecture Boundary Rules

The system should follow this conceptual dependency direction:

```text
Frontend
   |
   | program / CFG information
   v
Liveness
   |
   | LiveSet
   v
HRA
   |
   | HRA representation
   v
MDE-HRA Adapter
   |
   | deduplicated representation
   v
Solver
   |
   v
Results
```

---

## 11.1 Frontend Boundary

Frontend code is responsible for obtaining the program representation.

It must not contain:

- HRA semantics;
- MDE implementation;
- benchmark-specific analysis logic.

---

## 11.2 Liveness Boundary

Liveness is responsible for:

```text
CFG
+
USE/DEF
        ↓
LIVE-IN / LIVE-OUT
```

Liveness should not depend on MDE internals.

---

## 11.3 HRA Boundary

HRA is responsible for:

```text
pointer / heap operations
        ↓
heap access-path representation
```

HRA should remain independently testable.

---

## 11.4 MDE Boundary

MDE-HRA is responsible for:

```text
HRA structures
        ↓
deduplicated representation
```

MDE must not redefine the semantic meaning of HRA.

---

## 11.5 Solver Boundary

The solver is responsible for:

- propagation;
- merging;
- convergence;
- queries;
- final logical state.

---

## 11.6 Instrumentation Boundary

Instrumentation is responsible for observing:

- runtime;
- memory;
- construction;
- reuse;
- deduplication;
- result equivalence.

Instrumentation must not change semantic behavior.

---

# 12. Liveness Rules

Liveness is a **static analysis**.

Do not execute the program to determine liveness.

The analysis works over the program representation/CFG.

Conceptually:

```text
LIVE-IN  = USE ∪ (LIVE-OUT - DEF)

LIVE-OUT = union of successor LIVE-IN
```

The exact pointer-specific treatment must follow the chosen liveness-based analysis formulation used by the project.

The implementation must eventually provide inspectable:

```text
LIVE-IN
LIVE-OUT
```

for relevant program points.

---

# 13. HRA Rules

HRA should represent heap references using access-path/access-graph concepts.

Example:

```text
p
|
next
|
next
|
data
```

corresponds conceptually to:

```text
p.next.next.data
```

The first implementation must prioritize:

- correctness;
- clarity;
- testability;
- compatibility with the defined scope.

Do not attempt to reproduce every feature of a full general HRA implementation before the restricted version works.

---

# 14. MDE Rules

MDE is introduced **after** HRA works.

The purpose is to reduce repeated structural representation.

Example:

```text
p -> next -> data
q -> next -> data
r -> next -> data
```

contains repeated structure:

```text
next -> data
```

The implementation should investigate whether that structure can be shared.

The exact representation must come from the actual MDE/LatticeHashForest implementation selected for the project.

Never invent an API merely from a conceptual diagram.

---

# 15. Critical Semantic Invariant

The following is a project-wide invariant:

```text
LogicalResult(Liveness + HRA)
        ==
LogicalResult(Liveness + HRA + MDE)
```

for all supported test programs.

This does **not** mean that internal representations must be identical.

For example:

```text
Non-MDE:
node object A

MDE:
node ID 52
```

can still be semantically equivalent.

Correctness comparison must therefore operate on a normalized logical result.

---

# 16. Result Normalization Rule

Before comparing configurations:

1. extract the logical analysis result;
2. remove representation-specific IDs;
3. canonicalize ordering where necessary;
4. compare semantic content.

The exact canonical representation will be defined in `docs/design.md` and `docs/testing.md`.

Do not compare:

- memory addresses;
- allocation order;
- hash IDs;
- internal node numbers.

unless the metric specifically requires them.

---

# 17. Testing Rules

Testing is not postponed until the end.

Every implementation stage must have tests.

Required levels:

```text
Unit
  ↓
Component
  ↓
Integration
  ↓
Synthetic Program
  ↓
Benchmark
```

---

## 17.1 Unit Tests

Examples:

- live-set operations;
- access-path creation;
- field extension;
- graph operations;
- MDE structural reuse.

---

## 17.2 Component Tests

Examples:

```text
Liveness alone
HRA alone
MDE representation alone
```

---

## 17.3 Integration Tests

Examples:

```text
Liveness + HRA
HRA + MDE
Liveness + HRA + MDE
```

---

## 17.4 Regression Tests

Every discovered semantic bug must produce a regression test.

The fix is not complete until:

```text
failing test
    ↓
fix
    ↓
test passes
    ↓
relevant suite passes
```

---

# 18. Required Test Categories

At minimum, the project should eventually cover:

1. simple pointer;
2. pointer copy;
3. heap allocation;
4. single field access;
5. multi-field access path;
6. repeated access paths;
7. dead pointer;
8. branch;
9. loop;
10. control-flow join;
11. combined linked-structure case.

Detailed test cases belong in `docs/testing.md` and phase-specific files.

---

# 19. Debugging Rules

When the final result is incorrect, locate the **earliest stage where the state becomes incorrect**.

Use:

```text
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
Solver
 ↓
Result normalization
 ↓
Final result
```

Do not modify several modules simultaneously unless the failure is known to span them.

---

# 20. Intermediate-State Visibility

Each major analysis stage should provide a way to inspect its state during development.

At minimum, developers should eventually be able to inspect:

### CFG

```text
basic blocks
successors
predecessors
```

### Liveness

```text
program point
LIVE-IN
LIVE-OUT
```

### HRA

```text
root
access path
heap structure
```

### MDE

```text
constructed structures
reused structures
deduplication statistics
```

### Final result

```text
normalized logical result
```

This is essential for debugging.

---

# 21. Coding Rules

## Prefer

- small modules;
- explicit interfaces;
- meaningful names;
- deterministic behavior where practical;
- assertions for impossible states;
- documented invariants;
- focused commits;
- tests next to behavior changes.

## Avoid

- hidden global state;
- benchmark-specific special cases;
- hard-coded pointer names;
- hard-coded expected performance;
- duplicated semantic logic;
- mixing instrumentation into core semantics unnecessarily;
- premature optimization;
- undocumented assumptions.

---

# 22. Interface Rules

When one module needs another module:

1. define what information is required;
2. define the smallest interface that provides it;
3. avoid exposing internal implementation details;
4. document ownership/lifetime if relevant;
5. provide tests for the interface.

Example:

```text
Liveness → HRA
```

should conceptually exchange:

```text
LiveSet
```

rather than exposing liveness's entire internal solver.

---

# 23. No Hidden Semantic Changes

A performance optimization must not silently change:

- pointer reachability;
- access-path meaning;
- merge semantics;
- query semantics;
- fixpoint semantics.

If an optimization appears to require a semantic change, stop and investigate.

---

# 24. Performance Rules

Correctness comes first.

Required sequence:

```text
Correct implementation
       ↓
Correct tests
       ↓
Stable representation
       ↓
Instrumentation
       ↓
Benchmarking
       ↓
Performance optimization
```

Do not optimize based on intuition alone.

Measure first.

---

# 25. Instrumentation Rules

The project must eventually measure:

### Runtime

Total analysis execution time.

### Peak memory

Maximum memory consumed.

### Nodes/structures constructed

How many structural representations were requested.

### Nodes/structures reused

How many were satisfied by existing representations.

### Deduplication ratio

The project-level intended metric is:

```text
(Nodes Constructed - Nodes Reused)
/
Nodes Constructed
```

The exact operational definitions must be frozen before final experiments.

### Result equivalence

Whether:

```text
MDE result == non-MDE result
```

at the logical level.

---

# 26. Research Integrity Rules

Do not claim an improvement before measuring it.

Do not claim:

```text
X% faster
```

unless the experiment produced the number.

Do not claim:

```text
Y% less memory
```

unless memory was actually measured.

Do not claim:

```text
sound
```

merely because tests happened to pass.

Instead distinguish:

```text
tested equivalence
```

from:

```text
formal proof of soundness
```

unless a formal proof actually exists.

---

# 27. Prior Work vs Our Result

Prior published results are used as:

- motivation;
- design guidance;
- comparison context.

They are not automatically results of this implementation.

The implementation must independently measure MDE-HRA.

Do not transfer numerical results from prior work into our result tables.

---

# 28. Benchmark Rules

The project aims for approximately:

```text
5–8 open-source C programs
```

with relevant heap behavior.

Benchmarks should represent different levels of structural repetition.

Prefer a mixture of:

```text
low repetition
medium repetition
high repetition
```

and, where necessary, synthetic stress tests.

Do not select benchmarks solely because they produce favorable results.

---

# 29. Reproducibility Rules

Every benchmark result should be traceable to:

```text
benchmark
source/version
compiler version
build configuration
analysis configuration
command
machine/environment
raw output
processed result
```

Raw data must be preserved.

Processed tables must be reproducible from raw data.

---

# 30. Experiment Rules

Every benchmark should be run consistently under:

```text
A — HRA

B — Liveness + HRA

C — Liveness + HRA + MDE
```

The environment and build conditions should remain consistent.

The same logical input should be used for each configuration.

---

# 31. Negative Results Are Valid

Do not force the project to show an improvement.

Possible valid outcomes include:

```text
MDE substantially reduces memory.
```

or:

```text
MDE reduces memory but adds runtime overhead.
```

or:

```text
Liveness removes most repetition, so MDE provides little additional benefit.
```

or:

```text
MDE benefits only repetition-heavy programs.
```

The implementation exists to test the research question.

---

# 32. Documentation Rules

Documentation must evolve with the implementation.

Update documentation when:

- architecture changes;
- interfaces change;
- supported features change;
- metrics change;
- benchmarks change;
- assumptions change;
- an important implementation decision is made.

Do not allow code and documentation to diverge silently.

---

# 33. Decision Recording

Important decisions must be recorded in:

```text
docs/decisions.md
```

A decision should include:

```text
ID
Date
Status
Decision
Reason
Alternatives
Impact
```

Examples:

- choice of IR;
- choice of MDE implementation;
- HRA representation;
- result normalization;
- benchmark selection criteria;
- metric definitions;
- scope changes.

---

# 34. Phase Completion Rule

A phase is complete only when:

- implementation objectives are met;
- required tests exist;
- tests pass;
- intermediate state can be inspected;
- documentation is updated;
- known blocking issues are resolved or explicitly recorded.

A phase should not be declared complete merely because the code compiles.

---

# 35. AI Coding-Agent Rules

If an AI coding agent works on the repository, it must follow this procedure.

## Before modifying code

1. Read `project.md`.
2. Read `skill.md`.
3. Read the relevant phase file.
4. Read relevant architecture/design documentation.
5. Inspect the current repository state.
6. Identify existing implementation before creating new code.

## During modification

1. Make the smallest necessary change.
2. Preserve module boundaries.
3. Add/update tests.
4. Do not silently change scope.
5. Do not invent APIs for external libraries.
6. Do not remove failing tests merely to make the suite pass.
7. Do not change experimental data.

## After modification

Report:

```text
What changed
Why it changed
Files changed
Tests run
Tests passed/failed
Known limitations
Next recommended step
```

---

# 36. External Dependency Rule

When using an existing project/library:

1. inspect its actual source;
2. inspect its documented API;
3. identify version/commit;
4. record the dependency;
5. avoid assuming undocumented behavior.

For MDE in particular, the implementation should be based on the actual selected MDE/LatticeHashForest source rather than on a conceptual equation from presentation material.

---

# 37. No Blind Copying

Existing HRA, liveness, MDE, or pointer-analysis implementations may be used as references or dependencies according to their licenses and project decisions.

However:

- understand the code before integrating it;
- document reused components;
- isolate adaptations;
- do not copy large components without checking compatibility and licensing;
- preserve the distinction between original implementation and project-specific integration.

---

# 38. Integration Rule

Integration should happen through explicit interfaces.

Avoid:

```text
liveness.cpp
    directly edits
mde.cpp
```

Prefer:

```text
Liveness
   ↓
LiveSet
   ↓
HRA
   ↓
HRA Representation
   ↓
MDE Adapter
```

This makes each component independently testable.

---

# 39. Correctness Before Benchmarking

The project must not move to large performance evaluation while the semantic equivalence test is failing.

Required sequence:

```text
Basic correctness
      ↓
Component correctness
      ↓
Integration correctness
      ↓
MDE equivalence
      ↓
Instrumentation validation
      ↓
Benchmarking
```

---

# 40. Final System Requirements

The completed implementation should support:

```text
                  C Program
                      |
                      v
                 Frontend / IR
                      |
                      v
                     CFG
                      |
          +-----------+-----------+
          |                       |
          v                       |
      Liveness                    |
          |                       |
          v                       |
     Live Pointers                |
          |                       |
          +-----------+-----------+
                      |
                      v
                     HRA
                      |
                      v
                MDE / LHF
                      |
                      v
              Deduplicated State
                      |
                      v
                   Solver
                      |
                      v
                  Fixpoint
                      |
                      v
                   Result
                      |
          +-----------+-----------+
          |                       |
          v                       v
      Correctness             Metrics
      Comparison              Collection
```

The system must also allow the MDE component to be disabled so that the baseline can be executed under the same overall infrastructure.

---

# 41. Final Definition of Done

The implementation framework is considered ready for research evaluation only when:

## Foundation

- [ ] Repository builds.
- [ ] Development workflow is documented.
- [ ] Dependencies are recorded.
- [ ] Supported scope is documented.

## Analysis

- [ ] CFG/program representation works.
- [ ] Baseline HRA works.
- [ ] Liveness works.
- [ ] Liveness + HRA works.
- [ ] MDE representation works.
- [ ] Full MDE-HRA integration works.

## Correctness

- [ ] Unit tests pass.
- [ ] Component tests pass.
- [ ] Integration tests pass.
- [ ] Regression tests pass.
- [ ] MDE/non-MDE logical results match for supported tests.

## Measurement

- [ ] Runtime measurement works.
- [ ] Memory measurement works.
- [ ] Node construction measurement works.
- [ ] Node reuse measurement works.
- [ ] Dedup ratio measurement works.

## Research Evaluation

- [ ] Benchmark set is defined.
- [ ] Benchmark versions are recorded.
- [ ] All three configurations can run.
- [ ] Raw results are preserved.
- [ ] Experiments are reproducible.
- [ ] Research questions can be answered from measured data.

---

# 42. Final Operating Principle

The project should always follow this principle:

> **Build the simplest correct version first, validate it, then add the next layer. Never use a later optimization to hide an earlier semantic problem.**

The intended progression is:

```text
Understand
   ↓
Design
   ↓
Implement
   ↓
Test
   ↓
Validate
   ↓
Integrate
   ↓
Measure
   ↓
Evaluate
```

The phase-specific Markdown files will provide the detailed implementation instructions for each step of this progression.
