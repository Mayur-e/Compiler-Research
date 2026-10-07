#pragma once
#include "mdehra/hra/HRAGraph.hpp"

namespace mdehra {
namespace hra {

struct HRAState {
    std::map<ir::Variable, std::set<NodeID>> bindings;
    HRAGraph graph;

    bool operator==(const HRAState& other) const {
        return bindings == other.bindings && graph == other.graph;
    }
    bool operator!=(const HRAState& other) const {
        return !(*this == other);
    }

    // Merges `other` into this state. Returns true if this state changed.
    bool merge(const HRAState& other) {
        bool changed = false;
        for (const auto& kv : other.bindings) {
            const auto& var = kv.first;
            const auto& nodes = kv.second;
            auto& myNodes = bindings[var];
            size_t oldSize = myNodes.size();
            myNodes.insert(nodes.begin(), nodes.end());
            if (myNodes.size() > oldSize) changed = true;
        }

        size_t oldNodes = graph.nodes.size();
        size_t oldEdges = graph.edges.size();
        graph.merge(other.graph);
        if (graph.nodes.size() > oldNodes || graph.edges.size() > oldEdges) changed = true;

        return changed;
    }
};

} // namespace hra
} // namespace mdehra
