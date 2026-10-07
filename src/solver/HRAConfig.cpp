#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/hra/HRAState.hpp"

namespace mdehra {
namespace solver {

void LivenessGuidedHRAConfig::postProcessState(const ir::Statement* stmt, hra::HRAState& state, HRASolverStats& statsRef) const {
    if (!liveness) return;

    // Filter out variables from state.bindings that are not in liveAfter
    for (auto it = state.bindings.begin(); it != state.bindings.end(); ) {
        if (!liveness->isLiveAfter(stmt, it->first)) {
            it = state.bindings.erase(it);
            statsRef.bindingsRemoved++;
        } else {
            ++it;
        }
    }
}

} // namespace solver
} // namespace mdehra
