// MDE-HRA command-line driver — Phase 1 entry point.
//
// Phase 1 purpose: verify that the project configures, compiles, links, and
// runs correctly.  No analysis is performed here.  The executable prints the
// project identity and the three supported experimental configuration labels.
//
// Later phases will replace this stub with a real analysis driver that
// accepts a source file, a configuration flag, and analysis options.

#include <iostream>
#include "mdehra/config.hpp"

static void printConfig(mdehra::AnalysisConfiguration cfg) {
    std::cout << "  [" << mdehra::configurationLabel(cfg) << "] "
              << mdehra::configurationName(cfg) << "\n";
}

int main() {
    std::cout << mdehra::PROJECT_NAME << " "
              << mdehra::VERSION_MAJOR << "."
              << mdehra::VERSION_MINOR << "."
              << mdehra::VERSION_PATCH
              << " -- " << mdehra::PROJECT_FULL << "\n\n";

    // Show the three experimental configurations that will eventually be
    // supported.  The labels A / B / C correspond exactly to the research
    // design (docs/project.md §16, docs/decisions.md D1).
    std::cout << "Experimental configurations:\n";
    printConfig(mdehra::AnalysisConfiguration::HRA);
    printConfig(mdehra::AnalysisConfiguration::LIVENESS_HRA);
    printConfig(mdehra::AnalysisConfiguration::LIVENESS_HRA_MDE);

    std::cout << "\nStatus: Phase 1 foundation.\n"
              << "HRA, liveness analysis, and MDE are not yet implemented.\n";

    return 0;
}
