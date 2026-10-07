#pragma once
#include <vector>
#include <memory>
#include "mdehra/ir/ProgramPoint.hpp"
#include "mdehra/ir/Statement.hpp"

namespace mdehra {
namespace ir {

class BasicBlock {
public:
    ID id = INVALID_ID;
    std::vector<std::unique_ptr<Statement>> statements;
    std::vector<ID> predecessors;
    std::vector<ID> successors;

    BasicBlock() = default;
    BasicBlock(ID blockId) : id(blockId) {}

    // No copy
    BasicBlock(const BasicBlock&) = delete;
    BasicBlock& operator=(const BasicBlock&) = delete;

    // Movable
    BasicBlock(BasicBlock&&) = default;
    BasicBlock& operator=(BasicBlock&&) = default;
};

} // namespace ir
} // namespace mdehra
