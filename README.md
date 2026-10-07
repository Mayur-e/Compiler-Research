# MDE-HRA

**Deduplication-Accelerated Heap Reference Analysis for Scalable, Liveness-Pruned Pointer Analysis**

---

## Research Objective

This project investigates whether **structural deduplication using MDE / LatticeHashForest** can make **liveness-pruned Heap Reference Analysis (HRA)** more scalable while preserving the analysis result.

The system will support three experimental configurations:

| Label | Configuration | Purpose |
|---|---|---|
| **A** | HRA | Baseline heap reference analysis |
| **B** | Liveness + HRA | Liveness-pruned baseline |
| **C** | Liveness + HRA + MDE | Full proposed system |

The primary correctness requirement is:

```
CanonicalResult(B) == CanonicalResult(C)
```

The primary performance comparison is **B vs C**, which isolates the effect of MDE deduplication.

---

## Current Implementation Status

**Phase 1 — Project foundation.**

This phase establishes the build system, directory structure, and a minimal testable skeleton.

> **HRA, liveness analysis, and MDE are not yet implemented.**  
> The analyses will be added in subsequent phases following the roadmap in `docs/project.md`.

---

## Repository Structure

```
Project/
│
├── CMakeLists.txt          ← top-level CMake configuration
├── README.md               ← this file
├── .gitignore
│
├── include/                ← public headers (one subdirectory per module)
│   ├── mdehra/             ← project-level configuration (Phase 1)
│   ├── frontend/           ← [future] program representation / IR
│   ├── liveness/           ← [future] backward liveness analysis
│   ├── hra/                ← [future] heap reference analysis
│   ├── solver/             ← [future] fixpoint / worklist
│   └── instrumentation/    ← [future] metrics and measurement
│
├── src/                    ← implementation files
│   ├── CMakeLists.txt
│   ├── main.cpp            ← command-line driver entry point
│   ├── mdehra/             ← project-level configuration (Phase 1)
│   ├── frontend/           ← [future]
│   ├── liveness/           ← [future]
│   ├── hra/                ← [future]
│   ├── solver/             ← [future]
│   └── instrumentation/    ← [future]
│
├── tests/                  ← automated tests (CTest)
│   ├── CMakeLists.txt
│   └── test_config.cpp     ← Phase 1 smoke test
│
├── benchmarks/             ← [future] benchmark C programs and harness
│
├── scripts/                ← [future] automation scripts (Python / CMake)
│
└── docs/                   ← research documentation (do not modify lightly)
    ├── project.md           — implementation plan and phase roadmap
    ├── architecture.md      — system architecture
    ├── design.md            — technical design and data model
    ├── development.md       — development workflow
    ├── decisions.md         — architecture decision log
    ├── benchmarks.md        — benchmark strategy
    ├── experiments.md       — experimental methodology
    ├── testing.md           — testing strategy
    └── skill.md             — AI agent implementation guide
```

---

## Prerequisites

This project requires a modern C++17 compiler and the LLVM/Clang development libraries to parse C source code via Clang LibTooling.

**Current Environment Status (Phase 2A Inspection):**
- Native MSVC / Visual Studio is NOT installed.
- Native LLVM/Clang is NOT installed.
- MSYS2 is NOT installed.
- The `D:\mingw64` GCC 16.1.0 installation is incomplete and broken.
- The `C:\MinGW` GCC 6.3.0 installation works but lacks complete C++17 support and Clang integration.

**Recommended Setup:**
Because we rely on LLVM/Clang LibTooling, the recommended development environment on Windows is **MSYS2** using the **UCRT64** environment.

### 1. Windows Host Setup (MSYS2 Installation)
1. Download and install MSYS2 from [msys2.org](https://www.msys2.org/).
2. **IMPORTANT:** Install MSYS2 on the **D drive** (e.g., `D:\msys64`) as the C drive is full.
3. Once installed, open the **MSYS2 UCRT64** terminal (not the default MSYS or MINGW64 terminal).

### 2. MSYS2 Environment Setup (UCRT64)
Inside the **MSYS2 UCRT64** terminal, install the required compiler, LLVM/Clang libraries, and build tools:

```bash
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-llvm \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja
```

This will provide:
- `clang++` and `g++` (modern C++17/C++20 capable)
- LLVM and Clang development headers/libraries
- Modern CMake
- Ninja build system

---

## Build Instructions

All commands are run from the repository root (`d:\TS\Seminar\Project\`).

### 1. Configure

```powershell
cmake -S . -B build -G Ninja
```

### 2. Build

```powershell
cmake --build build
```

### 3. Run tests

```powershell
ctest --test-dir build --output-on-failure
```

### 4. Run the driver executable

```powershell
.\build\src\mdehra-driver.exe
```

### Clean rebuild

```powershell
cmake --build build --clean-first
```

---

## Test Instructions

Tests are registered with CTest and run automatically after building.

```powershell
ctest --test-dir build -V
```

### Current tests

| Test name | File | What it verifies |
|---|---|---|
| `Phase1_SmokeTest_Config` | `tests/test_config.cpp` | Version constants, configuration names (A/B/C), configuration labels, mutual distinctness of the three configurations |

---

## Future Analysis Configurations

The following configurations will be implemented in later phases:

```
[A]  HRA                        — Phase 3 (baseline heap reference analysis)
[B]  Liveness + HRA             — Phase 6 (liveness-pruned analysis)
[C]  Liveness + HRA + MDE       — Phase 9–10 (full proposed system)
```

---

## Important Current Limitations

- **No frontend or IR.** The strategy for parsing C programs (custom parser, Clang LibTooling, or other) has not yet been selected. This decision will be made in Phase 1 transition / Phase 2.
- **No CFG.** Control-flow graph construction belongs to Phase 2.
- **No analysis.** HRA, liveness, and MDE are not implemented.
- **MDE source undecided.** The specific MDE / LatticeHashForest implementation to be studied has not yet been identified.
- **Windows-primary development.** The build uses CMake + Ninja for portability. Shell scripts (`.sh`) have not been created; CMake commands are the canonical build mechanism.

---

## Documentation

All research documentation is in `docs/`. Reading order for new contributors:

1. `docs/project.md` — project overview and phase roadmap
2. `docs/architecture.md` — system architecture
3. `docs/design.md` — technical design
4. `docs/development.md` — development workflow
5. `docs/decisions.md` — architecture decision log
6. `docs/testing.md` — testing strategy
7. `docs/benchmarks.md` — benchmark selection
8. `docs/experiments.md` — experimental methodology
