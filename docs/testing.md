# MDE-HRA Testing Strategy

> **Detailed testing, correctness validation, regression, semantic equivalence, and debugging strategy for MDE-HRA**

---

# 1. Purpose

This document defines how the MDE-HRA implementation will be tested from the first component through the final research system.

The testing strategy must answer two different questions:

1. **Does each component work according to its intended design?**
2. **Does adding liveness and MDE preserve the logical analysis result while improving scalability where expected?**

The project must not treat:

```text
compiles successfully
```

as equivalent to:

```text
analysis is correct
```

Testing therefore proceeds in layers:

```text
Unit
  ↓
Component
  ↓
Integration
  ↓
Semantic Equivalence
  ↓
Regression
  ↓
Benchmark Validation
  ↓
Research Experiments
```

---

# 2. Testing Philosophy

The project follows five principles.

## 2.1 Test one layer before combining layers

For example:

```text
Liveness
```

must be testable before:

```text
Liveness + HRA
```

and:

```text
HRA
```

must be testable before:

```text
HRA + MDE
```

---

## 2.2 Correctness before performance

The required order is:

```text
Implementation
    ↓
Functional tests
    ↓
Semantic validation
    ↓
Instrumentation validation
    ↓
Performance experiments
```

Do not optimize or benchmark a configuration whose semantic result is known to be incorrect.

---

## 2.3 Test semantics independently from representation

MDE changes representation.

Therefore tests must distinguish:

```text
semantic correctness
```

from:

```text
node count / sharing / memory
```

A test must not fail simply because MDE uses different internal IDs.

---

## 2.4 Every discovered bug becomes a regression test

Whenever a bug is found:

```text
Bug
 ↓
Minimal reproducer
 ↓
Regression test
 ↓
Fix
 ↓
Regression suite
```

The original failure must remain permanently testable.

---

## 2.5 Unsupported features must be explicit

If a program uses a construct outside the supported scope:

```text
UNSUPPORTED
```

is a valid test outcome.

Silently analyzing it as if it were supported is not.

---

# 3. Testing Pyramid

The project uses the following testing pyramid:

```text
                    /\
                   /  \
                  /    \
                 /Bench \
                /marks   \
               /----------\
              / Integration\
             /--------------\
            / Component      \
           /------------------\
          / Unit Tests         \
         /______________________\
```

Most tests should be small and deterministic.

Large benchmarks are used later for system-level validation, not for discovering basic data-structure bugs.

---

# 4. Test Levels

## Level 1 — Unit Tests

Test individual operations.

Examples:

```text
LiveSet.insert()
LiveSet.remove()
AccessPath.extend()
AccessPath.equals()
AccessPath.hash()
CanonicalResult.normalize()
```

---

## Level 2 — Component Tests

Test complete modules.

Examples:

```text
Liveness alone
HRA alone
MDE representation alone
Result normalization alone
```

---

## Level 3 — Integration Tests

Test interactions.

Examples:

```text
Liveness + HRA
HRA + MDE
Liveness + HRA + MDE
```

---

## Level 4 — Program Tests

Run the complete analysis on small C programs.

Examples:

```text
simple pointer
linked node
branch
loop
repeated heap structure
dead pointer
```

---

## Level 5 — Benchmark Tests

Run supported open-source programs through:

```text
A
B
C
```

and verify:

- successful execution;
- result equivalence;
- stable metrics;
- reproducibility.

---

# 5. Test Configuration Matrix

The test framework must explicitly support:

| ID | Configuration | Liveness | MDE |
|---|---|---|---|
| A | HRA | Off | Off |
| B | Liveness + HRA | On | Off |
| C | Liveness + HRA + MDE | On | On |

For semantic equivalence:

```text
B == C
```

is the primary correctness check.

For broader validation:

```text
A vs B
A vs C
B vs C
```

can all be examined.

---

# 6. Test Categories

The minimum test categories are:

```text
T01 — infrastructure
T02 — CFG
T03 — USE/DEF
T04 — liveness
T05 — access paths
T06 — HRA
T07 — MDE
T08 — liveness + HRA
T09 — HRA + MDE
T10 — full integration
T11 — result normalization
T12 — semantic equivalence
T13 — instrumentation
T14 — regression
T15 — benchmark validation
```

---

# 7. Infrastructure Tests

Before testing analysis logic:

- repository builds;
- test runner works;
- required dependencies are available;
- a minimal program can be compiled/parsed;
- test output is deterministic enough for comparison.

A broken environment must not be confused with an analysis failure.

---

# 8. CFG Tests

The CFG must be tested independently.

Required cases:

### Straight-line code

```text
B1 → B2 → B3
```

Expected:

- one successor for intermediate blocks;
- correct entry/exit.

---

### Branch

```text
       B1
      /  \
     B2  B3
      \  /
       B4
```

Expected:

```text
successors(B1) = {B2, B3}
predecessors(B4) = {B2, B3}
```

---

### Loop

```text
B1 → B2 → B3
     ↑    |
     +----+
```

Expected:

- back edge exists;
- traversal terminates;
- predecessor/successor relationships are correct.

---

# 9. USE/DEF Tests

For simple statements, verify that the program facts are extracted correctly.

Example:

```c
q = p;
```

Conceptually:

```text
USE = {p}
DEF = {q}
```

The exact treatment of allocations, dereferences, and field operations must follow the chosen liveness formulation.

---

# 10. LiveSet Unit Tests

Required operations:

```text
insert
contains
remove
union
difference
equality
empty
iteration
```

Example:

```text
A = {p, q}
B = {q, r}

A ∪ B = {p, q, r}

A - B = {p}
```

---

# 11. Liveness Test: Straight-Line Program

Example:

```c
void f(Node *p) {
    Node *q = p;
    use(q);
}
```

The test should verify that the backward solver identifies the pointer needed by the later use.

Do not hard-code an expected live set without following the exact USE/DEF formulation.

The test should compare the implementation output with the manually derived expected dataflow result.

---

# 12. Liveness Test: Dead Pointer

Example:

```c
void f(Node *p) {
    Node *q = p;
    q = 0;
    use(p);
}
```

The purpose is to test that information associated only with the overwritten/dead value is not incorrectly propagated beyond the relevant definition according to the selected liveness semantics.

---

# 13. Liveness Test: Branch

Example:

```c
if (condition) {
    use(p);
} else {
    use(q);
}
```

The join point must account for liveness from both successors.

Conceptually:

```text
LIVE-OUT(before branch)
=
LIVE-IN(path 1)
∪
LIVE-IN(path 2)
```

Expected behavior must be derived from the actual program-point placement.

---

# 14. Liveness Test: Loop

A loop must verify that backward propagation handles cycles.

Example:

```c
while (condition) {
    use(p);
}
```

The test must confirm:

- worklist iteration terminates;
- live information reaches loop predecessors;
- the final result is stable.

---

# 15. AccessPath Unit Tests

Required examples:

```text
p
p.next
p.next.next
p.next.data
```

Test:

```text
root()
fields()
length()
extend()
equals()
hash()
toString()
```

---

# 16. AccessPath Equality Tests

Verify:

```text
p.next.data == p.next.data
```

and:

```text
p.next.data != q.next.data
```

and:

```text
p.next.data != p.data.next
```

for the initial named-field model.

---

# 17. HRA Unit/Component Tests

The baseline HRA must be testable without liveness or MDE.

Required cases:

1. pointer copy;
2. allocation;
3. field traversal;
4. repeated field traversal;
5. simple heap chain;
6. merge at control-flow join.

---

# 18. HRA Test: Pointer Copy

Example:

```c
q = p;
```

Expected logical relationship:

```text
q
```

must represent the appropriate relation to the object/reference represented by:

```text
p
```

The exact abstract-object result depends on the HRA formulation.

The test should compare canonical semantic facts, not internal node IDs.

---

# 19. HRA Test: Linked Structure

Example:

```c
q = p->next;
r = q->next;
```

The analysis should construct a logical access structure corresponding to:

```text
p
 |
 next
 |
 q
 |
 next
 |
 r
```

The test should verify the expected access-path/reference relation.

---

# 20. HRA Test: Named Fields

Example:

```c
p->next
p->data
```

The implementation must distinguish:

```text
next
```

from:

```text
data
```

and preserve field labels in the semantic representation.

---

# 21. HRA Test: Branch Join

Example:

```c
if (condition)
    q = p->next;
else
    q = p;
```

The merge operation must correctly combine the possible states according to the HRA semantics.

The expected result must be expressed in canonical form.

---

# 22. HRA Test: Loop

A loop should test repeated propagation:

```c
while (p != 0) {
    p = p->next;
}
```

The test should verify:

- solver termination;
- stable state;
- no accidental infinite growth outside the intended abstraction.

---

# 23. MDE Unit Tests

MDE must first be tested independently of full HRA.

Required concepts:

```text
insert
lookup
structural equality
reuse
new structure creation
```

---

# 24. MDE Reuse Test

Suppose the same structural request is made twice:

```text
Structure S
Structure S
```

Expected:

```text
first request  → create
second request → reuse
```

The exact counters depend on the instrumentation definition.

---

# 25. MDE Non-Reuse Test

Two structurally different requests:

```text
next → data
```

and:

```text
next → next
```

must not be incorrectly treated as identical.

---

# 26. MDE Label Sensitivity Test

If field labels are semantically relevant:

```text
next → X
```

must not be deduplicated with:

```text
data → X
```

unless the selected MDE semantics explicitly defines them as equivalent.

---

# 27. HRA + MDE Integration Test

Run the same HRA structure through:

```text
MDE OFF
```

and:

```text
MDE ON
```

Then compare canonical logical results.

Expected:

```text
equal
```

while internal representation may differ.

---

# 28. Liveness + HRA Integration Test

Run:

```text
Liveness
    ↓
HRA
```

on a program where some pointer information is dead.

Verify that:

- liveness produces the expected live information;
- HRA receives it;
- the resulting semantic state follows the intended pruning rule.

---

# 29. Full Integration Test

Run:

```text
C
 ↓
CFG
 ↓
Liveness
 ↓
HRA
 ↓
MDE
 ↓
Solver
 ↓
Canonical Result
```

The test passes only if:

- execution succeeds;
- solver converges;
- canonical result is valid;
- no unsupported feature is silently ignored.

---

# 30. Primary Semantic Equivalence Test

For every supported test program:

```text
Run B
Run C
Normalize B
Normalize C
Compare B and C
```

Expected:

```text
CanonicalResult(B)
=
CanonicalResult(C)
```

This is the most important integration test for the research contribution.

---

# 31. Why B vs C Is the Primary Correctness Test

Configuration B:

```text
Liveness + HRA
```

Configuration C:

```text
Liveness + HRA + MDE
```

Only the MDE representation layer is added.

Therefore:

```text
B → C
```

is the cleanest test of whether MDE preserves semantics.

---

# 32. A vs B Test

Also compare:

```text
A = HRA
B = Liveness + HRA
```

This is not expected to produce identical internal states in general because liveness changes what information is considered relevant.

The purpose is to verify that:

- liveness is being applied;
- the result remains valid under the intended pruning semantics;
- no unexpected loss of required information occurs.

---

# 33. A vs C Test

This comparison evaluates the complete change:

```text
HRA
    →
Liveness + HRA + MDE
```

It is useful for overall system evaluation but does not isolate MDE.

---

# 34. Canonical Result Testing

The normalization layer must itself be tested.

Two semantically identical internal representations:

```text
Graph A:
Node 1 → Node 2

Graph B:
Node 51 → Node 83
```

should normalize to equivalent results.

---

# 35. Negative Equivalence Test

The comparison system must detect a real semantic difference.

Example:

```text
Result B:
p.next → H1

Result C:
p.next → H2
```

Expected:

```text
FAIL
```

and a useful difference report.

A comparison framework that always says PASS is not a correctness framework.

---

# 36. Difference Reporting Test

When results differ, verify that the tool reports:

```text
missing facts
extra facts
changed relations
```

where the canonical representation permits this.

Example:

```text
Missing in C:
    p.next → H1

Extra in C:
    p.next → H2
```

---

# 37. Regression Testing

Every fixed bug must produce:

```text
tests/regression/<bug-name>/
```

or the repository's equivalent regression location.

The regression case should contain:

```text
minimal source
expected behavior
configuration
reason for regression
```

The exact directory structure can be adapted to the repository.

---

# 38. Regression Example

Suppose a bug causes:

```text
p.next.next
```

to disappear after MDE deduplication.

Create a minimal program that reproduces it.

Then verify:

```text
B result contains path
C result contains same path
```

The regression test remains permanently in the suite.

---

# 39. Instrumentation Tests

Instrumentation must be tested independently.

Verify:

```text
runtime measurement exists
memory measurement exists
constructed counter increments correctly
reuse counter increments correctly
dedup ratio is calculated correctly
```

---

# 40. Counter Test

Suppose:

```text
Constructed = 10
Reused = 4
```

Expected:

```text
DedupRatio = 0.6
```

or:

```text
60%
```

depending on the output format.

If:

```text
Constructed = 0
```

expected:

```text
N/A
```

not:

```text
NaN
```

or:

```text
Infinity
```

---

# 41. Instrumentation Neutrality Test

Run:

```text
instrumentation OFF
```

and:

```text
instrumentation ON
```

on the same program.

The canonical semantic result should remain identical.

Instrumentation must observe the analysis rather than alter it.

---

# 42. Determinism Tests

Run the same configuration multiple times.

Verify that:

```text
canonical logical result
```

is identical across runs.

Internal object IDs may differ if they are not intended to be deterministic.

---

# 43. Serialization Tests

If results are written to files, verify:

- valid format;
- complete result;
- stable canonical ordering;
- configuration metadata included;
- benchmark identity included.

---

# 44. Unsupported Feature Tests

Create explicit tests for constructs outside the initial scope.

Examples:

```text
pointer arithmetic
union
unsupported cast
unsupported heap pattern
```

Expected behavior:

```text
UNSUPPORTED
```

or the project's explicitly selected policy.

Never classify an unsupported input as a successful semantic analysis.

---

# 45. Error Handling Tests

Test:

```text
invalid input
missing source
malformed program
unsupported construct
invalid configuration
invalid MDE state
non-convergence
result comparison failure
```

The system should produce a meaningful diagnostic.

---

# 46. Convergence Tests

For every loop-containing test:

```text
analysis terminates
```

must be verified.

Where a maximum iteration/worklist safety bound exists, test both:

```text
normal convergence
```

and:

```text
bound exceeded
```

behavior.

---

# 47. Test Oracle Hierarchy

Use the following hierarchy for expected results:

```text
1. mathematically/manual derived result
2. independently implemented small reference case
3. baseline HRA result where appropriate
4. normalized configuration comparison
5. performance metrics
```

Performance must never be used as a correctness oracle.

---

# 48. Avoiding Circular Validation

Do not validate MDE solely by:

```text
MDE result == itself
```

That proves nothing.

The preferred validation is:

```text
independent/non-MDE representation
        VS
MDE representation
```

through canonical semantic extraction.

---

# 49. Small Reference Programs

Before large benchmarks, maintain a suite of tiny programs.

Recommended categories:

```text
01_simple_pointer.c
02_pointer_copy.c
03_malloc.c
04_single_field.c
05_linked_chain.c
06_repeated_path.c
07_dead_pointer.c
08_branch.c
09_loop.c
10_join.c
11_combined.c
```

The exact filenames can be changed to match the repository.

---

# 50. Test Program Design Rules

Each small program should ideally test one main behavior.

Avoid writing a 200-line program to test:

```text
AccessPath.extend()
```

Small tests make debugging much easier.

Complex programs should be reserved for integration tests.

---

# 51. Example Test: Repeated Structure

A useful MDE-focused test should create repeated linked structures.

Conceptually:

```c
a = allocate_node();
b = allocate_node();

a->next = ...;
b->next = ...;

use(a->next);
use(b->next);
```

The exact program should be designed to create repeated structural requests under the supported HRA semantics.

Expected:

```text
B and C semantic results equal
C reports meaningful reuse if the structure is actually repeated
```

---

# 52. Example Test: Dead Repetition

Create repeated structures involving pointers that become dead.

Purpose:

```text
test whether liveness reduces unnecessary analysis
```

This helps distinguish:

```text
benefit from liveness
```

from:

```text
benefit from MDE
```

---

# 53. Example Test: Live Repetition

Create repeated structures that remain live.

Purpose:

```text
test whether MDE still provides structural sharing
```

This is important because if liveness removes all repetition, MDE may have little work to perform.

---

# 54. Example Test: MDE Must Preserve Structure

Construct two paths:

```text
p.next.data
```

and:

```text
p.next.next
```

They share a prefix:

```text
p.next
```

but have different suffixes.

MDE must not collapse them into one incorrect semantic structure.

---

# 55. Example Test: Field Distinction

Construct:

```text
p.next
p.data
```

The final canonical result must preserve the distinction between:

```text
next
```

and:

```text
data
```

---

# 56. Test Naming

Test names should identify:

```text
component
behavior
expected property
```

Examples:

```text
test_liveset_union
test_access_path_extension
test_liveness_branch_join
test_hra_linked_chain
test_mde_reuses_equal_structure
test_mde_does_not_merge_different_fields
test_mde_result_equals_baseline
```

---

# 57. Test Output

A test failure should identify:

```text
program
configuration
stage
expected
actual
```

For semantic comparison:

```text
benchmark: linked_chain_01
configuration: C
comparison: B vs C
status: FAIL
difference:
    ...
```

---

# 58. Testing by Development Phase

Each implementation phase must add tests.

Conceptually:

```text
Phase 0
    infrastructure tests

Phase 1
    frontend / CFG tests

Phase 2
    representation tests

Phase 3
    HRA tests

Phase 4
    liveness tests

Phase 5
    liveness + HRA tests

Phase 6
    MDE tests

Phase 7
    MDE-HRA integration tests

Phase 8
    equivalence tests

Phase 9
    instrumentation tests

Phase 10+
    benchmark validation
```

The phase-specific MDs will state the exact tests required before a phase is considered complete.

---

# 59. Test Gates

A phase should not proceed when its required gate fails.

Example:

```text
HRA phase
    ↓
HRA tests pass?
    ├── NO → debug
    └── YES → proceed
```

For MDE integration:

```text
MDE integration
    ↓
B == C?
    ├── NO → debug semantics
    └── YES → proceed to measurement
```

---

# 60. Correctness Gate Before Experiments

Before final benchmark experiments:

```text
All mandatory tests pass
        AND
B/C semantic equivalence passes
        AND
instrumentation is validated
        AND
unsupported cases are classified
```

Only then proceed to performance experiments.

---

# 61. Benchmark Validation Tests

Before using an open-source benchmark for research:

Verify:

```text
program builds/loads
supported features
analysis completes
configuration A works
configuration B works
configuration C works
B == C
```

If not:

```text
record unsupported/failure reason
```

Do not silently remove the benchmark from the dataset.

---

# 62. Repeated Benchmark Runs

For performance measurements, run the same configuration multiple times according to the final experiment protocol.

The testing framework should distinguish:

```text
correctness test run
```

from:

```text
performance measurement run
```

Performance runs should avoid unnecessary debugging output.

---

# 63. Test Data Preservation

Keep:

```text
source program
expected result
actual result
configuration
test logs
```

for important failures.

Research results should not be overwritten by later runs without traceability.

---

# 64. Test Result Categories

Use explicit statuses:

```text
PASS
FAIL
UNSUPPORTED
ERROR
TIMEOUT
NON_CONVERGENT
```

Do not reduce all failures to:

```text
FAIL
```

when more precise diagnosis is available.

---

# 65. Timeout Testing

Large or pathological inputs may take too long.

A timeout should be recorded as:

```text
TIMEOUT
```

not:

```text
FAIL
```

unless the experiment defines timeout as a failure for a specific metric.

---

# 66. Non-Convergence Testing

If the fixpoint solver exceeds its safety limit:

```text
NON_CONVERGENT
```

must be recorded.

The system should retain enough diagnostics to identify:

- program point;
- current worklist state if feasible;
- iteration count;
- relevant configuration.

---

# 67. Memory Failure Testing

If a run exceeds available memory:

```text
ERROR / OOM
```

must be recorded according to the experiment schema.

Do not fabricate a memory number.

---

# 68. Golden Tests

For stable small examples, maintain canonical expected results.

Example:

```text
expected/
    simple_pointer.result
    linked_chain.result
```

A golden test compares:

```text
actual canonical result
```

against:

```text
expected canonical result
```

---

# 69. Golden Test Rule

Golden results must be updated only when:

1. the intended semantic behavior changed;
2. the change was reviewed;
3. the reason is documented.

Never update golden files merely to make a failing test pass.

---

# 70. Property-Based Opportunities

Where practical, test general properties.

Examples:

### Access path

```text
extend(p, f).length = p.length + 1
```

### Equality

```text
x == x
```

### Normalization

```text
normalize(semantic_equivalent_A)
=
normalize(semantic_equivalent_B)
```

### MDE

```text
reusing a structure must not change its logical meaning
```

These can supplement, not replace, concrete tests.

---

# 71. Test Isolation

Tests should not depend on execution order.

Avoid:

```text
test A creates global MDE state
test B expects it to exist
```

Prefer:

```text
each test initializes its own controlled state
```

unless shared fixtures are explicitly managed.

---

# 72. Test Cleanup

Temporary artifacts should not contaminate future tests.

Clean:

```text
temporary output
temporary benchmark state
temporary serialized graphs
```

after each test where appropriate.

---

# 73. Performance Test Isolation

Performance tests should not include:

```text
verbose debug logging
graph visualization
expensive result dumps
```

unless those costs are intentionally part of the measured configuration.

Otherwise instrumentation can distort the research result.

---

# 74. Correctness vs Performance Builds

Where the implementation supports build modes:

```text
debug
release
```

the final experiment should use the documented fixed build configuration.

Correctness debugging can use debug builds.

Performance comparisons must use consistent builds across A/B/C.

---

# 75. Testing MDE Counters

The instrumentation should distinguish at least:

```text
requested
created
reused
```

if the actual implementation makes these distinctions.

The final metric definitions must be fixed before reporting.

---

# 76. Testing Deduplication Ratio

Given:

```text
created = C
reused = R
```

test:

```text
ratio = (C - R) / C
```

with:

```text
C > 0
```

and:

```text
C = 0
```

cases.

Also verify:

```text
0 <= ratio <= 1
```

when the event definitions make that mathematically expected.

---

# 77. Semantic Difference Test After MDE

Intentionally create a test fixture where the MDE adapter is faulty or a semantic relation is altered.

The equivalence test should detect:

```text
B != C
```

This validates the validator itself.

This test should be kept as a controlled test of the comparison mechanism, not as a normal production failure.

---

# 78. Full Validation Matrix

For each small supported program:

| Test | A | B | C | B=C |
|---|---:|---:|---:|---:|
| Simple pointer | ✓ | ✓ | ✓ | ✓ |
| Pointer copy | ✓ | ✓ | ✓ | ✓ |
| Single field | ✓ | ✓ | ✓ | ✓ |
| Linked chain | ✓ | ✓ | ✓ | ✓ |
| Repeated path | ✓ | ✓ | ✓ | ✓ |
| Dead pointer | ✓ | ✓ | ✓ | ✓ |
| Branch | ✓ | ✓ | ✓ | ✓ |
| Loop | ✓ | ✓ | ✓ | ✓ |
| Join | ✓ | ✓ | ✓ | ✓ |
| Combined | ✓ | ✓ | ✓ | ✓ |

This matrix should be maintained as the suite grows.

---

# 79. Failure Diagnosis Workflow

When a B/C equivalence test fails:

```text
1. Reproduce failure
        ↓
2. Compare canonical results
        ↓
3. Identify first semantic difference
        ↓
4. Compare B and C before MDE
        ↓
5. If B is already wrong:
       debug liveness/HRA
        ↓
6. If B is correct:
       inspect HRA → MDE adapter
        ↓
7. Inspect MDE structure
        ↓
8. Inspect propagation/fixpoint
        ↓
9. Add regression test
        ↓
10. Fix and rerun suite
```

---

# 80. Failure Diagnosis Principle

Always find:

> **The earliest stage where the state becomes incorrect.**

Do not start by modifying the final result-comparison code unless the comparison itself is demonstrably wrong.

---

# 81. Debugging Artifacts

When useful, produce:

```text
CFG dump
liveness dump
HRA graph dump
MDE structure dump
canonical result dump
difference report
metrics report
```

These should be optional and controlled by debug/configuration flags.

---

# 82. Testing Research Claims

Before claiming:

> MDE reduces memory

verify:

```text
B memory
C memory
```

under the same benchmark and environment.

Before claiming:

> MDE reduces runtime

verify:

```text
B runtime
C runtime
```

using the final measurement protocol.

Before claiming:

> MDE preserves correctness

verify:

```text
B canonical result
C canonical result
```

across the defined supported test suite and benchmarks.

---

# 83. Testing Negative Results

A test suite must not require:

```text
C faster than B
```

to pass.

It should require:

```text
C correct
```

and:

```text
metrics valid
```

Performance improvement is a research outcome, not a functional-test requirement.

---

# 84. Final Testing Gates

## Gate 1 — Foundation

```text
Build
CFG
USE/DEF
```

must pass.

---

## Gate 2 — Analysis

```text
Liveness
HRA
```

must pass independently.

---

## Gate 3 — Integration

```text
Liveness + HRA
```

must pass.

---

## Gate 4 — MDE

```text
MDE representation
HRA + MDE
```

must pass.

---

## Gate 5 — Semantic Equivalence

```text
B == C
```

must pass for all required supported tests.

---

## Gate 6 — Instrumentation

Metrics must be validated independently.

---

## Gate 7 — Benchmark Readiness

Only after Gates 1–6 pass should final benchmark experiments begin.

---

# 85. Minimum Definition of Testing Complete

Testing is sufficiently mature for research evaluation when:

- [ ] infrastructure tests pass;
- [ ] CFG tests pass;
- [ ] USE/DEF tests pass;
- [ ] liveness tests pass;
- [ ] access-path tests pass;
- [ ] HRA tests pass;
- [ ] MDE tests pass;
- [ ] liveness + HRA tests pass;
- [ ] HRA + MDE tests pass;
- [ ] full integration tests pass;
- [ ] canonical normalization tests pass;
- [ ] B/C equivalence tests pass;
- [ ] difference reporting is tested;
- [ ] instrumentation is tested;
- [ ] regression suite exists;
- [ ] unsupported cases are explicit;
- [ ] benchmark validation procedure is ready.

---

# 86. Final Testing Model

The complete testing flow is:

```text
                    CODE CHANGE
                         |
                         v
                 +---------------+
                 | Unit Tests    |
                 +---------------+
                         |
                       PASS?
                      /     \
                    NO       YES
                    |         |
                  DEBUG       v
                         +---------------+
                         | Component     |
                         | Tests         |
                         +---------------+
                                |
                              PASS?
                             /     \
                           NO       YES
                           |         |
                         DEBUG       v
                                +---------------+
                                | Integration   |
                                +---------------+
                                       |
                                     PASS?
                                    /     \
                                  NO       YES
                                  |         |
                                DEBUG       v
                                       +---------------+
                                       | B vs C        |
                                       | Equivalence   |
                                       +---------------+
                                              |
                                            PASS?
                                           /     \
                                         NO       YES
                                         |         |
                                       DEBUG       v
                                             +-------------+
                                             | Instrument |
                                             | Validation |
                                             +-------------+
                                                   |
                                                   v
                                             Benchmarks
                                                   |
                                                   v
                                             Experiments
```

---

# 87. Central Testing Principle

The most important testing rule of the project is:

> **MDE is allowed to change the representation, but it is not allowed to change the logical result.**

Therefore the central test is:

```text
              Liveness + HRA
                    |
                    | canonical result
                    v
                   B
                    \
                     \
                      == ?
                     /
                    /
                   C
                    ^
                    |
          Liveness + HRA + MDE
```

If:

```text
B == C
```

then the representation optimization has passed the semantic-equivalence gate.

Only after that should we ask:

```text
Did C use less memory?
Did C run faster?
How much structure was reused?
Where does MDE help?
Where does it not help?
```

Those are **research questions**, not correctness assumptions.
