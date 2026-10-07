#include "mdehra/mde/MDEGuidedHRAConfig.hpp"

namespace mdehra {
namespace mde_adapter {

MDEGuidedHRAConfig::MDEGuidedHRAConfig(const liveness::LivenessResult* l) 
    : solver::LivenessGuidedHRAConfig(l) {}

std::shared_ptr<const hra::HRAState> MDEGuidedHRAConfig::canonicalizeState(const hra::HRAState& state) {
    stats.canonicalizeCalls++;
    StateIndex idx = encoder.encode(state);
    
    auto it = canonicalStates.find(idx);
    if (it != canonicalStates.end()) {
        mdeHits++;
        stats.reusedCanonicalStates++;
        this->traceCanonicalize(state, it->second, false, (size_t)idx.value);
        return it->second;
    }
    
    mdeMisses++;
    stats.newCanonicalStates++;
    auto ptr = std::make_shared<hra::HRAState>(state);
    canonicalStates[idx] = ptr;
    this->traceCanonicalize(state, ptr, true, (size_t)idx.value);
    return ptr;
}

} // namespace mde_adapter
} // namespace mdehra
