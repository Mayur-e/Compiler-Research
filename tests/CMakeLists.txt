# ---------------------------------------------------------------------------
# MDE-HRA test suite — Phase 1
# ---------------------------------------------------------------------------
# Tests are registered with CTest so that `ctest --test-dir build` (or
# `cmake --build build --target test`) runs them automatically.
#
# Phase 1 contains one smoke test.  Each subsequent phase will add tests
# in this directory (or in phase-specific subdirectories if the suite grows).

# --- Phase 1 smoke test -----------------------------------------------------
# Verifies: version constants, configuration names, configuration labels,
# and distinctness of the three experimental configurations (A / B / C).

add_executable(test_config
    test_config.cpp
)

target_link_libraries(test_config PRIVATE mdehra_core)

target_include_directories(test_config PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(
    NAME  Phase1_SmokeTest_Config
    COMMAND test_config
)

# --- Clang/LLVM Integration Probe (Phase 2A) --------------------------------
add_executable(clang_probe clang_probe.cpp)

# Include LLVM and Clang headers
target_include_directories(clang_probe PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# LLVM requires specific definitions to build correctly
separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND ${LLVM_DEFINITIONS})
target_compile_options(clang_probe PRIVATE ${LLVM_DEFINITIONS_LIST})



# Link the required Clang libraries for LibTooling and AST parsing
target_link_libraries(clang_probe PRIVATE
    clangTooling
    clangFrontend
    clangSerialization
    clangDriver
    clangParse
    clangSema
    clangAnalysis
    clangEdit
    clangAST
    clangLex
    clangBasic
)

# Use LLVM's CMake utility to link the necessary LLVM components
llvm_map_components_to_libnames(llvm_libs support core option mc)
target_link_libraries(clang_probe PRIVATE ${llvm_libs})

add_test(NAME Phase2A_ClangProbe COMMAND clang_probe)

# --- IR Test (Phase 2B) -----------------------------------------------------
add_executable(ir_test ir_test.cpp)
target_link_libraries(ir_test PRIVATE mdehra_ir)
target_include_directories(ir_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME Phase2B_IRTest COMMAND ir_test)

# --- Frontend Test (Phase 2B) -----------------------------------------------
add_executable(frontend_test frontend_test.cpp)

# Include LLVM and Clang headers
target_include_directories(frontend_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# LLVM requires specific definitions to build correctly
target_compile_options(frontend_test PRIVATE ${LLVM_DEFINITIONS_LIST})

# Link frontend, ir, and Clang libraries
target_link_libraries(frontend_test PRIVATE
    mdehra_frontend
    mdehra_ir
    clangTooling
    clangFrontend
    clangSerialization
    clangDriver
    clangParse
    clangSema
    clangAnalysis
    clangEdit
    clangAST
    clangLex
    clangBasic
    ${llvm_libs}
)

add_test(NAME Phase2B_FrontendTest COMMAND frontend_test)

# --- HRA Test (Phase 3) -----------------------------------------------------
add_executable(hra_test hra_test.cpp)
target_link_libraries(hra_test PRIVATE mdehra_solver mdehra_hra mdehra_ir)
target_include_directories(hra_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME Phase3_HRATest COMMAND hra_test)

# --- Liveness Test (Phase 4) ------------------------------------------------
add_executable(liveness_test liveness_test.cpp)
target_link_libraries(liveness_test PRIVATE mdehra_liveness mdehra_ir)
target_include_directories(liveness_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME Phase4_LivenessTest COMMAND liveness_test)

# --- Integration Test (Phase 5) ----------------------------------------------
add_executable(integration_test integration_test.cpp)
target_link_libraries(integration_test PRIVATE mdehra_solver mdehra_liveness mdehra_hra mdehra_ir)
target_include_directories(integration_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME Phase5_IntegrationTest COMMAND integration_test)

# --- MDE Test (Phase 6) ------------------------------------------------------
add_executable(mde_test mde_test.cpp)
target_include_directories(mde_test PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)
add_test(NAME Phase6_MDETest COMMAND mde_test)

# --- MDE Adapter Test (Phase 7) ----------------------------------------------
add_executable(hra_mde_adapter_test hra_mde_adapter_test.cpp)
target_include_directories(hra_mde_adapter_test PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)
target_link_libraries(hra_mde_adapter_test PRIVATE mdehra_adapter mdehra_hra)
add_test(NAME Phase7_AdapterTest COMMAND hra_mde_adapter_test)

# --- Configuration C Test (Phase 8) ------------------------------------------
add_executable(config_c_test config_c_test.cpp)
target_include_directories(config_c_test PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)
target_link_libraries(config_c_test PRIVATE mdehra_solver mdehra_liveness mdehra_adapter)
add_test(NAME Phase8_ConfigCTest COMMAND config_c_test)

# --- E2E Correctness Test (Phase 9) ------------------------------------------
add_executable(e2e_correctness_test e2e_correctness_test.cpp)
target_include_directories(e2e_correctness_test PRIVATE ${PROJECT_SOURCE_DIR}/mde_src/src/include)
target_link_libraries(e2e_correctness_test PRIVATE mdehra_solver mdehra_liveness mdehra_adapter mdehra_frontend clangTooling)
add_test(NAME Phase9_E2ECorrectnessTest COMMAND e2e_correctness_test)
