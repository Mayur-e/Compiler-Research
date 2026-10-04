# Development Workflow and Implementation Roadmap

## 1. Purpose

This document defines how the MDE-HRA project will be implemented from repository inspection through final experiments.

The system must preserve three independently testable configurations:

- **A = HRA**
- **B = Liveness + HRA**
- **C = Liveness + HRA + MDE**

The implementation proceeds incrementally so that every major component is understood, tested, and validated before it becomes a dependency of the next component.

---

## 2. Core Development Principles

### 2.1 Correctness before optimization

Use this order:

```text
understand → implement → test → validate → integrate → measure → optimize
```

Do not optimize a component whose semantics are not established.

### 2.2 Inspect existing code before rewriting

Before implementing HRA, liveness, or MDE, determine what the repository already contains.

Record:

- APIs
- data structures
- ownership
- equality/hash behavior
- fixpoint logic
- existing tests
- limitations
- build dependencies

Existing code is the first source of truth.

### 2.3 Keep semantic and representation layers separate

The architecture should preserve:

```text
Liveness
   ↓
HRA semantics
   ↓
HRA representation
   ↓
HRA→MDE adapter
   ↓
MDE representation
```

MDE must not silently redefine HRA semantics.

### 2.4 Preserve A/B/C throughout development

Every major integration step must preserve the ability to distinguish:

```text
A = HRA
B = Liveness + HRA
C = Liveness + HRA + MDE
```

---

# 3. Source-of-Truth Hierarchy

When documentation and implementation disagree, use:

1. actual repository behavior/code
2. existing tests
3. project architecture/design
4. referenced research implementations/papers
5. benchmark requirements
6. general assumptions

If an important discrepancy is found, record it in `decisions.md`.

---

# 4. Standard Development Cycle

Every task should follow:

```text
1. Read requirement
2. Locate affected code
3. Read surrounding interfaces
4. Identify invariants
5. Make smallest focused change
6. Compile
7. Run focused tests
8. Run related integration tests
9. Inspect output
10. Record design change if needed
```

Avoid large unrelated refactors.

---

# 5. Phase Roadmap

```text
Phase 0  Repository + Environment
Phase 1  Frontend / IR / CFG
Phase 2  Baseline HRA Representation
Phase 3  Baseline HRA
Phase 4  HRA Correctness
Phase 5  Backward Liveness
Phase 6  Liveness + HRA
Phase 7  MDE Study / Isolation
Phase 8  HRA→MDE Adapter
Phase 9  MDE-HRA Representation
Phase 10 Full Integration
Phase 11 B/C Semantic Equivalence
Phase 12 Instrumentation
Phase 13 Benchmark Harness
Phase 14 Experiments
Phase 15 Final Analysis
```

A phase is complete only when implementation, tests, evidence, and exit criteria are satisfied.

---

# 6. Phase 0 — Repository and Environment

## Objective

Understand the actual project before changing it.

## Tasks

- map source directories
- identify build system
- identify compiler/toolchain
- identify IR/frontend
- run baseline build
- run existing tests
- locate HRA
- locate liveness
- locate MDE/LatticeHashForest
- locate analysis driver
- locate configuration mechanism
- record known limitations

## Baseline artifacts

Create records containing:

```text
build command
test command
tool versions
repository revision
binary names
known failures
```

## Exit criteria

- baseline builds or known build failure is documented
- existing tests are recorded
- relevant implementations are located
- unknowns are explicitly listed

---

# 7. Phase 1 — Frontend / IR / CFG

## Objective

Establish the stable program representation used by later analyses.

Verify:

- source-to-IR conversion
- basic blocks
- predecessors
- successors
- program points
- statement/instruction representation
- deterministic CFG construction

Use tiny programs with:

```text
straight-line code
if/else
loops
nested branches
```

## Exit criteria

A source program can be transformed into an inspectable deterministic CFG.

---

# 8. Phase 2 — Baseline HRA Representation

## Objective

Define the semantic representation independently of MDE.

Required conceptual objects:

```text
ProgramPoint
AccessPath
HeapObject
Field
HRANode
HRAEdge
HRAState
```

Access paths may look like:

```text
p
p.next
p.next.next
p.next.data
```

Equality and hashing must reflect semantic identity rather than accidental memory addresses.

## Exit criteria

Representations can be constructed, compared, printed, and tested.

---

# 9. Phase 3 — Baseline HRA

Implement configuration A:

```text
A = HRA
```

Develop in small steps:

```text
allocation
pointer assignment
field write
field read
dereference
heap-object creation
state merge
forward propagation
fixpoint
```

Do not add MDE shortcuts.

The baseline must remain a trustworthy reference.

## Exit criteria

Small supported heap programs terminate and produce inspectable HRA results.

---

# 10. Phase 4 — HRA Correctness

Build tests for:

- one allocation
- multiple allocations
- pointer aliasing
- field access
- linked-list nodes
- repeated `next` traversal
- branches
- loops
- state merges

Useful debug output:

```text
program point
input state
transfer
output state
created nodes
merged states
```

## Exit criteria

Expected behavior is established for the supported HRA subset.

---

# 11. Phase 5 — Backward Liveness

Implement liveness independently.

Core equations:

```text
LIVE-IN  = USE ∪ (LIVE-OUT - DEF)
LIVE-OUT = union of LIVE-IN of successors
```

The exact USE/DEF definition must follow the selected pointer-level formulation.

Use a worklist until a fixpoint is reached.

Test:

- used pointer
- unused pointer
- pointer after reassignment
- last use
- branch
- loop
- join

Important: liveness is a static future-use property, not a trace of one execution.

## Exit criteria

Liveness reaches a stable solution and can be inspected at required program points.

---

# 12. Phase 6 — Liveness + HRA

Implement configuration B:

```text
B = Liveness + HRA
```

Liveness should reach HRA through a defined interface rather than scattered checks.

Conceptually:

```text
program point
      ↓
live pointer information
      ↓
HRA pruning decision
      ↓
HRA propagation
```

Do not change HRA semantics beyond the validated liveness-based pruning policy.

## Exit criteria

B passes the supported regression suite and produces valid normalized results.

---

# 13. Phase 7 — MDE Study / Isolation

Before integration, inspect the MDE implementation.

Determine:

- node identity
- structural equality
- hashing
- canonicalization
- node lookup
- reuse
- ownership
- mutability
- metadata
- lifecycle

Create an MDE-only test containing:

```text
equivalent structure A
equivalent structure B
different structure C
```

Expected:

```text
A == B → reuse
A != C → distinct
```

## Exit criteria

The MDE API and its identity/reuse semantics are understood and tested.

---

# 14. Phase 8 — HRA→MDE Adapter

Create a clear boundary:

```text
HRA → Adapter → MDE
```

The adapter may:

- map HRA structure to MDE nodes
- construct canonical keys
- request reused nodes
- preserve semantic metadata

It must not:

- invent HRA transfer rules
- change liveness
- drop semantic information
- merge semantically different states

## Exit criteria

An HRA state can be represented by MDE without semantic loss.

---

# 15. Phase 9 — MDE-HRA Representation

Implement:

```text
logical HRA structure
        ↓
canonical structural key
        ↓
MDE lookup
        ↓
existing node OR new node
```

Required properties:

- deterministic identity
- structural sharing
- semantic separation
- stable lookup

Tests must show:

```text
equivalent → reused
different  → distinct
```

---

# 16. Phase 10 — Full Integration

Combine:

```text
frontend
CFG
liveness
HRA
MDE
fixpoint
result normalization
```

Do not replace B wholesale.

Instead:

```text
working B
  ↓
insert MDE boundary
  ↓
focused tests
  ↓
B/C comparison
  ↓
loops and branches
```

## Exit criteria

C runs on the complete supported regression suite.

---

# 17. Phase 11 — B/C Semantic Equivalence

The central invariant is:

```text
CanonicalResult(B) == CanonicalResult(C)
```

Implement:

```text
canonicalize(result)
compare(result1, result2)
difference_report()
```

If a mismatch occurs:

1. stop performance interpretation
2. preserve both results
3. locate first semantic divergence
4. inspect adapter
5. inspect equality/hash
6. inspect state mutation
7. inspect propagation
8. add regression test
9. fix
10. rerun

---

# 18. Phase 12 — Instrumentation

Add counters without changing semantics.

Recommended:

```text
runtime
peak_memory
HRA nodes
logical states
propagation steps
fixpoint iterations
MDE lookups
MDE hits
MDE misses
candidate nodes
new nodes
reused nodes
dedup ratio
```

Instrumentation must be observational.

Verify that enabling instrumentation does not alter normalized results.

---

# 19. Phase 13 — Benchmark Harness

Automate:

```text
load benchmark
→ run A
→ run B
→ run C
→ collect metrics
→ normalize
→ compare B/C
→ save artifacts
```

Record:

```text
command
exit code
runtime
memory
configuration
source version
input size
tool version
```

The harness should orchestrate experiments, not implement analysis semantics.

---

# 20. Phase 14 — Experiments

Run the selected 5–8 benchmarks under:

```text
A = HRA
B = Liveness + HRA
C = Liveness + HRA + MDE
```

Primary comparisons:

```text
A vs B → liveness effect
B vs C → MDE effect
A vs C → overall effect
```

Collect:

```text
runtime
memory
logical states
physical nodes
candidate nodes
new nodes
reused nodes
dedup ratio
B/C correctness
```

---

# 21. Phase 15 — Final Analysis

Separate:

```text
correctness
representation/work
runtime
memory
scalability
```

Never infer correctness from speed.

Final outputs should include:

- correctness table
- runtime table
- memory table
- MDE reuse table
- scaling graphs
- B/C difference summary
- benchmark metadata

Every claim must be traceable to stored evidence.

---

# 22. Coding Rules

- Make focused changes.
- Avoid unrelated refactoring.
- Keep interfaces explicit.
- Protect hash-key identity from mutation.
- Prefer deterministic representations.
- Keep debug output available during development.
- Preserve baseline A and B.
- Do not silently broaden supported language features.
- Document unsupported behavior.

---

# 23. Testing Rule

Every change should progress through:

```text
compile
 ↓
focused unit test
 ↓
component test
 ↓
integration test
 ↓
regression suite
```

Do not wait until final integration to test a component.

---

# 24. Debugging Workflow

When output is wrong:

```text
reduce input
   ↓
find first divergent program point
   ↓
inspect CFG
   ↓
inspect liveness
   ↓
inspect HRA state
   ↓
inspect MDE representation
   ↓
classify component
   ↓
add regression test
   ↓
fix smallest responsible component
   ↓
rerun regression suite
```

Possible fault classes:

```text
frontend
CFG
liveness
HRA
adapter
MDE equality/hash
state mutation
fixpoint
normalization
```

---

# 25. AI Coding-Agent Procedure

An AI coding agent must:

1. read `project.md`
2. read `skill.md`
3. read `architecture.md`
4. read `design.md`
5. read `testing.md`
6. read the current phase document
7. inspect the actual repository
8. identify existing implementations
9. state the intended focused change
10. implement
11. build
12. run tests
13. inspect output
14. report changed files and evidence

The agent must not invent repository paths, APIs, or implementation behavior.

---

# 26. Git / Checkpoint Strategy

Use logical checkpoints such as:

```text
phase-0-baseline
phase-1-cfg
phase-2-hra-model
phase-3-hra
phase-4-hra-tests
phase-5-liveness
phase-6-liveness-hra
phase-7-mde-study
phase-8-adapter
phase-9-mde-representation
phase-10-integration
phase-11-equivalence
phase-12-instrumentation
phase-13-harness
phase-14-experiments
```

A checkpoint should correspond to a tested state.

---

# 27. Documentation Synchronization

Update documentation when:

- an interface changes
- a data structure changes
- a supported feature changes
- an assumption changes
- an experimental metric changes
- an architectural decision changes

Record significant changes in `decisions.md`.

---

# 28. Phase Completion Rule

A phase is complete only when it has:

```text
implementation
+
tests
+
observable behavior
+
documented assumptions
+
exit-criteria evidence
```

Compilation alone does not constitute completion.

---

# 29. Final Definition of Done

The implementation is complete when:

- [ ] baseline repository builds
- [ ] frontend/IR/CFG is stable
- [ ] baseline HRA works
- [ ] HRA correctness tests pass
- [ ] liveness works independently
- [ ] B works
- [ ] MDE is independently understood/tested
- [ ] HRA→MDE adapter works
- [ ] C works
- [ ] B/C semantic equivalence passes
- [ ] instrumentation works
- [ ] benchmark runner works
- [ ] benchmark suite is reproducible
- [ ] experiments are complete
- [ ] raw results are stored
- [ ] final analysis is traceable to raw data

---

# 30. Core Rule

Never jump directly from:

```text
HRA
```

to:

```text
HRA + Liveness + MDE + Benchmarks
```

Use:

```text
HRA
 ↓
HRA correctness
 ↓
Liveness
 ↓
Liveness + HRA
 ↓
MDE study
 ↓
Adapter
 ↓
MDE-HRA
 ↓
B/C equivalence
 ↓
Instrumentation
 ↓
Experiments
```

This sequence minimizes debugging ambiguity and protects the research validity of the final result.
