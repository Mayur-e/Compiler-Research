#include "mdehra/solver/HRASolver.hpp"
#include <deque>
#include <set>

namespace mdehra {
namespace solver {

void HRASolver::run(const ir::Function& func, HRAConfig* config) {
    entryStates.clear();
    exitStates.clear();

    if (func.blocks.empty()) return;

    std::deque<ir::ID> worklist;
    std::set<ir::ID> inWorklist;

    // Initialize all blocks and add to worklist
    for (const auto& kv : func.blocks) {
        ir::ID blockId = kv.first;
        worklist.push_back(blockId);
        inWorklist.insert(blockId);
    }

    while (!worklist.empty()) {
        if (config) {
            config->getStats().worklistIterations++;
            config->getStats().blocksProcessed++;
            // Phase 15: high-water mark for worklist
            if (worklist.size() > config->getStats().peakWorklistSize)
                config->getStats().peakWorklistSize = worklist.size();
        }
        ir::ID blockId = worklist.front();
        worklist.pop_front();
        inWorklist.erase(blockId);
        
        if (config) config->traceBlockPop(blockId, worklist);


        const auto& block = func.blocks.at(blockId);

        // Merge predecessor exit states to form entry state
        hra::HRAState newEntryState;
        for (ir::ID predId : block.predecessors) {
            auto it = exitStates.find(predId);
            if (it != exitStates.end()) {
                newEntryState.merge(*(it->second));
            }
        }
        
        if (config) config->getStats().statesConstructed++;
        
        std::shared_ptr<const hra::HRAState> canonicalEntry = config ? config->canonicalizeState(newEntryState) 
                                                                     : std::make_shared<hra::HRAState>(newEntryState);
        entryStates[blockId] = canonicalEntry;
        if (config) config->getStats().entryStatesProduced++;
        
        if (config) config->traceBlockEntry(blockId, *canonicalEntry);

        hra::HRAState newExitState = transferBlock(block, *canonicalEntry, config);
        
        if (config) config->getStats().statesConstructed++;
        
        std::shared_ptr<const hra::HRAState> canonicalExit = config ? config->canonicalizeState(newExitState)
                                                                    : std::make_shared<hra::HRAState>(newExitState);

        auto exitIt = exitStates.find(blockId);
        // Fast path: pointer equality (true if MDE config perfectly deduplicates)
        // Slow path: deep semantic equality
        if (exitIt == exitStates.end() || (exitIt->second != canonicalExit && *(exitIt->second) != *canonicalExit)) {
            exitStates[blockId] = canonicalExit;
            if (config) config->getStats().exitStatesProduced++;
            
            for (ir::ID succId : block.successors) {
                if (inWorklist.find(succId) == inWorklist.end()) {
                    worklist.push_back(succId);
                    inWorklist.insert(succId);
                    if (config) config->traceWorklistPush(succId, worklist);
                    // Phase 15: track peak after push
                    if (config && worklist.size() > config->getStats().peakWorklistSize)
                        config->getStats().peakWorklistSize = worklist.size();
                }
            }
        }
        
        if (config) {
            config->traceBlockExit(blockId, *canonicalExit);
            // Phase 15: peak heap nodes
            size_t nodeCount = canonicalExit->graph.nodes.size();
            if (nodeCount > config->getStats().peakHeapNodes)
                config->getStats().peakHeapNodes = nodeCount;
        }
    }
}

std::shared_ptr<const hra::HRAState> HRASolver::getBlockEntryState(ir::ID blockId) const {
    return entryStates.at(blockId);
}

std::shared_ptr<const hra::HRAState> HRASolver::getBlockExitState(ir::ID blockId) const {
    return exitStates.at(blockId);
}

std::shared_ptr<const hra::HRAState> HRASolver::getFunctionExitState(const ir::Function& func) const {
    hra::HRAState merged;
    bool found = false;
    for (const auto& kv : func.blocks) {
        if (kv.second.successors.empty()) {
            auto it = exitStates.find(kv.first);
            if (it != exitStates.end() && it->second) {
                merged.merge(*(it->second));
                found = true;
            }
        }
    }
    if (!found) {
        if (!exitStates.empty()) {
            return exitStates.rbegin()->second;
        }
        return nullptr;
    }
    return std::make_shared<hra::HRAState>(std::move(merged));
}

hra::HRAState HRASolver::transferBlock(const ir::BasicBlock& block, const hra::HRAState& inState, HRAConfig* config) {
    hra::HRAState state = inState;
    for (const auto& stmt : block.statements) {
        transferStatement(stmt.get(), state, config);
    }
    return state;
}

void HRASolver::transferStatement(const ir::Statement* stmt, hra::HRAState& state, HRAConfig* config) {
    if (!stmt) return;

    if (config) config->traceStatementPre(stmt, state);

    if (config) {
        config->getStats().statementsExamined++;
        if (config->shouldPruneStatement(stmt)) {
            config->getStats().statementsPruned++;
            // Still run post-processing so that dead variables are removed
            config->postProcessState(stmt, state, config->getStats());
            if (config) config->traceStatementPost(stmt, state, true);
            return;
        }
    }

    if (config) config->getStats().transfersExecuted++;

    switch (stmt->getType()) {
        case ir::StmtType::Alloc: {
            if (config) config->getStats().allocsProcessed++;
            const auto* allocStmt = static_cast<const ir::AllocStmt*>(stmt);
            state.graph.addNode(allocStmt->allocSite);
            state.bindings[allocStmt->dest].clear(); // kill previous binding
            state.bindings[allocStmt->dest].insert(allocStmt->allocSite);
            break;
        }
        case ir::StmtType::Assign: {
            if (config) config->getStats().assignsProcessed++;
            const auto* assignStmt = static_cast<const ir::AssignStmt*>(stmt);
            if (assignStmt->isNull) {
                state.bindings[assignStmt->dest].clear();
            } else {
                state.bindings[assignStmt->dest] = state.bindings[assignStmt->src];
            }
            break;
        }
        case ir::StmtType::Load: {
            if (config) config->getStats().loadsProcessed++;
            const auto* loadStmt = static_cast<const ir::LoadStmt*>(stmt);
            state.bindings[loadStmt->dest].clear();
            
            auto it = state.bindings.find(loadStmt->base);
            if (it != state.bindings.end()) {
                const auto& qNodes = it->second;
                for (auto srcNode : qNodes) {
                    for (const auto& edge : state.graph.edges) {
                        if (edge.source == srcNode && edge.field == loadStmt->field) {
                            state.bindings[loadStmt->dest].insert(edge.target);
                        }
                    }
                }
            }
            break;
        }
        case ir::StmtType::Store: {
            if (config) config->getStats().storesProcessed++;
            const auto* storeStmt = static_cast<const ir::StoreStmt*>(stmt);
            auto pIt = state.bindings.find(storeStmt->base);
            if (pIt != state.bindings.end()) {
                const auto& pNodes = pIt->second;
                if (storeStmt->isNull) {
                    for (auto pNode : pNodes) {
                        state.graph.removeEdges(pNode, storeStmt->field);
                    }
                } else {
                    auto qIt = state.bindings.find(storeStmt->value);
                    if (qIt != state.bindings.end()) {
                        const auto& qNodes = qIt->second;
                        for (auto pNode : pNodes) {
                            for (auto qNode : qNodes) {
                                state.graph.addEdge(pNode, storeStmt->field, qNode);
                            }
                        }
                    }
                }
            }
            break;
        }
        case ir::StmtType::Return:
        case ir::StmtType::Call:
        case ir::StmtType::Unknown:
            break;
    }

    if (config) {
        config->postProcessState(stmt, state, config->getStats());
    }
    
    if (config) config->traceStatementPost(stmt, state, false);
}

} // namespace solver
} // namespace mdehra
