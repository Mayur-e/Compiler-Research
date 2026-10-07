#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "mdehra/ir/BasicBlock.hpp"
#include "mdehra/ir/Variable.hpp"

namespace mdehra {
namespace ir {

class Function {
public:
    ID id = INVALID_ID;
    std::string name;
    ID entryBlockId = INVALID_ID;
    
    // Owns the basic blocks
    std::unordered_map<ID, BasicBlock> blocks;
    
    // Variables local to this function (including params)
    std::vector<Variable> parameters;
    std::unordered_map<ID, Variable> localVariables;

    Function() = default;
    Function(ID funcId, std::string funcName) : id(funcId), name(std::move(funcName)) {}

    // No copy
    Function(const Function&) = delete;
    Function& operator=(const Function&) = delete;

    // Movable
    Function(Function&&) = default;
    Function& operator=(Function&&) = default;
    
    BasicBlock* getBlock(ID blockId) {
        auto it = blocks.find(blockId);
        if (it != blocks.end()) return &it->second;
        return nullptr;
    }
    
    const BasicBlock* getBlock(ID blockId) const {
        auto it = blocks.find(blockId);
        if (it != blocks.end()) return &it->second;
        return nullptr;
    }
};

} // namespace ir
} // namespace mdehra
