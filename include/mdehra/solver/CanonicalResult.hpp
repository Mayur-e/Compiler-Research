#pragma once
// ============================================================
// CanonicalResult.hpp  (Phase 15)
//
// Shared helper: produces a deterministic string representation
// of an HRAState relative to a set of live-out variables.
// Two states are semantically equivalent iff their canonical
// strings are equal (for the same live-out set).
//
// This is identical to the canonicalizer used in
// tests/e2e_correctness_test.cpp so that perf-analyzer and
// CTest share the same correctness criterion.
// ============================================================

#include "mdehra/hra/HRAState.hpp"
#include "mdehra/ir/Program.hpp"
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <memory>
#include <tuple>

namespace mdehra {
namespace solver {

/// Produces a deterministic canonical string for \p statePtr
/// relative to the live-out variable set \p liveOut.
/// Returns "Empty" if statePtr is null.
inline std::string getCanonicalOutput(
        std::shared_ptr<const hra::HRAState> statePtr,
        const std::set<ir::Variable>& liveOut)
{
    if (!statePtr) return "Empty";
    const auto& state = *statePtr;

    std::map<hra::NodeID, hra::NodeID> canonicalIds;
    hra::NodeID nextId = 1;

    // Collect non-empty live bindings
    std::map<std::string, std::set<hra::NodeID>> liveBindings;
    for (const auto& var : liveOut) {
        auto it = state.bindings.find(var);
        if (it != state.bindings.end() && !it->second.empty()) {
            liveBindings[var.name] = it->second;
        }
    }

    // BFS canonical numbering from live-root nodes
    std::queue<hra::NodeID> q;
    for (const auto& kv : liveBindings) {
        std::vector<hra::NodeID> nodes(kv.second.begin(), kv.second.end());
        std::sort(nodes.begin(), nodes.end());
        for (hra::NodeID n : nodes) {
            if (canonicalIds.find(n) == canonicalIds.end()) {
                canonicalIds[n] = nextId++;
                q.push(n);
            }
        }
    }

    std::set<std::tuple<hra::NodeID, std::string, hra::NodeID>> canonicalEdges;
    while (!q.empty()) {
        hra::NodeID curr = q.front(); q.pop();
        for (const auto& edge : state.graph.edges) {
            if (edge.source == curr) {
                hra::NodeID to = edge.target;
                if (canonicalIds.find(to) == canonicalIds.end()) {
                    canonicalIds[to] = nextId++;
                    q.push(to);
                }
                canonicalEdges.insert({canonicalIds[curr], edge.field.name, canonicalIds[to]});
            }
        }
    }

    std::stringstream ss;
    ss << "Bindings:\n";
    for (const auto& kv : liveBindings) {
        ss << "  " << kv.first << " -> {";
        std::vector<hra::NodeID> cnodes;
        for (hra::NodeID n : kv.second) cnodes.push_back(canonicalIds[n]);
        std::sort(cnodes.begin(), cnodes.end());
        bool first = true;
        for (hra::NodeID c : cnodes) {
            if (!first) ss << ", ";
            ss << "C" << c;
            first = false;
        }
        ss << "}\n";
    }
    ss << "Edges:\n";
    for (const auto& e : canonicalEdges) {
        ss << "  C" << std::get<0>(e) << " -" << std::get<1>(e) << "-> C" << std::get<2>(e) << "\n";
    }
    return ss.str();
}

} // namespace solver
} // namespace mdehra
