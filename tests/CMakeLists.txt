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
