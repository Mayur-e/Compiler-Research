#pragma once
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/mde/HRAEncoder.hpp"
#include <map>

namespace mdehra {
namespace mde_adapter {

class MDEGuidedHRAConfig : public solver::LivenessGuidedHRAConfig {
public:
    MDEGuidedHRAConfig(const liveness::LivenessResult* l);

    std::shared_ptr<const hra::HRAState> canonicalizeState(const hra::HRAState& state) override;
    
    // Expose encoding directly for testing injective properties
    StateIndex encodeState(const hra::HRAState& state) {
        return encoder.encode(state);
    }
    
    // Performance metrics
    int mdeHits = 0;
    int mdeMisses = 0;

public:
    HRAEncoder encoder;
private:
    std::map<StateIndex, std::shared_ptr<const hra::HRAState>> canonicalStates;
};

} // namespace mde_adapter
} // namespace mdehra
