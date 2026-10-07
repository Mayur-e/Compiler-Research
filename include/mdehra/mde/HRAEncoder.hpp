#pragma once
#include "mdehra/hra/HRAState.hpp"
#define MDE_ENABLE_PERFORMANCE_METRICS
#include "mde/mde.hpp"
#include <vector>

namespace mdehra {
namespace mde_adapter {

// 1. TargetNodeSetMDE
using TargetNodeSetMDE = mde::MDENode<mde::MDEConfig<hra::NodeID>>;
using TargetNodeSetIndex = TargetNodeSetMDE::Index;

// 2. BindingMDE
using BindingMDE = mde::MDENode<
    mde::MDEConfig<ir::ID>, 
    mde::NestingBase<ir::ID, TargetNodeSetMDE>
>;
using BindingIndex = BindingMDE::Index;

// 3. EdgeSetMDE
struct EdgeTarget {
    ir::ID fieldId;
    hra::NodeID targetNode;

    bool operator<(const EdgeTarget& other) const {
        if (fieldId != other.fieldId) return fieldId < other.fieldId;
        return targetNode < other.targetNode;
    }
    bool operator==(const EdgeTarget& other) const {
        return fieldId == other.fieldId && targetNode == other.targetNode;
    }
};

struct EdgeTargetHash {
    std::size_t operator()(const EdgeTarget& e) const {
        return (std::hash<ir::ID>()(e.fieldId) * 31) ^ std::hash<hra::NodeID>()(e.targetNode);
    }
};

struct EdgeSetConfig : mde::MDEConfig<EdgeTarget> {
    using PropertyHash = EdgeTargetHash;
};
using EdgeSetMDE = mde::MDENode<EdgeSetConfig>;
using EdgeSetIndex = EdgeSetMDE::Index;

// 4. GraphMDE
using GraphMDE = mde::MDENode<
    mde::MDEConfig<hra::NodeID>,
    mde::NestingBase<hra::NodeID, EdgeSetMDE>
>;
using GraphIndex = GraphMDE::Index;

// 5. StateMDE
using StateMDE = mde::MDENode<
    mde::MDEConfig<int>, // Key will just be 0
    mde::NestingBase<int, BindingMDE, GraphMDE>
>;
using StateIndex = StateMDE::Index;

// The Encoder Context holds the MDE engines
class HRAEncoder {
public:
    TargetNodeSetMDE targetNodes;
    BindingMDE bindings;
    EdgeSetMDE edgeSets;
    GraphMDE graphs;
    StateMDE states;

    HRAEncoder()
        : bindings(BindingMDE::RefList{targetNodes}),
          graphs(GraphMDE::RefList{edgeSets}),
          states(StateMDE::RefList{bindings, graphs}) {}

    // Encode an HRA state into a deterministic StateIndex
    StateIndex encode(const hra::HRAState& state);
    
    // Performance metrics extraction
    std::string dump_perf() const;
};

} // namespace mde_adapter
} // namespace mdehra
