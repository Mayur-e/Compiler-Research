#pragma once
#include "mdehra/ir/Program.hpp"
#include "mdehra/hra/HRAState.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include <map>

namespace mdehra {
namespace solver {

class HRASolver {
public:
    void run(const ir::Function& func, HRAConfig* config = nullptr);

    std::shared_ptr<const hra::HRAState> getBlockEntryState(ir::ID blockId) const;
    std::shared_ptr<const hra::HRAState> getBlockExitState(ir::ID blockId) const;

private:
    hra::HRAState transferBlock(const ir::BasicBlock& block, const hra::HRAState& inState, HRAConfig* config);
    void transferStatement(const ir::Statement* stmt, hra::HRAState& state, HRAConfig* config);

    std::map<ir::ID, std::shared_ptr<const hra::HRAState>> entryStates;
    std::map<ir::ID, std::shared_ptr<const hra::HRAState>> exitStates;
};

} // namespace solver
} // namespace mdehra
