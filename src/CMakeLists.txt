# ---------------------------------------------------------------------------
# MDE-HRA core library and main driver executable
# ---------------------------------------------------------------------------

# --- Core library -----------------------------------------------------------
# mdehra_core holds the Phase-1 configuration abstraction.
# Later phases will add HRA, liveness, MDE, solver, etc. to this target
# or create additional library targets as needed.

add_library(mdehra_core STATIC
    mdehra/config.cpp
)

target_include_directories(mdehra_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# --- Main driver executable -------------------------------------------------
# Provides the entry point for the command-line analysis tool.
# Phase 1: prints version and configuration information only.
# Later phases will wire in the frontend, CFG, and analysis pipeline.

add_executable(mdehra-driver
    main.cpp
)

target_link_libraries(mdehra-driver PRIVATE mdehra_core)
