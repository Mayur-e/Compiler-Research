#include "mdehra/config.hpp"

namespace mdehra {

const char* configurationName(AnalysisConfiguration config) {
    switch (config) {
        case AnalysisConfiguration::HRA:
            return "A: HRA";
        case AnalysisConfiguration::LIVENESS_HRA:
            return "B: Liveness + HRA";
        case AnalysisConfiguration::LIVENESS_HRA_MDE:
            return "C: Liveness + HRA + MDE";
    }
    return "<unknown configuration>";
}

char configurationLabel(AnalysisConfiguration config) {
    switch (config) {
        case AnalysisConfiguration::HRA:             return 'A';
        case AnalysisConfiguration::LIVENESS_HRA:    return 'B';
        case AnalysisConfiguration::LIVENESS_HRA_MDE: return 'C';
    }
    return '?';
}

} // namespace mdehra
