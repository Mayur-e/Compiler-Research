#pragma once
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/mde/HRAEncoder.hpp"
#include <map>
#include <unordered_map>
#include <vector>

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

    size_t getRetainedStateBytes() const;
    
    // Performance metrics
    int mdeHits = 0;
    int mdeMisses = 0;
    size_t hashCollisions = 0;

public:
    HRAEncoder encoder;
private:
    std::map<StateIndex, std::shared_ptr<const hra::HRAState>> canonicalStates;
    // Fast-path structural fingerprint cache with collision resolution buckets
    std::unordered_map<uint64_t, std::vector<std::pair<std::shared_ptr<const hra::HRAState>, StateIndex>>> fingerprintCache;
};

} // namespace mde_adapter
} // namespace mdehra
