// ---------------------------------------------------------------------------
// MDE-HRA Phase 1 smoke tests
//
// Verifies the project foundation: version constants, project name, and the
// three AnalysisConfiguration identifiers (A / B / C).
//
// Design rationale: no external test library is introduced in Phase 1.
// A minimal inline test harness is sufficient and keeps the dependency
// surface zero.  If a testing library is adopted in a later phase, these
// tests can be ported at that time.
// ---------------------------------------------------------------------------

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "mdehra/config.hpp"

// ---------------------------------------------------------------------------
// Minimal inline harness
// ---------------------------------------------------------------------------
static int g_failures = 0;

static void check(bool condition, const char* description) {
    if (condition) {
        std::cout << "  PASS  " << description << "\n";
    } else {
        std::cerr << "  FAIL  " << description << "\n";
        ++g_failures;
    }
}

// String equality helper so tests read clearly.
static bool streq(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

// ---------------------------------------------------------------------------
// Test groups
// ---------------------------------------------------------------------------

static void testVersion() {
    std::cout << "[Version constants]\n";
    check(mdehra::VERSION_MAJOR == 0,  "VERSION_MAJOR == 0");
    check(mdehra::VERSION_MINOR == 1,  "VERSION_MINOR == 1");
    check(mdehra::VERSION_PATCH == 0,  "VERSION_PATCH == 0");
    check(streq(mdehra::PROJECT_NAME, "MDE-HRA"),
          "PROJECT_NAME == \"MDE-HRA\"");
}

static void testConfigurationNames() {
    std::cout << "[Configuration names]\n";

    check(streq(mdehra::configurationName(mdehra::AnalysisConfiguration::HRA),
                "A: HRA"),
          "configurationName(HRA) == \"A: HRA\"");

    check(streq(mdehra::configurationName(mdehra::AnalysisConfiguration::LIVENESS_HRA),
                "B: Liveness + HRA"),
          "configurationName(LIVENESS_HRA) == \"B: Liveness + HRA\"");

    check(streq(mdehra::configurationName(mdehra::AnalysisConfiguration::LIVENESS_HRA_MDE),
                "C: Liveness + HRA + MDE"),
          "configurationName(LIVENESS_HRA_MDE) == \"C: Liveness + HRA + MDE\"");
}

static void testConfigurationLabels() {
    std::cout << "[Configuration labels]\n";

    check(mdehra::configurationLabel(mdehra::AnalysisConfiguration::HRA) == 'A',
          "configurationLabel(HRA) == 'A'");

    check(mdehra::configurationLabel(mdehra::AnalysisConfiguration::LIVENESS_HRA) == 'B',
          "configurationLabel(LIVENESS_HRA) == 'B'");

    check(mdehra::configurationLabel(mdehra::AnalysisConfiguration::LIVENESS_HRA_MDE) == 'C',
          "configurationLabel(LIVENESS_HRA_MDE) == 'C'");
}

static void testConfigurationDistinctness() {
    std::cout << "[Configuration distinctness]\n";

    // The three experimental configurations must be distinct enum values.
    // This catches accidental enum aliasing.
    using AC = mdehra::AnalysisConfiguration;
    check(AC::HRA             != AC::LIVENESS_HRA,     "A != B");
    check(AC::LIVENESS_HRA    != AC::LIVENESS_HRA_MDE, "B != C");
    check(AC::HRA             != AC::LIVENESS_HRA_MDE, "A != C");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== MDE-HRA Phase 1 smoke tests ===\n\n";

    testVersion();
    std::cout << "\n";
    testConfigurationNames();
    std::cout << "\n";
    testConfigurationLabels();
    std::cout << "\n";
    testConfigurationDistinctness();

    std::cout << "\n===================================\n";
    if (g_failures == 0) {
        std::cout << "Result: ALL TESTS PASSED\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "Result: " << g_failures << " TEST(S) FAILED\n";
        return EXIT_FAILURE;
    }
}
