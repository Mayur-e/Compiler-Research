#include "mdehra/hra/Printer.hpp"

namespace mdehra {
namespace hra {

void HRAPrinter::print(std::ostream& os, const HRAGraph& graph) {
    os << "Nodes: {";
    bool first = true;
    for (NodeID n : graph.nodes) {
        if (!first) os << ", ";
        os << "N" << n;
        first = false;
    }
    os << "}\n";

    os << "Edges:\n";
    for (const auto& edge : graph.edges) {
        os << "  N" << edge.source << " --" << edge.field.name << "--> N" << edge.target << "\n";
    }
}

void HRAPrinter::print(std::ostream& os, const HRAState& state) {
    os << "Bindings:\n";
    for (const auto& kv : state.bindings) {
        os << "  " << kv.first.name << " -> {";
        bool first = true;
        for (NodeID n : kv.second) {
            if (!first) os << ", ";
            os << "N" << n;
            first = false;
        }
        os << "}\n";
    }
    os << "Graph:\n";
    print(os, state.graph);
}

} // namespace hra
} // namespace mdehra
