#pragma once
#include "mdehra/ir/ProgramPoint.hpp"
#include "mdehra/ir/Variable.hpp"
#include "mdehra/ir/Statement.hpp"
#include <set>
#include <map>

namespace mdehra {
namespace liveness {

struct LivenessResult {
    struct LivenessStats {
        size_t blocksProcessed = 0;
        size_t iterations = 0;
    };
    LivenessStats stats;

    // block ID -> live variables
    std::map<ir::ID, std::set<ir::Variable>> liveIn;
    std::map<ir::ID, std::set<ir::Variable>> liveOut;

    // Statement -> live variables
    std::map<const ir::Statement*, std::set<ir::Variable>> liveBefore;
    std::map<const ir::Statement*, std::set<ir::Variable>> liveAfter;

    bool isLiveIn(ir::ID blockId, const ir::Variable& var) const {
        auto it = liveIn.find(blockId);
        return it != liveIn.end() && it->second.count(var) > 0;
    }

    bool isLiveOut(ir::ID blockId, const ir::Variable& var) const {
        auto it = liveOut.find(blockId);
        return it != liveOut.end() && it->second.count(var) > 0;
    }

    bool isLiveBefore(const ir::Statement* stmt, const ir::Variable& var) const {
        auto it = liveBefore.find(stmt);
        return it != liveBefore.end() && it->second.count(var) > 0;
    }

    bool isLiveAfter(const ir::Statement* stmt, const ir::Variable& var) const {
        auto it = liveAfter.find(stmt);
        return it != liveAfter.end() && it->second.count(var) > 0;
    }
};

} // namespace liveness
} // namespace mdehra
