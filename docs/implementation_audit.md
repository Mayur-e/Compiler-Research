# MDE-HRA Implementation and Correctness Audit

## 1. Executive Summary

This audit documents the research-grade engineering upgrade of the **MDE-HRA** (*Canonical State Sharing and Liveness-Guided Pruning in Heap Reference Analysis*) C++ static analysis framework.

### Experimental Configurations Under Audit

The framework evaluates four distinct configurations of Heap Reference Analysis (HRA):

- **Configuration A (Baseline HRA):** Standard iterative fixed-point HRA without liveness-guided pruning or state memoization.
- **Configuration B (Liveness-Guided HRA):** Backward flow-sensitive pointer liveness analysis combined with HRA, eagerly pruning dead pointer variables from the abstract state during transfer functions.
- **Configuration C (Proposed MDE-HRA):** Backward liveness analysis + HRA + Memoization/Deduplication Engine (MDE) canonical state sharing.
- **Configuration D (Ablation: HRA + MDE):** Baseline HRA coupled with MDE state deduplication in the absence of liveness pruning, isolating the direct impact of liveness-guided state reduction on memoization efficiency.

### Core Semantic Invariants

To guarantee that deduplication and pruning optimize memory representation without altering analysis semantics, two dual invariants are enforced and verified on every analyzed program:

1. **Primary Soundness Invariant:**
   $$\text{CanonicalResult}(B) \equiv \text{CanonicalResult}(C)$$
   *Evaluated over variables live at function exit. MDE canonical state sharing must produce an identical normalized heap graph to un-memoized liveness-guided analysis.*

2. **Ablation Invariant:**
   $$\text{CanonicalResult}(A) \equiv \text{CanonicalResult}(D)$$
   *Evaluated over all program variables. MDE state deduplication on unpruned states must produce an identical normalized heap graph to the baseline solver.*

---

## 2. Clang AST / CFG Frontend Audit & Modernization

### 2.1 Previous Limitations Identified

Prior to this upgrade, the Clang frontend in [ClangFrontend.cpp](file:///d:/TS/Seminar/Project/src/frontend/ClangFrontend.cpp) exhibited several critical defects that compromised research-grade evaluation:
1. **Silent Dropping of Constructs:** Unsupported constructs (such as pointer arithmetic or array subscripts) were silently skipped without diagnostics, producing artificially empty IR graphs that gave false-positive "success" runs.
2. **Missing Statement Support:**
   - `DeclStmt` with initializers (e.g., `struct Node* p = malloc(...);` or `struct Node* q = NULL;`) was unhandled.
   - Nested struct member accesses (e.g., `a->next = b->prev;`) were not decomposed into loads and stores.
   - Standard allocator `calloc` / `__builtin_calloc` was unrecognized.
   - Null pointer constants (`0`, `NULL`, `(void*)0`) were not converted into explicit null-binding or edge-clearing IR statements.
3. **Loss of Provenance:** Statements lacked line, column, and filename metadata.
4. **System Header Pollution:** Inclusion of `<stdlib.h>` pulled MinGW inline library functions into the user CFG, causing analysis noise.

### 2.2 Architectural Improvements

The frontend was refactored in [ClangFrontend.hpp](file:///d:/TS/Seminar/Project/include/mdehra/frontend/ClangFrontend.hpp) and [ClangFrontend.cpp](file:///d:/TS/Seminar/Project/src/frontend/ClangFrontend.cpp):

```
+--------------------------------------------------------------------------+
|                        Clang Frontend Architecture                       |
+--------------------------------------------------------------------------+
                                     |
                [Clang ASTContext & Clang CFG]
                                     |
                        +------------v-----------+
                        |   System Header Check  | --> Skip MinGW/UCRT inlines
                        +------------+-----------+
                                     |
                        +------------v-----------+
                        |  CFG Statement Visitor |
                        +------------+-----------+
                                     |
       +-----------------------------+-----------------------------+
       |                             |                             |
 [Supported IR]            [Unsupported Pointer]         [Scalar / Ignored]
 - AllocStmt               - Pointer Arithmetic          - Scalar assign
 - AssignStmt (Null-aware) - Array Subscript             - Scalar return
 - LoadStmt (Decomposed)   - Unmodeled Calls             - Diagnostic: Info
 - StoreStmt (Edge-clear)  - Multi-level Deref                     |
 - ReturnStmt (Pointer)              |                             v
 - CallStmt ("free")                 v                       (Retained/Skipped)
       |                   +-------------------+
       |                   | Diagnostic Emit   |
       |                   +---------+---------+
       |                             |
       |                   +---------v---------+
       |                   |  Strict Mode?     |
       |                   +----+---------+----+
       |                     Yes|         |No
       |                        v         v
       |                  [Exit Code 2] [Permissive Mode]
       |                  UNSUPPORTED   isComplete = false
       +------------------------+---------+
                                |
                                v
                     [TranslationResult IR]
```

#### Feature Coverage & Diagnostics Matrix

| Construct | Example Code | Translated IR Statement | Severity | Diagnostic ConstructType |
|:---|:---|:---|:---:|:---|
| **Pointer Alloc (malloc)** | `p = malloc(8);` | `AllocStmt(p, site)` | `Info` | `ModeledAllocation` |
| **Pointer Alloc (calloc)** | `p = calloc(1, 8);` | `AllocStmt(p, site)` | `Info` | `ModeledAllocation` |
| **Pointer Init in Decl** | `struct Node* p = q;` | `AssignStmt(p, q)` | `Info` | `ModeledDeclaration` |
| **Null Assignment** | `p = NULL;` / `p = 0;` | `AssignStmt::createNull(p)` | `Info` | `ModeledNullAssign` |
| **Field Store** | `p->next = q;` | `StoreStmt(p, next, q)` | `Info` | `ModeledStore` |
| **Null Field Store** | `p->next = NULL;` | `StoreStmt::createNull(p, next)` | `Info` | `ModeledStore` |
| **Nested Field Store** | `a->next = b->prev;` | `LoadStmt(tmp, b, prev); StoreStmt(a, next, tmp)` | `Info` | `DecomposedNestedStore` |
| **Deallocation Call** | `free(p);` | `CallStmt("free", [p])` | `Info` | `ModeledDealloc` |
| **Pointer Arithmetic** | `q = p + 1;` | *None* | `Unsupported` | `PointerArithmetic` |
| **Array Subscripting** | `q = arr[i];` | *None* | `Unsupported` | `ArraySubscript` |
| **Unmodeled Call** | `printf("...");` | *None* | `Unsupported` | `UnmodeledFunctionCall` |
| **Multi-Level Deref** | `**p = q;` | *None* | `Unsupported` | `MultiLevelPointerStore` |

---

## 3. Solver & CFG Multi-Exit Normalization

### 3.1 Multi-Exit CFG Resolution

In real-world C programs and benchmarks (such as `bm05_branch_alloc.c` or functions with early returns), CFGs contain multiple terminal blocks (`successors.empty()`).

- **Previous Defect:** Exit states were sampled from an arbitrary single exit block, ignoring paths that exited through alternate returns.
- **Resolution:** Added [HRASolver::getFunctionExitState(func)](file:///d:/TS/Seminar/Project/src/solver/HRASolver.cpp):
  ```cpp
  std::shared_ptr<const hra::HRAState> HRASolver::getFunctionExitState(const ir::Function& func) const {
      std::shared_ptr<hra::HRAState> merged = nullptr;
      for (const auto& kv : func.blocks) {
          if (kv.second.successors.empty()) {
              auto it = blockExitStates.find(kv.first);
              if (it != blockExitStates.end() && it->second) {
                  if (!merged) merged = std::make_shared<hra::HRAState>(*it->second);
                  else merged->joinWith(*it->second);
              }
          }
      }
      return merged;
  }
  ```
- **Liveness Propagation:** [LivenessSolver::run()](file:///d:/TS/Seminar/Project/src/liveness/LivenessSolver.cpp) was updated to seed returned variables into `liveOut` across all exit blocks.

### 3.2 Null Store Edge Removal

To properly model memory overwrites with `NULL`:
- [HRAGraph::removeEdges(src, field)](file:///d:/TS/Seminar/Project/include/mdehra/hra/HRAGraph.hpp) was introduced.
- When `StoreStmt::isNull` is evaluated in transfer functions, outgoing edges from all nodes in the source pointer's points-to set are removed from the abstract heap graph, correctly destroying aliases.

---

## 4. MDE Canonicalization Optimization & State Accounting

### 4.1 Structural Fingerprinting & Deep Collision Checking

In [MDEGuidedHRAConfig.cpp](file:///d:/TS/Seminar/Project/src/mde/MDEGuidedHRAConfig.cpp):
- A 64-bit FNV-1a structural fingerprint is computed over bindings and graph edges.
- On fingerprint match, states in the hash bucket are checked via **deep structural equality** (`*entry.first == state`).
- States are **never** conflated based on hash equality alone; collision buckets resolve any hash collisions deterministically.

### 4.2 Exact Memory and Timing Accounting

- **Separate Timing:** Wall-clock canonicalization time is measured via `std::chrono::high_resolution_clock` and stored in `stats.canonicalizeTimeMs`, completely separated from solver transfer execution time.
- **Retained State Memory Accounting:** [MDEGuidedHRAConfig::getRetainedStateBytes()](file:///d:/TS/Seminar/Project/src/mde/MDEGuidedHRAConfig.cpp) computes exact analyzer-owned state memory in bytes:
  $$\text{RetainedBytes} = \sum_{s \in \mathcal{S}} \left( \text{sizeof}(s) + \text{bindings\_bytes}(s) + \text{graph\_bytes}(s) \right)$$
- Empirically confirms that Config C retains significantly fewer state bytes than Config D because dead pointer variables are pruned prior to state canonicalization.

---

## 5. Unified Research Driver (`mdehra-driver`)

The project executable target was upgraded in [src/main.cpp](file:///d:/TS/Seminar/Project/src/main.cpp) to `mdehra-driver`:

- **Deterministic SHA-256:** Computes exact 256-bit hash of original source file bytes.
- **Command-line Interface:**
  - `--config <A|B|C|D|ALL>`: Selects specific experimental configuration or runs all 4.
  - `--strict`: Fails with exit code 2 if any unsupported construct is encountered.
  - `--permissive`: Allows partial translation with warnings.
  - `--json <path>`: Emits comprehensive structured JSON output with escaped forward-slash paths.
  - `--reps <n>`: Runs solver repetitions for stable microbenchmarking.
  - `--explain`: Dumps full diagnostic messages.
- **Exit Codes:**
  - `0`: Analysis completed successfully (`OK`).
  - `2`: Analysis rejected unsupported constructs in strict mode (`UNSUPPORTED`).
  - `1`: Input/runtime error (`ERROR`).

---

## 6. Audit Verification Summary

| Verification Target | Test Suite | Result |
|:---|:---|:---:|
| Smoke tests & Config distinctness | `Phase1_SmokeTest_Config` | **PASS** |
| Clang integration probe | `Phase2A_ClangProbe` | **PASS** |
| Custom IR structures | `Phase2B_IRTest` | **PASS** |
| Clang frontend AST translation | `Phase2B_FrontendTest` | **PASS** |
| Baseline HRA worklist & transfers | `Phase3_HRATest` | **PASS** |
| Flow-sensitive pointer liveness | `Phase4_LivenessTest` | **PASS** |
| Integration & canonicalization | `Phase5_IntegrationTest` | **PASS** |
| MDE interning & deduplication | `Phase6_MDETest` | **PASS** |
| MDE-HRA state adapter | `Phase7_AdapterTest` | **PASS** |
| Config C solver loop | `Phase8_ConfigCTest` | **PASS** |
| E2E correctness pipeline (Phase 9) | `Phase9_E2ECorrectnessTest` | **PASS** |
| Strict mode rejections & diagnostics | `Upgrade_FrontendDiagnosticsTest` | **PASS** |
| 4-Configuration semantics & dual invariants | `Upgrade_SemanticsValidationTest` | **PASS** |
| Evaluation pack synthetic benchmarks | 4 / 4 Benchmarks in Strict Mode | **PASS** |
| Project standard benchmarks | BM01 - BM08 in Strict Mode | **PASS** |
| Controlled scaling runner (N=8..64) | 80 / 80 Runs (5 reps x 4 configs) | **PASS** |
