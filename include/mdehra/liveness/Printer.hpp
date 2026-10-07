#pragma once
#include "mdehra/liveness/LivenessResult.hpp"
#include "mdehra/ir/Program.hpp"
#include <ostream>

namespace mdehra {
namespace liveness {

class LivenessPrinter {
public:
    static void print(std::ostream& os, const LivenessResult& result, const ir::Function& func);
};

} // namespace liveness
} // namespace mdehra
