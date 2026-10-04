# MDE-HRA Technical Design

> **Detailed technical design, data model, interfaces, semantic comparison, and result visualization for the MDE-HRA project**

---

# 1. Purpose

This document converts the system architecture into a **technical design that can be implemented and tested**.

The architecture document answers:

> What components exist and how are they connected?

This document answers:

> What does each component contain, what information does it exchange, how does the analysis operate, and how do we compare the three configurations?

The design covers:

- program points;
- CFG information;
- USE/DEF information;
- liveness;
- live sets;
- access paths;
- heap objects;
- HRA state;
- HRA graphs;
- MDE representation;
- HRA-to-MDE adapter;
- propagation;
- fixpoint;
- result normalization;
- semantic comparison;
- metrics;
- visualization;
- interfaces;
- invariants;
- error handling;
- deterministic behavior.

The exact APIs and type names can be adjusted to the selected implementation language and repository, but the **semantic contracts in this document should remain stable**.

---

# 2. Design Principles

The implementation follows these principles.

## 2.1 Semantics before representation

First define:

> What should the analysis mean?

Then define:

> How should that meaning be stored efficiently?

Therefore:

```text
Semantic model
     ↓
Baseline representation
     ↓
MDE representation
```

not:

```text
MDE data structure
     ↓
invent semantics around it
```

---

## 2.2 One semantic analysis, multiple representations

The project should not implement separate semantic algorithms for:

```text
HRA
Liveness + HRA
Liveness + HRA + MDE
```

Instead:

```text
Common semantic pipeline
        |
        +---- MDE OFF
        |
        +---- MDE ON
```

This is essential for a fair comparison.

---

## 2.3 Every major object needs a clear meaning

Every internal object must answer:

- What does it represent?
- Who creates it?
- Who owns it?
- Who reads it?
- Can it be modified?
- How is equality defined?
- How is it serialized/debugged?

---

## 2.4 Logical equality is more important than object equality

Two internal graphs may have:

```text
different node IDs
different allocation addresses
different hash locations
```

while representing the same logical analysis result.

Therefore:

```text
Object identity != Semantic identity
```

---

# 3. Overall Technical Model

The core model is:

```text
Program
  ↓
ProgramPoint / CFG
  ↓
USE / DEF
  ↓
LiveSet
  ↓
AccessPath
  ↓
Heap Object / HRA State
  ↓
HRA Graph
  ↓
MDE Representation
  ↓
Propagation / Fixpoint
  ↓
Canonical Result
```

---

# 4. Core Concepts

The following concepts are the minimum conceptual vocabulary of the implementation.

```text
ProgramPoint
BasicBlock
UseDefInfo
LiveSet
PointerVariable
HeapObject
Field
AccessPath
HRAEdge
HRANode
HRAState
HRAResult
MDENode / MDEStructure
AnalysisState
CanonicalResult
Metrics
```

The concrete class/struct names can differ, but the concepts should remain identifiable.

---

# 5. ProgramPoint

## 5.1 Meaning

A `ProgramPoint` identifies a location in the analyzed program at which analysis information is associated.

Depending on the IR, it may represent:

- an instruction;
- a statement boundary;
- a basic-block entry;
- a basic-block exit.

The exact granularity must be selected once the frontend/IR is finalized.

---

## 5.2 Required Properties

Conceptually:

```text
ProgramPoint
    ├── function
    ├── block
    └── instruction/statement position
```

---

## 5.3 Requirements

A program point must be:

- uniquely identifiable;
- comparable;
- usable as a map key;
- printable for debugging;
- stable across one analysis run.

---

# 6. BasicBlock

A basic block represents a sequence of instructions with one entry and one or more control-flow relationships according to the chosen CFG representation.

Conceptually:

```text
BasicBlock
    ├── ID
    ├── instructions
    ├── predecessors
    └── successors
```

The block should not own semantic HRA information.

---

# 7. USE/DEF Information

Liveness requires program facts describing pointer use and definition.

Conceptually:

```text
UseDefInfo
    ├── USE
    └── DEF
```

For a program point:

```text
USE(p)
DEF(p)
```

must be available to the liveness component.

---

## 7.1 Example

For:

```c
q = p;
```

conceptually:

```text
USE = {p}
DEF = {q}
```

For:

```c
p = malloc(...);
```

the exact treatment depends on the chosen pointer-liveness formulation, but the implementation must document it consistently.

---

# 8. LiveSet

A `LiveSet` represents pointers considered live at a program point.

Conceptually:

```text
LiveSet = {p, q, r}
```

It must support:

```text
insert
remove
contains
union
difference
equality
isEmpty
iteration
```

---

## 8.1 LIVE-IN

```text
LIVE-IN[p]
```

represents pointers that may be required before program point `p`.

---

## 8.2 LIVE-OUT

```text
LIVE-OUT[p]
```

represents pointers that may be required after program point `p`.

---

# 9. Liveness Data Model

For each relevant program point:

```text
LivenessInfo
    ├── liveIn
    └── liveOut
```

A global liveness result is conceptually:

```text
Map<ProgramPoint, LivenessInfo>
```

The exact container can vary.

---

# 10. Liveness Equations

The general backward equations are:

```text
LIVE-OUT[n]
    =
union LIVE-IN[s]
for every successor s of n
```

and:

```text
LIVE-IN[n]
    =
USE[n] ∪ (LIVE-OUT[n] - DEF[n])
```

The solver repeats until no live set changes.

---

# 11. Liveness Worklist

A conceptual implementation is:

```text
initialize all LIVE-IN and LIVE-OUT

worklist = all relevant program points

while worklist not empty:
    n = remove(worklist)

    newOut = union of successor liveIn

    newIn = use[n] union
            (newOut - def[n])

    if newOut != oldOut:
        update out[n]

    if newIn != oldIn:
        update in[n]

    if changed:
        add predecessors to worklist
```

This is a conceptual algorithm; the exact scheduling mechanism can differ.

---

# 12. AccessPath

## 12.1 Meaning

An `AccessPath` represents a sequence of pointer/field traversals from a root pointer.

Examples:

```text
p
p.next
p.next.next
p.next.data
```

Conceptually:

```text
AccessPath
    ├── root = p
    └── fields = [next, next, data]
```

---

## 12.2 Canonical Form

An access path should have a canonical representation.

For example:

```text
root: p
fields: [next, next, data]
```

rather than storing only:

```text
"p.next.next.data"
```

as an opaque string.

The string form can be generated for debugging/output.

---

# 13. AccessPath Operations

The representation should support:

```text
extend(field)
prefix()
suffix()
length()
root()
fields()
equals()
hash()
toString()
```

Example:

```text
p.next
    .extend(next)
        ↓
p.next.next
```

---

# 14. AccessPath Equality

Two access paths are equal when:

```text
same root
AND
same ordered field sequence
```

For example:

```text
p.next.data
```

equals:

```text
p.next.data
```

but not:

```text
q.next.data
```

and not:

```text
p.data.next
```

unless the semantic representation explicitly says otherwise.

---

# 15. HeapObject

A `HeapObject` represents an abstract heap allocation/object.

It is not necessarily a concrete runtime memory address.

Conceptually:

```text
HeapObject
    ├── abstract ID
    └── allocation/source information
```

The exact abstraction depends on the selected HRA formulation.

---

# 16. Field

A field represents a named structure field.

Example:

```c
node->next
```

has field:

```text
next
```

and:

```c
node->data
```

has field:

```text
data
```

The initial scope emphasizes named fields such as linked-list `next`.

---

# 17. HRA Node

An HRA node represents an abstract location/reference state in the HRA structure.

Conceptually:

```text
HRANode
    ├── semantic identity
    ├── associated heap/object information
    └── outgoing field edges
```

The node must not be confused with an MDE node.

An HRA node is part of the semantic/analysis representation.

An MDE node is part of the optimized structural representation.

---

# 18. HRA Edge

An HRA edge represents a field-based transition.

Example:

```text
Node A --next--> Node B
```

Conceptually:

```text
HRAEdge
    ├── source
    ├── field
    └── target
```

---

# 19. HRA Graph

The baseline HRA graph is conceptually:

```text
HRA Graph
    ├── roots
    ├── nodes
    └── edges
```

Example:

```text
p
|
next
|
v
N1
|
next
|
v
N2
|
data
|
v
N3
```

which represents an access structure related to:

```text
p.next.next.data
```

---

# 20. HRA State

An `HRAState` represents the analysis information at a program point.

Conceptually:

```text
HRAState
    ├── pointer bindings
    ├── heap/reference structure
    └── relevant access-path information
```

It may be associated with:

```text
ProgramPoint
```

or with solver worklist entries.

---

# 21. Pointer Binding

A pointer binding conceptually answers:

> Which abstract heap locations can this pointer refer to?

Example:

```text
p → {H1, H2}
```

This is a logical relation.

The internal representation can later be optimized by MDE.

---

# 22. HRA Transfer

For each relevant statement/instruction, HRA applies a transfer operation.

Conceptually:

```text
StateBefore
     |
     v
Transfer(statement)
     |
     v
StateAfter
```

Examples of statement categories include:

```text
allocation
pointer assignment
field read
field write
pointer copy
```

Only operations inside the defined project scope must initially be supported.

---

# 23. HRA Merge

At a control-flow join:

```text
       B1
        \
         \
          B3
         /
        /
       B2
```

states from different predecessors must be merged.

Conceptually:

```text
StateJoin = merge(StateB1, StateB2)
```

The merge operation must preserve the intended HRA semantics.

---

# 24. MDE Representation

The MDE layer represents repeated structures in a shared/deduplicated form.

Conceptually:

```text
HRA structures:

A → next → X
B → next → X
C → next → X

             ↓ MDE

        shared structure
              X
             ↑
       +-----+-----+
       |     |     |
       A     B     C
```

The exact MDE/LatticeHashForest data structure must be based on the actual selected implementation.

---

# 25. MDE Node

An MDE node is a representation object.

It may contain information such as:

```text
structural key
children / transitions
reference count or reuse information
associated metadata
```

The exact fields must be determined from the actual MDE implementation.

Do not invent undocumented MDE internals.

---

# 26. Structural Identity

MDE requires a definition of when two structures are considered reusable.

Conceptually:

```text
same structure
=
same relevant label
+
same relevant children/substructure
```

The exact structural key must be derived from the chosen MDE implementation.

---

# 27. HRA-to-MDE Adapter

The adapter is responsible for translating:

```text
HRA representation
```

into:

```text
MDE representation
```

Conceptually:

```text
HRA Node/Edge
      |
      v
Adapter
      |
      v
MDE Key / Structure
      |
      v
MDE Lookup
      |
      +---- existing ----> reuse
      |
      +---- missing -----> create
```

---

# 28. Adapter Responsibilities

The adapter should:

1. receive HRA structural information;
2. construct the appropriate MDE structural key;
3. query MDE;
4. reuse an existing structure when equivalent;
5. create a new structure when necessary;
6. return the representation handle;
7. record reuse/creation statistics.

It should not redefine HRA semantics.

---

# 29. Adapter Non-Responsibilities

The adapter should not:

- decide whether a pointer is live;
- redefine pointer semantics;
- alter program control flow;
- silently remove a semantic HRA edge;
- introduce benchmark-specific logic.

---

# 30. AnalysisState

A common top-level analysis state can conceptually contain:

```text
AnalysisState
    ├── programPoint
    ├── livePointers
    └── HRAState
```

For MDE mode:

```text
AnalysisState
    ├── programPoint
    ├── livePointers
    ├── HRAState
    └── MDE representation handle
```

The MDE handle is representation information, not a replacement for the logical meaning of the HRA state.

---

# 31. Configuration Object

The analysis should be controlled by configuration.

Conceptually:

```text
AnalysisConfig
    ├── livenessEnabled
    ├── mdeEnabled
    ├── instrumentationEnabled
    └── output/debug settings
```

Required combinations:

```text
false, false  → HRA baseline
true,  false  → Liveness + HRA
true,  true   → Liveness + HRA + MDE
```

Other combinations may be technically possible but are not required for the primary experiment.

---

# 32. Configuration Matrix

| Liveness | MDE | Configuration |
|---:|---:|---|
| Off | Off | A — HRA |
| On | Off | B — Liveness + HRA |
| On | On | C — Liveness + HRA + MDE |

The experimental framework should record the configuration explicitly in every result.

---

# 33. Propagation Engine

The propagation engine receives:

```text
current program point
current state
```

and produces successor states.

Conceptually:

```text
        current state
              |
              v
       transfer function
              |
              v
       successor states
              |
              v
        merge/update
              |
              v
          worklist
```

---

# 34. Worklist Entry

A worklist item can conceptually contain:

```text
WorkItem
    ├── programPoint
    └── analysis state
```

The exact representation may use references/IDs rather than full copies.

---

# 35. Fixpoint Condition

The analysis reaches a fixpoint when processing remaining worklist entries cannot produce any semantically new information.

Conceptually:

```text
while worklist not empty:
    process item

    if state changes:
        enqueue affected successors

stop when:
    no new state changes
```

---

# 36. Semantic State vs Physical State

This distinction is critical.

## Semantic state

Example:

```text
p may point to H1
p.next may point to H2
```

## Physical state

Example:

```text
MDE node 81
MDE node 104
hash bucket 12
```

Correctness compares the first.

Performance measures can inspect the second.

---

# 37. Canonical Result

A `CanonicalResult` is the representation-independent form used for comparison.

Conceptually:

```text
CanonicalResult
    ├── canonical pointer relations
    ├── canonical access paths
    └── canonical heap/reference relations
```

It must not contain:

```text
MDE-specific IDs
memory addresses
allocation order
hash-table positions
```

---

# 38. Result Normalization Pipeline

For every configuration:

```text
Raw Internal State
       |
       v
Extract Semantic Facts
       |
       v
Canonicalize
       |
       v
Sort / Normalize
       |
       v
Canonical Result
```

Then:

```text
A ↔ B ↔ C
```

can be compared.

---

# 39. What Exactly Are We Comparing?

The primary semantic comparison is:

```text
B = Liveness + HRA
C = Liveness + HRA + MDE
```

Compare:

```text
CanonicalResult(B)
        ==
CanonicalResult(C)
```

This answers:

> Did MDE change the analysis result?

---

# 40. Secondary Comparison

Also compare:

```text
A = HRA
B = Liveness + HRA
```

This answers:

> What changed when liveness pruning was introduced?

And:

```text
A = HRA
C = Liveness + HRA + MDE
```

This gives the overall effect.

---

# 41. Comparison Visualization

The project should visualize the three configurations as **parallel analysis pipelines**.

Recommended conceptual view:

```text
                         SAME INPUT
                             |
          +------------------+------------------+
          |                  |                  |
          v                  v                  v
       CONFIG A           CONFIG B           CONFIG C
        HRA               Liveness             Liveness
          |                   |                  |
          v                   v                  v
       HRA State             HRA              HRA
          |                   |                  |
          v                   v                  v
        Solver            HRA State          HRA State
          |                   |                  |
          v                   |                  v
       Result                 |                 MDE
                              |                  |
                              v                  v
                           Solver             Solver
                              |                  |
                              v                  v
                           Result             Result
          |                   |                  |
          +-------------------+------------------+
                              |
                              v
                     RESULT NORMALIZATION
                              |
                              v
                    SEMANTIC COMPARISON
```

This is the best primary visualization because it makes the experimental isolation obvious.

---

# 42. Comparison Visualization — What Changes?

The comparison should highlight only the additional components:

```text
A → HRA
B → Liveness + HRA
C → Liveness + HRA + MDE
```

Therefore:

```text
A → B
    added:
    Liveness

B → C
    added:
    MDE
```

This gives a direct visual explanation of the experiment.

---

# 43. Semantic Comparison Visualization

A second visualization should show:

```text
                 Canonical Result
                        |
        +---------------+---------------+
        |               |               |
        v               v               v
        A               B               C
       HRA        Liveness+HRA    Liveness+HRA+MDE
        |               |               |
        +---------------+---------------+
                        |
                        v
                  Equality Check
```

The equality result should be represented as:

```text
PASS
```

or:

```text
MISMATCH
```

rather than using performance values as a proxy for correctness.

---

# 44. Structural Visualization

A third useful visualization is an example showing the effect of deduplication.

Before MDE:

```text
Root p
 |
 +-- next --> N1
               |
               +-- data --> D1

Root q
 |
 +-- next --> N2
               |
               +-- data --> D2
```

After MDE:

```text
Root p ----+
           |
Root q ----+----> Shared next/data structure
```

The actual sharing pattern depends on the MDE implementation.

This diagram is explanatory, not a claim about the exact final internal representation.

---

# 45. Important Visualization Rule

Do not visualize MDE as simply:

```text
fewer nodes = correct
```

Node reduction is a performance/representation metric.

Correctness must be shown separately:

```text
Result equivalence
```

---

# 46. Metric Model

The implementation should collect at least:

```text
runtime
peak memory
nodes/structures constructed
nodes/structures reused
deduplication ratio
correctness/equivalence
```

---

# 47. Runtime Metric

For configuration `X`:

```text
Runtime(X)
```

Record at least:

```text
total analysis time
```

Preferably also:

```text
frontend time
CFG time
liveness time
HRA time
MDE time
solver time
normalization time
```

if the measurement infrastructure allows it.

---

# 48. Peak Memory Metric

Record:

```text
PeakMemory(X)
```

under a consistent measurement methodology.

The comparison should use the same machine/environment for all configurations where possible.

---

# 49. Construction Metric

Define:

```text
Constructed
```

as the number of relevant representation creation attempts/events according to the chosen instrumentation point.

The exact event must be fixed before final experiments.

---

# 50. Reuse Metric

Define:

```text
Reused
```

as the number of representation requests satisfied by an already existing equivalent structure.

Again, the exact event definition must be fixed before final experiments.

---

# 51. Deduplication Ratio

The proposed metric is:

```text
DedupRatio =
    (Constructed - Reused)
    /
    Constructed
```

For example, if:

```text
Constructed = 100
Reused = 40
```

then:

```text
DedupRatio = 0.60
```

or:

```text
60%
```

The denominator must never be zero without explicit handling.

If:

```text
Constructed = 0
```

report:

```text
N/A
```

rather than:

```text
NaN
```

---

# 52. Logical Result Equality

Correctness is represented separately from performance.

Conceptually:

```text
Equivalent =
    CanonicalResult(B) == CanonicalResult(C)
```

Possible output:

```text
PASS
```

or:

```text
FAIL
```

For failure, the system should report the first detected semantic difference if possible.

---

# 53. Difference Report

When:

```text
Result(B) != Result(C)
```

the comparison framework should report something like:

```text
Missing in C:
    p.next.data → H2

Extra in C:
    p.next.data → H3
```

or the equivalent canonical difference.

This is far more useful than simply printing:

```text
false
```

---

# 54. Comparison Table

For each benchmark, the final experiment table should conceptually contain:

| Benchmark | Config | Runtime | Peak Memory | Constructed | Reused | Dedup Ratio | Result |
|---|---|---:|---:|---:|---:|---:|---|
| P1 | A | ... | ... | ... | ... | N/A | reference |
| P1 | B | ... | ... | ... | ... | N/A | reference |
| P1 | C | ... | ... | ... | ... | ... | PASS |
| P2 | A | ... | ... | ... | ... | N/A | reference |
| P2 | B | ... | ... | ... | ... | N/A | reference |
| P2 | C | ... | ... | ... | ... | ... | PASS |

The actual values will come only from experiments.

---

# 55. Relative Performance Metrics

After raw data is collected, derived metrics may be calculated.

For example:

```text
Runtime Improvement =
    (Runtime(B) - Runtime(C))
    /
    Runtime(B)
```

and:

```text
Memory Improvement =
    (Memory(B) - Memory(C))
    /
    Memory(B)
```

These are derived research metrics and must be clearly labeled as such.

No performance claim should be hard-coded into the implementation.

---

# 56. Baseline Choice

For the main MDE performance comparison:

```text
Baseline = B
```

because:

```text
B = Liveness + HRA
C = Liveness + HRA + MDE
```

This keeps liveness constant.

---

# 57. Why Not Compare Only A vs C?

Because:

```text
A → C
```

changes two things:

```text
Liveness
MDE
```

Therefore an improvement cannot be attributed specifically to MDE.

The primary comparison must be:

```text
B vs C
```

---

# 58. MDE Expected Effect

The design hypothesis is:

```text
Repeated structural information
        ↓
MDE sharing
        ↓
fewer physical structures
        ↓
potentially lower memory
        ↓
potentially lower processing cost
```

The word **potentially** is important.

The implementation must measure the actual effect.

---

# 59. Liveness Expected Effect

The design hypothesis is:

```text
Dead pointer information
        ↓
pruned
        ↓
less information reaches HRA
        ↓
potentially smaller analysis state
```

Again, this must be experimentally verified.

---

# 60. Three-Level Comparison Model

The project should visualize results at three levels.

## Level 1 — Semantic

```text
Do results match?
```

## Level 2 — Structural

```text
How much representation was constructed/reused?
```

## Level 3 — System

```text
How much runtime/memory was required?
```

Therefore:

```text
Correctness
    ↓
Structure
    ↓
Performance
```

This hierarchy should be maintained in reporting.

---

# 61. Detailed Interface: Liveness → HRA

Conceptual contract:

```text
Input:
    ProgramPoint

Output:
    LiveSet
```

Example:

```text
liveSet = liveness.getLiveIn(point)
```

HRA uses this information to determine whether relevant pointer information should be propagated.

The exact pruning rule must be defined according to the chosen HRA/liveness formulation.

---

# 62. Detailed Interface: HRA → MDE

Conceptual contract:

```text
Input:
    HRA structural unit

Output:
    MDE representation handle
```

The adapter owns translation.

HRA does not need to know:

```text
hash bucket
internal MDE node ID
storage layout
```

---

# 63. Detailed Interface: Solver → Result

Conceptual contract:

```text
Input:
    stable analysis state

Output:
    logical analysis result
```

The result extractor must expose semantic facts independent of representation.

---

# 64. Detailed Interface: Result → Comparison

Conceptual contract:

```text
Input:
    raw result

Output:
    canonical result
```

Then:

```text
compare(canonicalB, canonicalC)
```

returns:

```text
equivalent
+
optional difference report
```

---

# 65. Detailed Interface: Instrumentation

Conceptual contract:

```text
record(event)
```

Possible events:

```text
HRA_NODE_CREATE
HRA_EDGE_CREATE
MDE_LOOKUP
MDE_CREATE
MDE_REUSE
STATE_MERGE
SOLVER_ITERATION
```

The final event vocabulary should be frozen when instrumentation is implemented.

---

# 66. Ownership Rules

## Liveness owns

```text
LiveSet
LivenessInfo
```

## HRA owns

```text
HRAState
HRA graph
semantic access-path relationships
```

## MDE owns

```text
deduplicated structural representation
```

## Solver owns

```text
worklist
fixpoint iteration
```

## Result layer owns

```text
canonicalization
comparison
difference reporting
```

## Instrumentation owns

```text
metrics
events
measurement output
```

---

# 67. Mutability Rules

Prefer immutable or controlled-mutation representations for semantic objects where practical.

Particularly:

```text
AccessPath
ProgramPoint
canonical result
```

should be safe to use as keys.

Mutable objects used as hash keys can create subtle correctness bugs.

---

# 68. Hashing Rules

Any object used as a key must have consistent:

```text
equals()
hash()
```

semantics.

For:

```text
AccessPath
```

hashing should depend on:

```text
root
+
ordered field sequence
```

For MDE structural keys, hashing must match the structural equality definition.

---

# 69. Canonical Ordering

When results are serialized or compared:

- sort pointer names if appropriate;
- sort field sequences deterministically;
- sort abstract object identifiers after canonicalization;
- remove representation-specific IDs.

The exact canonical ordering must be deterministic.

---

# 70. Example End-to-End Case

Consider:

```c
struct Node {
    struct Node *next;
    int data;
};

void f(struct Node *p) {
    struct Node *q = p;
    if (q != 0) {
        q = q->next;
        use(q->data);
    }
}
```

Conceptually:

```text
p
 ↓
q
 ↓
q.next
 ↓
q.next.data
```

---

# 71. Example Liveness View

Near:

```c
use(q->data);
```

`q` is relevant because it is needed to reach:

```text
q->data
```

Earlier:

```c
q = p;
```

the liveness state depends on the complete backward analysis.

The exact live sets should be produced by the implementation, not manually assumed.

---

# 72. Example HRA View

HRA may construct a structure corresponding conceptually to:

```text
q
 |
 next
 |
 N1
 |
 data
 |
 V
```

The exact abstract heap representation depends on the HRA formulation.

---

# 73. Example MDE View

Suppose another pointer creates the same suffix:

```text
r.next.data
```

The representation contains repeated structure:

```text
next → data
```

MDE may reuse the existing equivalent structural representation.

The implementation must verify that this reuse preserves the logical result.

---

# 74. What We Visualize During Development

The implementation should provide debugging views at four levels.

## View 1 — CFG

```text
B1 → B2 → B3
```

## View 2 — Liveness

```text
Point P:
IN  = {p,q}
OUT = {q}
```

## View 3 — HRA

```text
p --next--> N1 --data--> V1
```

## View 4 — MDE

```text
p ----+
      +---- shared structure
q ----+
```

This allows developers to locate where behavior changes.

---

# 75. What We Visualize During Experiments

The final research presentation should use:

### Visualization 1

Three configuration pipelines:

```text
A | B | C
```

### Visualization 2

Runtime comparison.

### Visualization 3

Peak memory comparison.

### Visualization 4

Constructed vs reused structures.

### Visualization 5

Deduplication ratio.

### Visualization 6

Correctness/equivalence status.

The exact charts are generated from measured data.

---

# 76. Runtime Visualization

For each benchmark:

```text
          Runtime
             |
             |     █
             | █   █
             | █ █ █
             +---------
               A B C
```

The actual chart should use measured values.

A grouped bar chart is appropriate for discrete configuration comparison.

---

# 77. Memory Visualization

Similarly:

```text
       Peak Memory
             |
             | █
             | █ █
             | █ █ █
             +---------
               A B C
```

This shows whether MDE reduces physical memory.

---

# 78. Deduplication Visualization

For MDE-enabled configuration:

```text
Constructed
████████████████████

Reused
████████
```

The exact visualization should use:

```text
Constructed
Reused
DedupRatio
```

and not imply that all constructed nodes are semantically unique unless that is exactly how the instrumentation defines them.

---

# 79. Correctness Visualization

Correctness should be represented independently:

```text
Benchmark  Result
P1         PASS
P2         PASS
P3         PASS
P4         PASS
```

If a mismatch occurs:

```text
P5         FAIL
```

with a linked/detailed difference report.

---

# 80. Visualization Rule for Research Claims

Do not visually combine:

```text
correctness
+
runtime
+
memory
```

into one ambiguous score.

Keep them separate.

A faster result is not automatically more correct.

A smaller graph is not automatically semantically equivalent.

---

# 81. Design Invariants

The implementation must preserve:

### Invariant 1

```text
AccessPath equality is semantic.
```

### Invariant 2

```text
MDE IDs do not define semantic equality.
```

### Invariant 3

```text
Liveness is computed from program structure, not runtime execution.
```

### Invariant 4

```text
MDE must not silently remove semantic HRA information.
```

### Invariant 5

```text
Canonical results are representation-independent.
```

### Invariant 6

```text
Configuration A/B/C use the same input program.
```

### Invariant 7

```text
B and C differ primarily by MDE.
```

### Invariant 8

```text
Instrumentation must not change semantic analysis.
```

---

# 82. Error Cases

The design should explicitly handle:

```text
empty program
unsupported statement
unsupported field
missing CFG successor
invalid access path
invalid MDE key
duplicate semantic fact
result normalization failure
zero construction count
non-converging analysis
```

Each should produce an explicit diagnostic rather than silent corruption.

---

# 83. Non-Convergence

The solver must have a defensible convergence strategy.

If the analysis fails to converge within a configured safety bound:

```text
Analysis did not converge
```

should be reported as an analysis failure.

Do not silently return a partial result and label it successful.

---

# 84. Unsupported Feature Reporting

For a benchmark containing unsupported constructs:

```text
Benchmark: X
Status: UNSUPPORTED
Reason:
    pointer arithmetic
```

rather than:

```text
Benchmark: X
Result: PASS
```

when the analysis did not actually support the program.

---

# 85. Design for Future Extensions

The design should permit future additions such as:

```text
more fields
more heap shapes
additional pointer operations
context sensitivity
additional deduplication strategies
```

but these are not part of the initial implementation unless explicitly added to scope.

The architecture should be extensible without forcing the first version to implement them.

---

# 86. Minimum First Implementation

The first technically complete implementation should aim for:

```text
C input
  ↓
CFG
  ↓
Liveness
  ↓
Restricted HRA
  ↓
Restricted access-path representation
  ↓
MDE adapter
  ↓
MDE representation
  ↓
Fixpoint
  ↓
Canonical result
```

with:

```text
A
B
C
```

configurations and correctness comparison.

---

# 87. Design Completion Criteria

The technical design is considered sufficiently defined when:

- [ ] program-point meaning is fixed;
- [ ] liveness state is defined;
- [ ] access-path representation is defined;
- [ ] HRA state is defined;
- [ ] HRA graph semantics are defined;
- [ ] MDE boundary is defined;
- [ ] adapter responsibility is defined;
- [ ] propagation model is defined;
- [ ] fixpoint behavior is defined;
- [ ] canonical result is defined;
- [ ] semantic equality is defined;
- [ ] configuration A/B/C are defined;
- [ ] metric definitions are documented;
- [ ] visualization strategy is documented;
- [ ] failure behavior is documented;
- [ ] ownership boundaries are documented.

---

# 88. Final Technical Model

The complete technical relationship is:

```text
                 PROGRAM
                    |
                    v
             +-------------+
             | ProgramPoint|
             +-------------+
                    |
                    v
             +-------------+
             | CFG + Facts |
             +-------------+
                    |
                    v
             +-------------+
             |  Liveness   |
             +-------------+
                    |
                    v
               LiveSet
                    |
                    v
             +-------------+
             |     HRA     |
             +-------------+
                    |
                    v
             +-------------+
             | AccessPath  |
             | Heap State  |
             +-------------+
                    |
                    v
             +-------------+
             | HRA Graph   |
             +-------------+
                    |
                    v
             +-------------+
             | MDE Adapter |
             +-------------+
                    |
                    v
             +-------------+
             | MDE / LHF   |
             +-------------+
                    |
                    v
             +-------------+
             |   Solver    |
             +-------------+
                    |
                    v
             +-------------+
             | Fixpoint    |
             +-------------+
                    |
                    v
             +-------------+
             | Raw Result  |
             +-------------+
                    |
                    v
             +-------------+
             | Canonicalize|
             +-------------+
                    |
                    v
             +-------------+
             | Compare     |
             +-------------+
                    |
          +---------+---------+
          |         |         |
          v         v         v
       Runtime    Memory   Dedup/Reuse
```

---

# 89. Core Comparison Model

The most important research comparison is:

```text
                 SAME PROGRAM
                      |
          +-----------+-----------+
          |                       |
          v                       v
        CONFIG B                CONFIG C
          |                       |
          v                       v
     Liveness + HRA        Liveness + HRA
                                  +
                                 MDE
          |                       |
          v                       v
   Canonical Result B      Canonical Result C
          |                       |
          +-----------+-----------+
                      |
                      v
              Semantic Equality
                      |
             +--------+--------+
             |                 |
            PASS              FAIL
             |                 |
             v                 v
        Measure/Report    Debug Difference
```

This is the central technical comparison of the project.

---

# 90. Final Design Principle

The implementation must preserve this hierarchy:

```text
                SEMANTICS
                   ↑
                   |
        +----------+----------+
        |                     |
     Liveness                HRA
        |                     |
        +----------+----------+
                   |
                   v
              REPRESENTATION
                   |
                  MDE
                   |
                   v
              PERFORMANCE
```

The project is successful only if the bottom layer improves scalability **without breaking the semantic layer above it**.
