# ---------------------------------------------------------------------------
# MDE-HRA core library and main driver executable
# ---------------------------------------------------------------------------

# --- Core library -----------------------------------------------------------
# mdehra_core holds the Phase-1 configuration abstraction.
add_library(mdehra_core STATIC
    mdehra/config.cpp
)
target_include_directories(mdehra_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# --- IR library -------------------------------------------------------------
add_library(mdehra_ir STATIC
    ir/Printer.cpp
)
target_include_directories(mdehra_ir PUBLIC ${PROJECT_SOURCE_DIR}/include)

# --- Frontend library -------------------------------------------------------
add_library(mdehra_frontend STATIC
    frontend/ClangFrontend.cpp
)
target_include_directories(mdehra_frontend PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)
target_compile_options(mdehra_frontend PRIVATE ${LLVM_DEFINITIONS_LIST})
target_link_libraries(mdehra_frontend PUBLIC mdehra_ir)

# --- HRA library -------------------------------------------------------------
add_library(mdehra_hra STATIC
    hra/Printer.cpp
)
target_include_directories(mdehra_hra PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mdehra_hra PUBLIC mdehra_ir)

# --- Liveness library --------------------------------------------------------
add_library(mdehra_liveness STATIC
    liveness/LivenessSolver.cpp
    liveness/Printer.cpp
)
target_include_directories(mdehra_liveness PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mdehra_liveness PUBLIC mdehra_ir)

# --- Solver library ----------------------------------------------------------
add_library(mdehra_solver STATIC
    solver/HRASolver.cpp
    solver/HRAConfig.cpp
)
target_include_directories(mdehra_solver PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mdehra_solver PUBLIC mdehra_hra)

# --- MDE Adapter library -----------------------------------------------------
add_library(mdehra_adapter STATIC
    mde/HRAEncoder.cpp
    mde/MDEGuidedHRAConfig.cpp
)
target_include_directories(mdehra_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(mdehra_adapter PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)
target_link_libraries(mdehra_adapter PUBLIC mdehra_hra)



# --- Main driver executable -------------------------------------------------
add_executable(mdehra-driver
    main.cpp
)

target_link_libraries(mdehra-driver PRIVATE 
    mdehra_core 
    mdehra_frontend
    mdehra_liveness
    mdehra_solver
)

# --- Instrumentation executable -----------------------------------------------
add_executable(instr-driver
    instrumentation.cpp
)

target_link_libraries(instr-driver PRIVATE 
    mdehra_core 
    mdehra_frontend
    mdehra_liveness
    mdehra_solver
    mdehra_adapter
    clangTooling
)
target_include_directories(instr-driver PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)

# --- Trace driver executable -----------------------------------------------
add_executable(trace-driver
    trace_driver.cpp
)

target_link_libraries(trace-driver PRIVATE 
    mdehra_core 
    mdehra_frontend
    mdehra_liveness
    mdehra_solver
    mdehra_adapter
    clangTooling
)
target_include_directories(trace-driver PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)

# --- IR dump diagnostic executable -------------------------------------------
# Prints full block structure (predecessors/successors/statements) for a
# given C source file. Used to provide CFG back-edge evidence for BM08.
add_executable(ir-dump-driver
    ir_dump.cpp
)
target_link_libraries(ir-dump-driver PRIVATE
    mdehra_core
    mdehra_frontend
    mdehra_ir
    clangTooling
)

