#pragma once
#include "mdehra/ir/Program.hpp"
#include "mdehra/liveness/LivenessResult.hpp"

namespace mdehra {
namespace liveness {

class LivenessSolver {
public:
    LivenessResult run(const ir::Function& func);
private:
    void getUseDef(const ir::Statement* stmt, std::set<ir::Variable>& useSet, std::set<ir::Variable>& defSet);
};

} // namespace liveness
} // namespace mdehra
