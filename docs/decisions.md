# Architecture and Research Decision Log

## 1. Purpose

This document records important technical and research decisions for the MDE-HRA project.

Each significant decision should capture:

```text
what was decided
why
what alternative was rejected
what assumption it creates
what evidence could cause it to change
```

Important decisions must not be silently changed.

---

# 2. Project Identity

The project combines:

```text
Liveness-based pruning
+
Heap Reference Analysis (HRA)
+
MDE / LatticeHashForest-style structural deduplication
```

The central question is:

> Can structural deduplication make liveness-pruned heap-reference analysis more scalable while preserving the analysis result?

Initial scope is intentionally constrained so the interaction can be validated first.

---

# 3. D1 — Three Experimental Configurations

## Decision

Maintain:

```text
A = HRA
B = Liveness + HRA
C = Liveness + HRA + MDE
```

## Reason

This isolates contributions:

```text
A → B = liveness effect
B → C = MDE effect
A → C = overall effect
```

## Rejected

Only comparing baseline versus full system.

## Reason rejected

That would mix liveness and MDE effects.

---

# 4. D2 — B vs C Is the Primary MDE Comparison

The principal MDE experiment is:

```text
B = Liveness + HRA
C = Liveness + HRA + MDE
```

Both must use the same:

- input
- frontend
- CFG
- liveness result
- HRA transfer semantics
- fixpoint policy
- result normalization

The intended experimental difference is MDE representation/deduplication.

---

# 5. D3 — Semantic Correctness Uses Canonical Equivalence

The central invariant is:

```text
CanonicalResult(B) == CanonicalResult(C)
```

MDE is an optimization of representation, not a change in analysis semantics.

Do not confuse:

```text
same semantic result
same physical nodes
same runtime
same memory
```

Only semantic equality establishes preservation.

---

# 6. D4 — Compare Canonical Results, Not Raw Objects

Comparison follows:

```text
raw result
 ↓
canonicalization
 ↓
semantic comparison
```

Canonicalization removes implementation-dependent details such as:

- addresses
- object identity
- allocation order
- hash-table order
- internal IDs

This is necessary because MDE is expected to change representation identity.

---

# 7. D5 — Liveness Is Static Backward Analysis

Use the standard data-flow form:

```text
LIVE-IN  = USE ∪ (LIVE-OUT - DEF)
LIVE-OUT = union of LIVE-IN of successors
```

The precise USE/DEF semantics must match the project's pointer-level formulation.

Liveness is not a record of one concrete execution.

---

# 8. D6 — Liveness Precedes HRA Use

Conceptual pipeline:

```text
C program
 ↓
IR / CFG
 ↓
backward liveness
 ↓
HRA
```

Liveness supplies future-use information that HRA can use for validated pruning.

---

# 9. D7 — HRA Remains an Independent Baseline

Configuration A must remain:

```text
A = HRA
```

without dependency on MDE.

Reason:

- baseline debugging
- regression
- controlled experimentation
- reproducibility

Do not redesign baseline HRA around MDE merely for convenience.

---

# 10. D8 — MDE Is Primarily a Representation Optimization

Conceptually:

```text
HRA logical structure
 ↓
canonical structural identity
 ↓
MDE lookup
 ↓
reuse OR create
```

MDE should reduce duplicated physical representation while preserving HRA semantics.

---

# 11. D9 — Use an HRA→MDE Adapter

Use:

```text
HRA
 ↓
Adapter
 ↓
MDE
```

The adapter may translate structural information and request canonical nodes.

It must not:

- invent HRA transfer rules
- change liveness
- drop semantic information
- merge semantically different states

This boundary improves modularity and debugging.

---

# 12. D10 — Initial Scope: Singly-Linked Heap Structures

Initial implementation focuses on:

```text
singly-linked heap structures
```

and approximately one or two relevant named fields where practical.

Examples of access paths:

```text
p
p.next
p.next.next
p.next.data
```

This gives a controlled scope for validating the core idea.

---

# 13. D11 — Initially Exclude Unions and Complex Pointer Arithmetic

The first supported scope should not depend on:

```text
unions
complex pointer arithmetic
```

unless existing infrastructure already supports them reliably.

Reason: these features substantially complicate object/access-path modeling.

They may become future extensions.

---

# 14. D12 — Liveness Backward, HRA Forward

Use the natural directions:

```text
Liveness → backward
HRA      → forward
```

They interact through explicit state/query interfaces.

Do not force both analyses into one direction merely for implementation convenience.

---

# 15. D13 — Fixpoints Are Explicit

Liveness and HRA must have explicit convergence logic.

For liveness:

```text
initialize
 ↓
propagate backward
 ↓
repeat until stable
```

For HRA:

```text
initialize
 ↓
propagate forward
 ↓
merge
 ↓
repeat until stable
```

Loops make fixpoint handling necessary.

---

# 16. D14 — Static Analysis Is Not Concrete Execution

A pointer may be live because a future use exists on a possible control-flow path.

Therefore:

```text
not observed in one execution
```

must never be treated as equivalent to:

```text
statically dead
```

---

# 17. D15 — Structural Equality Must Be Semantic

MDE identity must reflect the relevant structure.

Required behavior:

```text
same semantic structure → reusable
different semantic structure → distinct
```

Equality based only on object address is insufficient.

Incorrect equality can directly produce incorrect analysis results.

---

# 18. D16 — Do Not Deduplicate Solely by Pointer Address

Concrete allocation addresses are not sufficient as structural identity.

Two logically equivalent regions may have different allocation identities.

MDE must operate on the semantic structure selected by the HRA representation.

---

# 19. D17 — Separate Logical and Physical State

The implementation distinguishes:

```text
logical analysis state
```

from:

```text
physical representation node
```

MDE may reduce physical duplication without changing the logical result.

Therefore node counts must be interpreted carefully.

---

# 20. D18 — Measure Deduplication Explicitly

Collect:

```text
candidate_nodes
new_nodes
reused_nodes
dedup_ratio
```

Default metric:

```text
dedup_ratio = reused_nodes / candidate_nodes
```

Always report raw counts as well.

---

# 21. D19 — Correctness and Performance Are Separate

The evaluation reports independently:

```text
correctness
representation/work
runtime
memory
scalability
```

A faster result is not evidence of correctness.

A smaller representation is not automatically a semantic improvement.

---

# 22. D20 — Include Low-Dedup Benchmarks

The benchmark suite must contain cases where structural reuse is low.

Reason:

- prevents favorable-case selection
- reveals MDE overhead
- shows where the technique is not useful

A valid result may be:

```text
high repetition → high reuse
low repetition → little reuse
```

---

# 23. D21 — Approximately 5–8 Main Benchmarks

The final evaluation should contain approximately:

```text
5–8 C programs
```

covering diverse behaviors such as:

- linked structures
- loops
- branches
- pointer reassignment
- liveness opportunities
- repeated heap structures
- at least one realistic workload

Exact programs are selected only after compatibility inspection.

---

# 24. D22 — Separate Development and Evaluation Benchmarks

Use tiny synthetic programs for:

```text
debugging
unit/integration tests
semantic reasoning
```

and 5–8 selected programs for:

```text
final evaluation
```

This keeps debugging manageable while retaining external validity.

---

# 25. D23 — Correctness Before Performance

Do not treat C as a successful optimization until:

```text
CanonicalResult(B) == CanonicalResult(C)
```

has been checked.

Incorrect optimizations can easily appear faster.

---

# 26. D24 — Instrumentation Must Be Observational

Counters must not change:

- liveness
- HRA transfer
- equality
- propagation
- MDE decisions
- canonical result

Validate that instrumentation-on and instrumentation-off runs preserve semantic results.

---

# 27. D25 — Preserve Raw Experimental Data

Keep:

```text
raw measurements
raw outputs
canonical results
```

separate from:

```text
speedups
aggregates
plots
summary tables
```

Raw data allows later verification and recomputation.

---

# 28. D26 — Record Reproducibility Metadata

Each run should identify:

```text
source version
analysis version
configuration
benchmark
input size
compiler
flags
hardware
run number
timestamp
```

This makes performance comparisons auditable.

---

# 29. D27 — No Cherry-Picking

Do not remove a benchmark simply because MDE performs poorly.

Negative or neutral results are part of the scientific evaluation.

---

# 30. D28 — Preserve Failure Categories

Use explicit statuses:

```text
FRONTEND_CRASH
ANALYSIS_CRASH
MDE_CRASH
ASSERTION_FAILURE
UNSUPPORTED_FEATURE
TIMEOUT
OOM
NON_CONVERGED
```

Different failures require different diagnoses.

---

# 31. D29 — External Implementations Are References, Not Automatic Dependencies

Existing HRA/MDE/liveness implementations may be studied, but before reuse inspect:

```text
license
API
data structures
assumptions
dependencies
compatibility
semantic behavior
```

Never blindly merge an external implementation.

---

# 32. D30 — Build Incrementally

Required order:

```text
repository
 ↓
frontend/CFG
 ↓
HRA representation
 ↓
HRA
 ↓
HRA correctness
 ↓
liveness
 ↓
liveness + HRA
 ↓
MDE study
 ↓
adapter
 ↓
MDE-HRA
 ↓
B/C equivalence
 ↓
instrumentation
 ↓
experiments
```

Each layer is validated before becoming a dependency.

---

# 33. D31 — Phase Documents Are Progressive

Do not assume every later implementation detail before repository inspection.

After Phase 0, update later phase plans if the actual codebase requires it.

This avoids building an implementation around assumptions that are not true in the repository.

---

# 34. D32 — B Is the MDE Semantic Reference

For MDE correctness:

```text
B = reference
C = optimized representation
```

The question is:

> Does C preserve the normalized behavior of B?

This gives the MDE experiment a precise semantic baseline.

---

# 35. D33 — MDE Must Not Change Liveness

B and C must use the same liveness information.

Otherwise the B/C comparison would mix:

```text
different liveness
+
different representation
```

and would no longer isolate MDE.

---

# 36. D34 — MDE Must Not Redefine HRA Transfer Semantics

HRA transfer rules should remain conceptually identical between B and C.

The intended change is representation/canonicalization, not analysis meaning.

---

# 37. D35 — Determinism Is Required

For the same:

```text
source
configuration
tool version
environment
```

the canonical semantic result should be deterministic where practical.

This is important for testing and experiment reproducibility.

---

# 38. D36 — Protect Hash-Key Identity

Objects used as hash keys must not mutate in a way that changes their hash/equality identity after insertion.

Otherwise MDE may suffer:

```text
failed lookup
duplicate node
incorrect reuse
invalid map/set state
```

---

# 39. D37 — Unsupported Behavior Must Be Explicit

If a benchmark depends on unsupported constructs, record:

```text
UNSUPPORTED
```

with the reason.

Do not silently approximate unsupported behavior and report it as sound analysis.

---

# 40. D38 — Result Normalization Is Architectural

Canonical result generation is a first-class stage:

```text
analysis
 ↓
raw result
 ↓
normalization
 ↓
canonical semantic result
```

This stage is required for robust B/C comparison.

---

# 41. D39 — Primary Performance Metrics

Core metrics:

```text
runtime
peak memory
logical states
physical representation nodes
candidate nodes
new nodes
reused nodes
dedup ratio
```

Runtime alone is insufficient to explain MDE behavior.

---

# 42. D40 — Scaling Results Are Empirical

Benchmark scaling may reveal observed trends, but it does not automatically establish formal asymptotic complexity.

Claims about complexity require separate theoretical justification.

---

# 43. D41 — Inputs Must Be Identical Across A/B/C

For a benchmark:

```text
A input == B input == C input
```

The intended difference is configuration.

This is mandatory for controlled comparison.

---

# 44. D42 — Semantic Mismatch Stops Performance Interpretation

If:

```text
CanonicalResult(B) != CanonicalResult(C)
```

the benchmark becomes a correctness case.

Performance numbers may be retained as debugging evidence but must not be presented as successful optimization results.

---

# 45. D43 — Phase Completion Requires Evidence

A phase requires:

```text
implementation
tests
observable behavior
documented assumptions
exit-criteria evidence
```

Compilation alone is not completion.

---

# 46. D44 — Prefer a Narrow Correct Subset

The first implementation should prioritize:

```text
small + correct + testable
```

over:

```text
broad + incomplete
```

The research contribution is the interaction of liveness, HRA, and MDE, not immediate support for every C feature.

---

# 47. D45 — Architecture Can Change With Evidence

If repository inspection or experiments invalidate an architectural assumption:

```text
observe discrepancy
 ↓
verify behavior
 ↓
assess impact
 ↓
update design
 ↓
record decision
 ↓
continue
```

Do not preserve an incorrect design merely because it was written first.

---

# 48. D46 — Negative Results Remain Valid

If MDE:

```text
does not improve runtime
uses more memory
helps only high-repetition workloads
```

these results must remain in the evaluation.

The objective is to characterize the technique, not guarantee improvement.

---

# 49. D47 — Claims Must Be Narrow

Avoid:

> MDE makes pointer analysis faster.

Prefer evidence-bound claims such as:

> On the evaluated supported heap-reference workloads, MDE reduced physical representation duplication while preserving the normalized B result, with runtime effects depending on workload characteristics.

Final wording must follow measured evidence.

---

# 50. D48 — Documentation Is Living Documentation

Update:

```text
architecture.md
design.md
development.md
decisions.md
```

when implementation evidence changes an assumption.

The documentation must reflect the actual system.

---

# 51. Decision Change Template

When changing an important decision, record:

```text
## Decision DXX — <title>

Status:
Accepted / Superseded / Rejected

Previous decision:
...

New decision:
...

Reason:
...

Evidence:
...

Impact:
...

Affected files:
...

Date:
...
```

Do not silently overwrite important architectural history.

---

# 52. Current High-Level Architecture

```text
                 C PROGRAM
                     |
                     v
                  IR / CFG
                     |
                     v
                 LIVENESS
                     |
                     v
                    HRA
                     |
              logical HRA state
                     |
                     v
               MDE ADAPTER
                     |
                     v
             MDE / LHF STRUCTURE
                     |
                     v
             FORWARD FIXPOINT
                     |
                     v
              CANONICAL RESULT
```

Configurations:

```text
A = HRA
B = Liveness + HRA
C = Liveness + HRA + MDE
```

Primary invariant:

```text
CanonicalResult(B) == CanonicalResult(C)
```

---

# 53. Decision Review Triggers

Revisit decisions if:

- HRA cannot expose required semantic state
- liveness cannot be queried at required program points
- MDE equality differs from the expected structural identity
- MDE mutation conflicts with HRA ownership
- canonicalization changes semantic content
- B/C results differ systematically
- instrumentation changes behavior
- benchmark requirements exceed supported scope
- performance bottlenecks are unrelated to representation duplication

Decisions should change because of evidence, not convenience.

---

# 54. Final Decision Principle

The project follows:

```text
Do not optimize what has not been understood.
Do not integrate what has not been tested.
Do not claim correctness without semantic comparison.
Do not claim performance without measurement.
Do not hide limitations.
```

The final implementation should emerge from:

```text
evidence
 ↓
small validated decisions
 ↓
incremental integration
 ↓
controlled experiments
 ↓
defensible research conclusions
```
