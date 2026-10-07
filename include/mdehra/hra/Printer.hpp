#pragma once
#include "mdehra/hra/HRAState.hpp"
#include <ostream>

namespace mdehra {
namespace hra {

class HRAPrinter {
public:
    static void print(std::ostream& os, const HRAState& state);
    static void print(std::ostream& os, const HRAGraph& graph);
};

} // namespace hra
} // namespace mdehra
