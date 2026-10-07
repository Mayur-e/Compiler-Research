#pragma once
#include <ostream>
#include "mdehra/ir/Program.hpp"

namespace mdehra {
namespace ir {

class Printer {
public:
    static void print(std::ostream& os, const Program& program);
    static void print(std::ostream& os, const Function& function);
    static void print(std::ostream& os, const BasicBlock& block);
    static void print(std::ostream& os, const Statement& stmt);
    static void print(std::ostream& os, const Variable& var);
    static void print(std::ostream& os, const Field& field);
};

} // namespace ir
} // namespace mdehra
