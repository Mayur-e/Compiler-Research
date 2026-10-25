#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include <chrono>

namespace mdehra {
namespace mde_adapter {

static uint64_t computeStructuralFingerprint(const hra::HRAState& state) {
    uint64_t h = 0xcbf29ce484222325ULL;
    auto fnv = [&](uint64_t v) {
        h ^= v;
        h *= 0x100000001b3ULL;
    };
    for (const auto& kv : state.bindings) {
        fnv(kv.first.id);
        fnv(kv.first.isPointer ? 1 : 0);
        for (hra::NodeID n : kv.second) {
            fnv(n);
        }
    }
    for (hra::NodeID n : state.graph.nodes) {
        fnv(n * 31);
    }
    for (const auto& e : state.graph.edges) {
        fnv(e.source);
        fnv(e.field.id);
        fnv(e.target);
    }
    return h;
}

MDEGuidedHRAConfig::MDEGuidedHRAConfig(const liveness::LivenessResult* l) 
    : solver::LivenessGuidedHRAConfig(l) {}

std::shared_ptr<const hra::HRAState> MDEGuidedHRAConfig::canonicalizeState(const hra::HRAState& state) {
    auto tStart = std::chrono::high_resolution_clock::now();
    stats.canonicalizeCalls++;

    uint64_t fp = computeStructuralFingerprint(state);
    auto fpIt = fingerprintCache.find(fp);
    if (fpIt != fingerprintCache.end()) {
        for (const auto& entry : fpIt->second) {
            // Verify structural equality to prevent hash collisions from conflating states
            if (*(entry.first) == state) {
                mdeHits++;
                stats.reusedCanonicalStates++;
                this->traceCanonicalize(state, entry.first, false, (size_t)entry.second.value);
                auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
                stats.canonicalizeTimeMs += elapsed;
                return entry.first;
            } else {
                hashCollisions++;
            }
        }
    }

    // Fallback: encode into MDE
    StateIndex idx = encoder.encode(state);
    auto it = canonicalStates.find(idx);
    if (it != canonicalStates.end()) {
        mdeHits++;
        stats.reusedCanonicalStates++;
        fingerprintCache[fp].push_back({it->second, idx});
        this->traceCanonicalize(state, it->second, false, (size_t)idx.value);
        auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
        stats.canonicalizeTimeMs += elapsed;
        return it->second;
    }

    mdeMisses++;
    stats.newCanonicalStates++;
    auto ptr = std::make_shared<hra::HRAState>(state);
    canonicalStates[idx] = ptr;
    fingerprintCache[fp].push_back({ptr, idx});
    this->traceCanonicalize(state, ptr, true, (size_t)idx.value);
    stats.retainedStateBytes = getRetainedStateBytes();
    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
    stats.canonicalizeTimeMs += elapsed;
    return ptr;
}

size_t MDEGuidedHRAConfig::getRetainedStateBytes() const {
    size_t total = 0;
    for (const auto& kv : canonicalStates) {
        total += sizeof(hra::HRAState);
        for (const auto& b : kv.second->bindings) {
            total += sizeof(ir::Variable) + sizeof(std::set<hra::NodeID>) + b.second.size() * sizeof(hra::NodeID);
        }
        total += kv.second->graph.nodes.size() * sizeof(hra::NodeID);
        total += kv.second->graph.edges.size() * sizeof(hra::HRAEdge);
    }
    return total;
}

} // namespace mde_adapter
} // namespace mdehra
