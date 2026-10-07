cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Project declaration
# ---------------------------------------------------------------------------
project(mde-hra
    VERSION 0.1.0
    DESCRIPTION "Deduplication-Accelerated Heap Reference Analysis (MDE-HRA)"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++17 — required across all targets
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # Use -std=c++17, not -std=gnu++17

# ---------------------------------------------------------------------------
# Compiler warnings
# GCC/Clang: -Wall -Wextra -Wpedantic
# MSVC:      /W4
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ---------------------------------------------------------------------------
# Keep build artifacts out of the source tree.
# Enforce out-of-source builds.
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed.\n"
        "Please create a separate build directory:\n"
        "  cmake -S . -B build -G Ninja\n"
        "  cmake --build build\n"
    )
endif()

# ---------------------------------------------------------------------------
# Find LLVM and Clang (Phase 2A integration)
# ---------------------------------------------------------------------------
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang ${Clang_VERSION}")

# ---------------------------------------------------------------------------
# Global include path
# ---------------------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)


# ---------------------------------------------------------------------------
# Testing support via CTest
# ---------------------------------------------------------------------------
enable_testing()

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(tests)
