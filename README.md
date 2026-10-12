# MDE-HRA

**Deduplication-Accelerated Heap Reference Analysis for Scalable, Liveness-Pruned Pointer Analysis**

---

## Research Objective

This project investigates whether **structural deduplication using MDE / LatticeHashForest** can make **liveness-pruned Heap Reference Analysis (HRA)** more scalable while preserving the analysis result.

The system implements three experimental configurations evaluated on a synthetic benchmark corpus:

| Label | Configuration | Purpose |
|---|---|---|
| **A** | HRA | Baseline heap reference analysis |
| **B** | Liveness + HRA | Liveness-pruned baseline |
| **C** | Liveness + HRA + MDE | Full proposed system |

The primary correctness requirement is:

```
CanonicalResult(B) == CanonicalResult(C)
```

The primary performance comparison is **B vs C**, which isolates the effect of MDE structural deduplication.

---

## Current Implementation Status

**All phases complete (Phase 1 – Phase 13.2).**

| Phase | Description | Status |
|---|---|---|
| 0 | Repository & Environment | ✅ COMPLETE |
| 1 | C++/CMake Foundation | ✅ COMPLETE |
| 1.5 | Technical Foundation | ✅ COMPLETE |
| 2A | Clang/LLVM Frontend | ✅ COMPLETE |
| 2B | Custom High-Level IR | ✅ COMPLETE |
| 3 | Baseline HRA (Config A) | ✅ COMPLETE |
| 4 | Independent Backward Liveness | ✅ COMPLETE |
| 5 | Liveness + HRA (Config B) | ✅ COMPLETE |
| 6 | MDE Investigation | ✅ COMPLETE |
| 7 | HRA → MDE Adapter | ✅ COMPLETE |
| 7.1 | Research Correctness Audit | ✅ COMPLETE |
| 8 | Configuration C | ✅ COMPLETE |
| 8.1 | Configuration C Audit | ✅ COMPLETE |
| 9 | End-to-End Correctness | ✅ COMPLETE |
| 10 | Experimental Instrumentation | ✅ COMPLETE |
| 11 | Benchmark Campaign (BM01–BM08) | ✅ COMPLETE |
| 11.1 | Genuine Loop Benchmark (BM08) | ✅ COMPLETE |
| 12 | Statistical Analysis (A vs B vs C) | ✅ COMPLETE |
| 12.1 | Correctness Evidence Audit | ✅ COMPLETE |
| 13 | Live Execution & Visualization | ✅ COMPLETE |
| 13.1 | Developer View Refinement | ✅ COMPLETE |
| 13.2 | Developer View Interaction Refinement | ✅ COMPLETE |

---

## Repository Structure

```
Project/
│
├── CMakeLists.txt               ← top-level CMake configuration
├── README.md                    ← this file
├── .gitignore
│
├── include/mdehra/              ← public headers
│   ├── config.hpp               ← project-level configuration, version constants
│   ├── ir/                      ← custom high-level IR
│   │   ├── Program.hpp          ← Function / Block / Statement definitions
│   │   ├── Statement.hpp        ← Alloc, Assign, Load, Store, Return
│   │   ├── Variable.hpp, Field.hpp
│   │   ├── Printer.hpp          ← IR statement printer
│   │   └── ProgramPoint.hpp
│   ├── frontend/
│   │   └── ClangFrontend.hpp    ← Clang LibTooling → custom IR bridge
│   ├── hra/
│   │   ├── HRAState.hpp         ← Abstract heap graph (nodes, bindings, edges)
│   │   └── HRATransfer.hpp      ← Transfer functions (alloc/assign/load/store)
│   ├── liveness/
│   │   ├── LivenessSolver.hpp   ← Backward dataflow liveness solver
│   │   └── LivenessResult.hpp   ← Per-statement liveBefore/liveAfter maps
│   ├── solver/
│   │   ├── HRAConfig.hpp        ← Configuration policy interface + trace hooks
│   │   └── HRASolver.hpp        ← Worklist fixpoint solver
│   └── mde/
│       ├── HRAEncoder.hpp       ← HRAState → MDE structural index encoder
│       └── MDEGuidedHRAConfig.hpp  ← Configuration C: liveness + structural dedup
│
├── src/                         ← implementation files
│   ├── CMakeLists.txt
│   ├── main.cpp                 ← basic command-line driver
│   ├── instrumentation.cpp      ← instr-driver: runs A/B/C and emits CSV metrics
│   ├── trace_driver.cpp         ← trace-driver: runs A/B/C and emits JSON trace
│   ├── ir_dump.cpp              ← ir-dump-driver: prints block/CFG structure
│   ├── frontend/ClangFrontend.cpp
│   ├── hra/Printer.cpp
│   ├── ir/Printer.cpp
│   ├── liveness/
│   │   ├── LivenessSolver.cpp
│   │   └── Printer.cpp
│   ├── solver/
│   │   ├── HRASolver.cpp
│   │   └── HRAConfig.cpp
│   └── mde/
│       ├── HRAEncoder.cpp
│       └── MDEGuidedHRAConfig.cpp
│
├── mde_src/src/include/mde/
│   └── mde.hpp                  ← LatticeHashForest / MDE header-only library
│
├── tests/                       ← automated CTest suite
│   ├── CMakeLists.txt
│   ├── test_config.cpp          ← Phase 1 smoke test (config constants)
│   ├── clang_probe.cpp          ← Phase 2A: Clang/LLVM integration probe
│   ├── ir_test.cpp              ← Phase 2B: IR construction test
│   ├── frontend_test.cpp        ← Phase 2B: ClangFrontend → IR round-trip
│   ├── hra_test.cpp             ← Phase 3: HRA transfer function correctness
│   ├── liveness_test.cpp        ← Phase 4: backward liveness solver correctness
│   ├── integration_test.cpp     ← Phase 5: Liveness + HRA integration (Config B)
│   ├── mde_test.cpp             ← Phase 6: MDE/LatticeHashForest unit tests
│   ├── hra_mde_adapter_test.cpp ← Phase 7: HRAEncoder + MDEGuidedHRAConfig
│   ├── config_c_test.cpp        ← Phase 8: Config C correctness and canonicalization
│   └── e2e_correctness_test.cpp ← Phase 9: CanonicalResult(B) == CanonicalResult(C)
│
├── experiments/
│   ├── benchmarks/              ← synthetic C benchmark programs (BM01–BM08)
│   │   ├── bm01_simple_alloc.c         ← simple allocation chain
│   │   ├── bm02_linked_list_build.c    ← linked list construction
│   │   ├── bm03_dead_pointers.c        ← dead pointer pruning target
│   │   ├── bm04_aliasing.c             ← pointer aliasing
│   │   ├── bm05_branch_alloc.c         ← conditional allocation
│   │   ├── bm06_loop_list.c            ← field-load chain (unrolled)
│   │   ├── bm07_multi_field_graph.c    ← multi-field heap graph
│   │   └── bm08_genuine_loop.c         ← genuine while loop (real CFG back-edge)
│   ├── raw/                     ← raw CSV campaign results
│   ├── analysis/                ← statistical analysis outputs
│   ├── logs/                    ← per-benchmark run logs
│   └── metadata/                ← benchmark metadata
│
├── visualization/               ← live execution & trace visualizer
│   ├── index.html               ← developer dashboard UI
│   ├── app.js                   ← trace renderer and panel synchronization engine
│   ├── BM03_A_trace.json        ← pre-generated trace: BM03 Config A
│   ├── BM03_B_trace.json        ← pre-generated trace: BM03 Config B
│   ├── BM03_C_trace.json        ← pre-generated trace: BM03 Config C
│   ├── BM08_A_trace.json        ← pre-generated trace: BM08 Config A
│   ├── BM08_B_trace.json        ← pre-generated trace: BM08 Config B
│   └── BM08_C_trace.json        ← pre-generated trace: BM08 Config C
│
├── scripts/                     ← automation scripts
│   └── run_benchmarks.sh        ← benchmark campaign runner
│
└── docs/                        ← research documentation
    ├── project.md               ← implementation plan and phase roadmap
    ├── architecture.md          ← system architecture
    ├── design.md                ← technical design and data model
    ├── development.md           ← development workflow
    ├── decisions.md             ← architecture decision log
    ├── benchmarks.md            ← benchmark strategy
    ├── experiments.md           ← experimental methodology
    └── testing.md               ← testing strategy
```

---

## Prerequisites

**Compiler & Build Tools:**
- C++17 compiler: `g++` (GCC 13+ via MSYS2 UCRT64)
- CMake ≥ 3.20
- Ninja build system

**LLVM/Clang Libraries** (for Clang LibTooling frontend):
- `clangTooling`, `clangFrontend`, `clangAST`, `clangSema`, `clangParse`, `clangLex`, `clangBasic`

**Runtime DLLs** (Windows):
- LLVM runtime DLLs must be on `PATH`: `D:\msys64\ucrt64\bin`

### MSYS2 UCRT64 Installation

Install MSYS2 from [msys2.org](https://www.msys2.org/), then in the **MSYS2 UCRT64** terminal:

```bash
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-llvm \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja
```

---

## Build Instructions

All commands are run from the repository root (`d:\TS\Seminar\Project\`).

### 1. Configure

```powershell
cmake -S . -B build -G Ninja
```

### 2. Build all targets

```powershell
cmake --build build
```

This produces the following executables under `build/src/`:

| Executable | Purpose |
|---|---|
| `mdehra-driver.exe` | Basic command-line driver |
| `instr-driver.exe` | Benchmark campaign runner — emits CSV metrics |
| `trace-driver.exe` | Trace-mode runner — emits structured JSON execution trace |
| `ir-dump-driver.exe` | IR/CFG diagnostic — prints block structure and edges |

### 3. Build a specific target

```powershell
cmake --build build --target trace-driver
cmake --build build --target instr-driver
```

### 4. Clean rebuild

```powershell
cmake --build build --clean-first
```

---

## Running the Analyses

All commands require the LLVM runtime on `PATH`:

```powershell
$env:PATH = "D:\msys64\ucrt64\bin;$env:PATH"
```

### Instrumentation driver (benchmark campaign)

```powershell
.\build\src\instr-driver.exe <source.c> <A|B|C>
```

Example — run BM03 with all three configurations:

```powershell
.\build\src\instr-driver.exe experiments\benchmarks\bm03_dead_pointers.c A
.\build\src\instr-driver.exe experiments\benchmarks\bm03_dead_pointers.c B
.\build\src\instr-driver.exe experiments\benchmarks\bm03_dead_pointers.c C
```

### Trace driver (visualization)

```powershell
.\build\src\trace-driver.exe <source.c> <A|B|C> > output.json
```

Example — generate trace for BM08 Config C:

```powershell
.\build\src\trace-driver.exe experiments\benchmarks\bm08_genuine_loop.c C > visualization\BM08_C_trace.json
```

### IR dump (CFG diagnostic)

```powershell
.\build\src\ir-dump-driver.exe <source.c>
```

---

## Running the Test Suite

```powershell
ctest --test-dir build --output-on-failure
```

Verbose mode:

```powershell
ctest --test-dir build -V
```

### Test inventory

| CTest name | Source file | Phase | What it verifies |
|---|---|---|---|
| `Phase1_SmokeTest_Config` | `test_config.cpp` | 1 | Version constants, config names and labels (A/B/C) |
| `Phase2A_ClangProbe` | `clang_probe.cpp` | 2A | Clang/LLVM headers and toolchain integration |
| `Phase2B_IRTest` | `ir_test.cpp` | 2B | Custom IR construction (blocks, statements) |
| `Phase2B_FrontendTest` | `frontend_test.cpp` | 2B | ClangFrontend → custom IR translation |
| `Phase3_HRATest` | `hra_test.cpp` | 3 | HRA transfer functions (alloc/assign/load/store) |
| `Phase4_LivenessTest` | `liveness_test.cpp` | 4 | Backward liveness solver correctness |
| `Phase5_IntegrationTest` | `integration_test.cpp` | 5 | Liveness + HRA (Config B) integration |
| `Phase6_MDETest` | `mde_test.cpp` | 6 | MDE/LatticeHashForest structural encoding |
| `Phase7_AdapterTest` | `hra_mde_adapter_test.cpp` | 7 | HRAEncoder injectivity + MDEGuidedHRAConfig |
| `Phase8_ConfigCTest` | `config_c_test.cpp` | 8 | Config C canonicalization and pruning correctness |
| `Phase9_E2ECorrectnessTest` | `e2e_correctness_test.cpp` | 9 | `CanonicalResult(B) == CanonicalResult(C)` |

---

## Live Execution Visualizer

The `visualization/` directory contains a browser-based developer dashboard for inspecting real solver traces step-by-step.

### Launch

```powershell
cd visualization
python -m http.server 8000
```

Open `http://localhost:8000/` in your browser.

### Usage

1. Load one of the pre-generated trace files (e.g. `BM08_C_trace.json`) using the file picker.
2. Use **First / Prev / Step / Next / Last / Run to Fixpoint** to navigate the trace.
3. Clicking any entry in the **Raw Event Log** also jumps to that trace position.

All panels synchronize to the currently selected trace event:

| Panel | Content |
|---|---|
| **Current Event Inspector** | Step number, event type, block, statement, status, before/after state, change |
| **Execution Pipeline** | Highlights current stage: CFG → IR → Liveness → HRA → MDE → Fixpoint |
| **CFG Graph** | Mermaid-rendered directed CFG with back-edge highlighting (BM08), zoom/pan/fit |
| **Source / IR** | Block-by-block statement listing with active statement highlighted |
| **HRA Heap Graph** | Mermaid-rendered abstract heap (variable roots, allocation nodes, field edges), zoom/pan/fit |
| **HRA Textual State & Diff** | Full state text + BEFORE/EVENT/AFTER/CHANGES diff |
| **Liveness Details** | Statement USE/DEF, LiveBefore, LiveAfter for the current statement |
| **Worklist Queue** | Before queue / event (POP or PUSH) / after queue |
| **MDE Inspector** | Hit or miss at the current step, canonical index, reuse or create |
| **Event Causality** | Plain-language explanation derived from the event type |
| **Summary Statistics** | Total transfers, worklist iterations, prunings, MDE hit rate |
| **Raw Event Log** | Clickable full trace listing |

### Generating new traces

```powershell
$env:PATH = "D:\msys64\ucrt64\bin;$env:PATH"
.\build\src\trace-driver.exe experiments\benchmarks\bm03_dead_pointers.c A > visualization\BM03_A_trace.json
.\build\src\trace-driver.exe experiments\benchmarks\bm03_dead_pointers.c B > visualization\BM03_B_trace.json
.\build\src\trace-driver.exe experiments\benchmarks\bm03_dead_pointers.c C > visualization\BM03_C_trace.json
.\build\src\trace-driver.exe experiments\benchmarks\bm08_genuine_loop.c  A > visualization\BM08_A_trace.json
.\build\src\trace-driver.exe experiments\benchmarks\bm08_genuine_loop.c  B > visualization\BM08_B_trace.json
.\build\src\trace-driver.exe experiments\benchmarks\bm08_genuine_loop.c  C > visualization\BM08_C_trace.json
```

---

## Benchmark Corpus

Eight synthetic C benchmarks, each as an intraprocedural function in `experiments/benchmarks/`:

| Benchmark | Description | Key Property |
|---|---|---|
| BM01 | Simple allocation chain | Baseline: all pointers live |
| BM02 | Linked list construction | Forward field edges |
| BM03 | Dead pointers | t1, t2 pruned in Config B/C; result live |
| BM04 | Aliasing | Multiple variables pointing to same node |
| BM05 | Conditional allocation | Branch-dependent heap shape |
| BM06 | Field-load chain (unrolled) | Sequential field traversal |
| BM07 | Multi-field graph | Two distinct field types (left/right) |
| BM08 | Genuine while loop | Real CFG back-edge, fixpoint by convergence |

---

## Analysis Configuration Summary

| | Config A | Config B | Config C |
|---|---|---|---|
| **HRA** | ✅ | ✅ | ✅ |
| **Liveness pruning** | ❌ | ✅ | ✅ |
| **MDE deduplication** | ❌ | ❌ | ✅ |
| **CanonicalResult = B** | (baseline) | reference | ✅ verified |

---

## Documentation

All research documentation is in `docs/`:

1. `docs/project.md` — project overview and phase roadmap
2. `docs/architecture.md` — system architecture
3. `docs/design.md` — technical design
4. `docs/development.md` — development workflow
5. `docs/decisions.md` — architecture decision log
6. `docs/testing.md` — testing strategy
7. `docs/benchmarks.md` — benchmark selection
8. `docs/experiments.md` — experimental methodology
