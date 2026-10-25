#include "mdehra/liveness/LivenessSolver.hpp"
#include <deque>

namespace mdehra {
namespace liveness {

void LivenessSolver::getUseDef(const ir::Statement* stmt, std::set<ir::Variable>& useSet, std::set<ir::Variable>& defSet) {
    useSet.clear();
    defSet.clear();
    if (!stmt) return;

    switch (stmt->getType()) {
        case ir::StmtType::Alloc: {
            const auto* allocStmt = static_cast<const ir::AllocStmt*>(stmt);
            defSet.insert(allocStmt->dest);
            break;
        }
        case ir::StmtType::Assign: {
            const auto* assignStmt = static_cast<const ir::AssignStmt*>(stmt);
            defSet.insert(assignStmt->dest);
            if (!assignStmt->isNull) {
                useSet.insert(assignStmt->src);
            }
            break;
        }
        case ir::StmtType::Load: {
            const auto* loadStmt = static_cast<const ir::LoadStmt*>(stmt);
            defSet.insert(loadStmt->dest);
            useSet.insert(loadStmt->base);
            break;
        }
        case ir::StmtType::Store: {
            const auto* storeStmt = static_cast<const ir::StoreStmt*>(stmt);
            useSet.insert(storeStmt->base);
            if (!storeStmt->isNull) {
                useSet.insert(storeStmt->value);
            }
            break;
        }
        case ir::StmtType::Return: {
            const auto* retStmt = static_cast<const ir::ReturnStmt*>(stmt);
            if (retStmt->hasValue) {
                useSet.insert(retStmt->value);
            }
            break;
        }
        case ir::StmtType::Call: {
            const auto* callStmt = static_cast<const ir::CallStmt*>(stmt);
            for (const auto& arg : callStmt->arguments) {
                useSet.insert(arg);
            }
            break;
        }
        case ir::StmtType::Unknown:
            break;
    }
}

LivenessResult LivenessSolver::run(const ir::Function& func) {
    LivenessResult result;

    if (func.blocks.empty()) return result;

    std::deque<ir::ID> worklist;
    std::set<ir::ID> inWorklist;

    for (const auto& kv : func.blocks) {
        worklist.push_back(kv.first);
        inWorklist.insert(kv.first);
    }

    // Identify all exit blocks (blocks with no successors)
    std::vector<ir::ID> exitBlockIds;
    for (const auto& kv : func.blocks) {
        if (kv.second.successors.empty()) {
            exitBlockIds.push_back(kv.first);
        }
    }

    // Collect all returned variables in the function
    std::set<ir::Variable> allReturnVars;
    for (const auto& kv : func.blocks) {
        for (const auto& stmtPtr : kv.second.statements) {
            if (stmtPtr->getType() == ir::StmtType::Return) {
                const auto* retStmt = static_cast<const ir::ReturnStmt*>(stmtPtr.get());
                if (retStmt->hasValue) {
                    allReturnVars.insert(retStmt->value);
                }
            }
        }
    }

    // Seed all exit blocks' liveOut with returned variables
    for (ir::ID exitId : exitBlockIds) {
        result.liveOut[exitId].insert(allReturnVars.begin(), allReturnVars.end());
    }

    while (!worklist.empty()) {
        result.stats.iterations++;
        result.stats.blocksProcessed++;
        
        ir::ID blockId = worklist.front();
        worklist.pop_front();
        inWorklist.erase(blockId);

        const auto& block = func.blocks.at(blockId);

        std::set<ir::Variable> newLiveOut;
        for (ir::ID succId : block.successors) {
            auto it = result.liveIn.find(succId);
            if (it != result.liveIn.end()) {
                const auto& succLiveIn = it->second;
                newLiveOut.insert(succLiveIn.begin(), succLiveIn.end());
            }
        }
        
        // Add current block's seeded liveOut (e.g. from exit block seeding)
        auto seedIt = result.liveOut.find(blockId);
        if (seedIt != result.liveOut.end()) {
            newLiveOut.insert(seedIt->second.begin(), seedIt->second.end());
        }

        std::set<ir::Variable> currentLive = newLiveOut;
        
        // Traverse backwards
        for (auto it = block.statements.rbegin(); it != block.statements.rend(); ++it) {
            const ir::Statement* stmt = it->get();
            result.liveAfter[stmt] = currentLive;

            std::set<ir::Variable> useSet;
            std::set<ir::Variable> defSet;
            getUseDef(stmt, useSet, defSet);

            for (const auto& d : defSet) {
                currentLive.erase(d);
            }
            for (const auto& u : useSet) {
                currentLive.insert(u);
            }

            result.liveBefore[stmt] = currentLive;
        }

        bool changed = false;
        auto outIt = result.liveOut.find(blockId);
        if (outIt == result.liveOut.end() || outIt->second != newLiveOut) {
            result.liveOut[blockId] = newLiveOut;
            changed = true;
        }
        
        auto inIt = result.liveIn.find(blockId);
        if (inIt == result.liveIn.end() || inIt->second != currentLive) {
            result.liveIn[blockId] = currentLive;
            changed = true;
        }

        if (changed) {
            for (ir::ID predId : block.predecessors) {
                if (inWorklist.find(predId) == inWorklist.end()) {
                    worklist.push_back(predId);
                    inWorklist.insert(predId);
                }
            }
        }
    }

    return result;
}

} // namespace liveness
} // namespace mdehra
