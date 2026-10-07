#include "mdehra/mde/HRAEncoder.hpp"
#include <algorithm>
#include <sstream>

namespace mdehra {
namespace mde_adapter {

StateIndex HRAEncoder::encode(const hra::HRAState& state) {
    // 1. Encode Bindings
    std::vector<BindingMDE::PropertyElement> bindingElements;
    // Map is already sorted by Variable
    for (const auto& kv : state.bindings) {
        const ir::Variable& var = kv.first;
        const std::set<hra::NodeID>& targets = kv.second;
        
        // Encode targets for this variable
        std::vector<TargetNodeSetMDE::PropertyElement> targetElements;
        for (hra::NodeID t : targets) {
            targetElements.push_back({t});
        }
        // No need to sort targetElements because std::set is already sorted
        TargetNodeSetIndex tIdx = targetNodes.register_set(std::move(targetElements));
        
        // Create binding property element
        bindingElements.push_back({var.id, std::make_tuple(tIdx)});
    }
    // No need to sort bindingElements because std::map is already sorted by var (and its ID)
    // Wait! Is std::map sorted by var.id? 
    // Let's check ir::Variable's operator<. It first checks ID, then name. 
    // Since IDs are unique and it uses ID first, iterating map will give sorted order of IDs.
    BindingIndex bIdx = bindings.register_set(std::move(bindingElements));

    // 2. Encode Graph
    // Group edges by source node
    std::map<hra::NodeID, std::set<EdgeTarget>> nodeEdges;
    
    // Make sure we include all nodes, even those without edges!
    for (hra::NodeID n : state.graph.nodes) {
        nodeEdges[n] = {}; // Initialize empty set
    }
    
    // Add edges
    for (const hra::HRAEdge& e : state.graph.edges) {
        nodeEdges[e.source].insert({e.field.id, e.target});
    }

    std::vector<GraphMDE::PropertyElement> graphElements;
    for (const auto& kv : nodeEdges) {
        hra::NodeID src = kv.first;
        const std::set<EdgeTarget>& edges = kv.second;
        
        std::vector<EdgeSetMDE::PropertyElement> edgeElements;
        for (const EdgeTarget& et : edges) {
            edgeElements.push_back({et});
        }
        // std::set is already sorted, so edgeElements is sorted
        EdgeSetIndex eIdx = edgeSets.register_set(std::move(edgeElements));
        
        graphElements.push_back({src, std::make_tuple(eIdx)});
    }
    // std::map is already sorted by NodeID, so graphElements is sorted.
    GraphIndex gIdx = graphs.register_set(std::move(graphElements));

    // 3. Encode State
    std::vector<StateMDE::PropertyElement> stateElements;
    stateElements.push_back({0, std::make_tuple(bIdx, gIdx)});
    // A single element vector is trivially sorted
    
    return states.register_set(std::move(stateElements));
}

std::string HRAEncoder::dump_perf() const {
    std::stringstream ss;
    ss << "=== MDE Performance Dump ===\n";
    ss << "--- TargetNodes ---\n" << targetNodes.dump_perf() << "\n";
    ss << "--- Bindings ---\n" << bindings.dump_perf() << "\n";
    ss << "--- EdgeSets ---\n" << edgeSets.dump_perf() << "\n";
    ss << "--- Graphs ---\n" << graphs.dump_perf() << "\n";
    ss << "--- States ---\n" << states.dump_perf() << "\n";
    return ss.str();
}

} // namespace mde_adapter
} // namespace mdehra
