#pragma once
#include "mdehra/liveness/LivenessResult.hpp"
#include "mdehra/ir/Program.hpp"
#include "mdehra/hra/HRAState.hpp"
#include <memory>
#include <deque>

namespace mdehra {
namespace solver {

struct HRASolverStats {
    // Semantic work
    size_t statesConstructed = 0;
    size_t entryStatesProduced = 0;
    size_t exitStatesProduced = 0;
    size_t blocksProcessed = 0;
    size_t worklistIterations = 0;
    
    // Transfer ops
    size_t transfersExecuted = 0;
    size_t allocsProcessed = 0;
    size_t assignsProcessed = 0;
    size_t loadsProcessed = 0;
    size_t storesProcessed = 0;

    // Liveness work
    size_t statementsExamined = 0;
    size_t statementsPruned = 0;
    size_t bindingsRemoved = 0;
    
    // Canonical reuse & performance
    size_t canonicalizeCalls = 0;
    size_t newCanonicalStates = 0;
    size_t reusedCanonicalStates = 0;
    double canonicalizeTimeMs = 0.0;
    size_t retainedStateBytes = 0;

    // High-water marks
    size_t peakWorklistSize = 0;   // maximum worklist queue length observed
    size_t peakHeapNodes   = 0;   // maximum abstract heap node count in any state
    size_t finalHeapNodes  = 0;   // heap node count in the final exit state
};

class HRAConfig {
public:
    virtual ~HRAConfig() = default;
    
    // Returns true if the statement's effect can be entirely skipped
    virtual bool shouldPruneStatement(const ir::Statement* stmt) const {
        return false;
    }
    
    // Allows the configuration to perform post-statement state reduction (e.g., live variable filtering)
    virtual void postProcessState(const ir::Statement* stmt, hra::HRAState& state, HRASolverStats& stats) const {}
    
    // Canonicalizes and deduplicates the state, returning a shared_ptr
    virtual std::shared_ptr<const hra::HRAState> canonicalizeState(const hra::HRAState& state) {
        return std::make_shared<hra::HRAState>(state);
    }
    
    virtual HRASolverStats& getStats() = 0;
    
    // Trace hooks (empty by default)
    virtual void traceBlockPop(ir::ID blockId, const std::deque<ir::ID>& worklist) {}
    virtual void traceBlockEntry(ir::ID blockId, const hra::HRAState& state) {}
    virtual void traceStatementPre(const ir::Statement* stmt, const hra::HRAState& state) {}
    virtual void traceStatementPost(const ir::Statement* stmt, const hra::HRAState& state, bool pruned) {}
    virtual void traceBlockExit(ir::ID blockId, const hra::HRAState& state) {}
    virtual void traceWorklistPush(ir::ID blockId, const std::deque<ir::ID>& worklist) {}
    virtual void traceCanonicalize(const hra::HRAState& original, std::shared_ptr<const hra::HRAState> canonical, bool isNew, size_t mdeIndex = 0) {}
};

class BaselineHRAConfig : public HRAConfig {
public:
    HRASolverStats stats;
    HRASolverStats& getStats() override { return stats; }
};

class LivenessGuidedHRAConfig : public HRAConfig {
public:
    const liveness::LivenessResult* liveness;
    HRASolverStats stats;

    LivenessGuidedHRAConfig(const liveness::LivenessResult* l) : liveness(l) {}
    
    HRASolverStats& getStats() override { return stats; }

    bool shouldPruneStatement(const ir::Statement* stmt) const override {
        if (!liveness) return false;

        switch (stmt->getType()) {
            case ir::StmtType::Alloc: {
                const auto* allocStmt = static_cast<const ir::AllocStmt*>(stmt);
                if (!liveness->isLiveAfter(stmt, allocStmt->dest)) {
                    return true;
                }
                break;
            }
            case ir::StmtType::Assign: {
                const auto* assignStmt = static_cast<const ir::AssignStmt*>(stmt);
                if (!liveness->isLiveAfter(stmt, assignStmt->dest)) {
                    return true;
                }
                break;
            }
            case ir::StmtType::Load: {
                const auto* loadStmt = static_cast<const ir::LoadStmt*>(stmt);
                if (!liveness->isLiveAfter(stmt, loadStmt->dest)) {
                    return true;
                }
                break;
            }
            case ir::StmtType::Store:
            case ir::StmtType::Return:
            case ir::StmtType::Call:
            case ir::StmtType::Unknown:
                break;
        }
        return false;
    }

    void postProcessState(const ir::Statement* stmt, hra::HRAState& state, HRASolverStats& statsRef) const override;
};

} // namespace solver
} // namespace mdehra
