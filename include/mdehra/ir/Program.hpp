#pragma once
#include <unordered_map>
#include <string>
#include "mdehra/ir/Function.hpp"
#include "mdehra/ir/Variable.hpp"

namespace mdehra {
namespace ir {

class Program {
public:
    std::unordered_map<ID, Function> functions;
    std::unordered_map<ID, Field> globalFields; // Field ID definitions

    Program() = default;
    
    // No copy
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    // Movable
    Program(Program&&) = default;
    Program& operator=(Program&&) = default;

    Function* getFunction(ID funcId) {
        auto it = functions.find(funcId);
        if (it != functions.end()) return &it->second;
        return nullptr;
    }

    const Function* getFunction(ID funcId) const {
        auto it = functions.find(funcId);
        if (it != functions.end()) return &it->second;
        return nullptr;
    }
};

} // namespace ir
} // namespace mdehra
