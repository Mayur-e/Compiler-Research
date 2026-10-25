#pragma once
#include "mdehra/ir/ProgramPoint.hpp"
#include "mdehra/ir/Variable.hpp"
#include <set>
#include <map>
#include <vector>
#include <string>

namespace mdehra {
namespace hra {

using NodeID = ir::ID;

struct HRAEdge {
    NodeID source;
    ir::Field field;
    NodeID target;

    bool operator==(const HRAEdge& other) const {
        return source == other.source && field == other.field && target == other.target;
    }
    bool operator<(const HRAEdge& other) const {
        if (source != other.source) return source < other.source;
        if (field < other.field) return true;
        if (other.field < field) return false;
        return target < other.target;
    }
};

struct HRAGraph {
    std::set<NodeID> nodes;
    std::set<HRAEdge> edges;

    bool operator==(const HRAGraph& other) const {
        return nodes == other.nodes && edges == other.edges;
    }

    void addNode(NodeID id) { nodes.insert(id); }
    void addEdge(NodeID src, const ir::Field& f, NodeID tgt) {
        nodes.insert(src);
        nodes.insert(tgt);
        edges.insert({src, f, tgt});
    }

    void removeEdges(NodeID src, const ir::Field& f) {
        for (auto it = edges.begin(); it != edges.end(); ) {
            if (it->source == src && it->field == f) {
                it = edges.erase(it);
            } else {
                ++it;
            }
        }
    }

    void merge(const HRAGraph& other) {
        nodes.insert(other.nodes.begin(), other.nodes.end());
        edges.insert(other.edges.begin(), other.edges.end());
    }
};

} // namespace hra
} // namespace mdehra
