#include "mdehra/ir/Printer.hpp"
#include <iostream>
#include <algorithm>

namespace mdehra {
namespace ir {

void Printer::print(std::ostream& os, const Variable& var) {
    os << var.name << (var.isPointer ? "*" : "");
}

void Printer::print(std::ostream& os, const Field& field) {
    os << field.name;
}

void Printer::print(std::ostream& os, const Statement& stmt) {
    switch (stmt.getType()) {
        case StmtType::Assign: {
            const auto& s = static_cast<const AssignStmt&>(stmt);
            print(os, s.dest);
            os << " = ";
            print(os, s.src);
            break;
        }
        case StmtType::Load: {
            const auto& s = static_cast<const LoadStmt&>(stmt);
            print(os, s.dest);
            os << " = ";
            print(os, s.base);
            os << "->";
            print(os, s.field);
            break;
        }
        case StmtType::Store: {
            const auto& s = static_cast<const StoreStmt&>(stmt);
            print(os, s.base);
            os << "->";
            print(os, s.field);
            os << " = ";
            print(os, s.value);
            break;
        }
        case StmtType::Alloc: {
            const auto& s = static_cast<const AllocStmt&>(stmt);
            print(os, s.dest);
            os << " = alloc#" << s.allocSite;
            break;
        }
        case StmtType::Return: {
            const auto& s = static_cast<const ReturnStmt&>(stmt);
            os << "return";
            if (s.hasValue) {
                os << " ";
                print(os, s.value);
            }
            break;
        }
        case StmtType::Call: {
            os << "call(...)";
            break;
        }
        case StmtType::Unknown: {
            os << "unknown_stmt";
            break;
        }
    }
}

void Printer::print(std::ostream& os, const BasicBlock& block) {
    os << "Block B" << block.id << ":\n";
    for (const auto& stmt : block.statements) {
        os << "    ";
        print(os, *stmt);
        os << "\n";
    }
    
    if (!block.successors.empty()) {
        os << "    goto ";
        for (size_t i = 0; i < block.successors.size(); ++i) {
            if (i > 0) os << ", ";
            os << "B" << block.successors[i];
        }
        os << "\n";
    }
}

void Printer::print(std::ostream& os, const Function& function) {
    os << "Function: " << function.name << "\n\n";
    
    // Attempt to print blocks in deterministic order (by ID)
    std::vector<ID> blockIds;
    for (const auto& pair : function.blocks) {
        blockIds.push_back(pair.first);
    }
    std::sort(blockIds.begin(), blockIds.end());
    
    for (ID id : blockIds) {
        print(os, function.blocks.at(id));
        os << "\n";
    }
}

void Printer::print(std::ostream& os, const Program& program) {
    std::vector<ID> funcIds;
    for (const auto& pair : program.functions) {
        funcIds.push_back(pair.first);
    }
    std::sort(funcIds.begin(), funcIds.end());
    
    for (ID id : funcIds) {
        print(os, program.functions.at(id));
    }
}

} // namespace ir
} // namespace mdehra
