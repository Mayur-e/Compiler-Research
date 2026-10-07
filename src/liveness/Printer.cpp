#include "mdehra/liveness/Printer.hpp"

namespace mdehra {
namespace liveness {

void LivenessPrinter::print(std::ostream& os, const LivenessResult& result, const ir::Function& func) {
    for (const auto& kv : func.blocks) {
        ir::ID blockId = kv.first;
        const auto& block = kv.second;

        os << "Block " << blockId << ":\n";
        
        os << "  LiveIn: {";
        bool first = true;
        if (result.liveIn.count(blockId)) {
            for (const auto& var : result.liveIn.at(blockId)) {
                if (!first) os << ", ";
                os << var.name;
                first = false;
            }
        }
        os << "}\n";

        for (const auto& stmtPtr : block.statements) {
            const ir::Statement* stmt = stmtPtr.get();
            os << "    Stmt LiveBefore: {";
            first = true;
            if (result.liveBefore.count(stmt)) {
                for (const auto& var : result.liveBefore.at(stmt)) {
                    if (!first) os << ", ";
                    os << var.name;
                    first = false;
                }
            }
            os << "}\n";
            
            os << "    Stmt LiveAfter: {";
            first = true;
            if (result.liveAfter.count(stmt)) {
                for (const auto& var : result.liveAfter.at(stmt)) {
                    if (!first) os << ", ";
                    os << var.name;
                    first = false;
                }
            }
            os << "}\n";
        }

        os << "  LiveOut: {";
        first = true;
        if (result.liveOut.count(blockId)) {
            for (const auto& var : result.liveOut.at(blockId)) {
                if (!first) os << ", ";
                os << var.name;
                first = false;
            }
        }
        os << "}\n";
    }
}

} // namespace liveness
} // namespace mdehra
