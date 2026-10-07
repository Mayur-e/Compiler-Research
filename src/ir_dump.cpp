// CFG and IR diagnostic: print the full block structure including
// predecessors/successors for a given source file.
// Used to evidence back-edges in BM08.
//
// Build: already compiled as a separate test binary via tests/CMakeLists.txt
// Usage: ir_dump_driver <source_file>

#include "mdehra/ir/Program.hpp"
#include "mdehra/ir/Printer.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace mdehra;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>\n";
        return 1;
    }

    std::string sourceFile = argv[1];
    std::ifstream ifs(sourceFile);
    if (!ifs.is_open()) {
        std::cerr << "Cannot open: " << sourceFile << "\n";
        return 1;
    }

    std::string code((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());
    std::string fullCode =
        "void* __builtin_malloc(unsigned long long);\n"
        "struct Node { struct Node* next; struct Node* prev; int data; };\n"
        + code;

    auto ast = clang::tooling::buildASTFromCode(fullCode);
    if (!ast) {
        std::cerr << "Clang parse failed.\n";
        return 1;
    }

    auto program = frontend::ClangFrontend::translate(ast->getASTContext());

    // Print full IR with block structure
    for (const auto& kv : program->functions) {
        const auto& func = kv.second;
        std::cout << "=== Function: " << func.name << " ===\n";
        std::cout << "  Entry block: " << func.entryBlockId << "\n";
        std::cout << "  Total blocks: " << func.blocks.size() << "\n\n";

        for (const auto& bkv : func.blocks) {
            const auto& block = bkv.second;
            std::cout << "  Block B" << block.id << ":\n";

            std::cout << "    Predecessors: [";
            for (size_t i = 0; i < block.predecessors.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << "B" << block.predecessors[i];
            }
            std::cout << "]\n";

            std::cout << "    Successors:   [";
            for (size_t i = 0; i < block.successors.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << "B" << block.successors[i];
            }
            std::cout << "]\n";

            std::cout << "    Statements (" << block.statements.size() << "):\n";
            for (const auto& stmtPtr : block.statements) {
                const ir::Statement* stmt = stmtPtr.get();
                switch (stmt->getType()) {
                    case ir::StmtType::Alloc: {
                        const auto* s = static_cast<const ir::AllocStmt*>(stmt);
                        std::cout << "      Alloc: " << s->dest.name
                                  << " = alloc#" << s->allocSite << "\n";
                        break;
                    }
                    case ir::StmtType::Assign: {
                        const auto* s = static_cast<const ir::AssignStmt*>(stmt);
                        std::cout << "      Assign: " << s->dest.name
                                  << " = " << s->src.name << "\n";
                        break;
                    }
                    case ir::StmtType::Load: {
                        const auto* s = static_cast<const ir::LoadStmt*>(stmt);
                        std::cout << "      Load: " << s->dest.name
                                  << " = " << s->base.name
                                  << "->" << s->field.name << "\n";
                        break;
                    }
                    case ir::StmtType::Store: {
                        const auto* s = static_cast<const ir::StoreStmt*>(stmt);
                        std::cout << "      Store: " << s->base.name
                                  << "->" << s->field.name
                                  << " = " << s->value.name << "\n";
                        break;
                    }
                    case ir::StmtType::Return: {
                        const auto* s = static_cast<const ir::ReturnStmt*>(stmt);
                        if (s->hasValue)
                            std::cout << "      Return: " << s->value.name << "\n";
                        else
                            std::cout << "      Return (void)\n";
                        break;
                    }
                    case ir::StmtType::Call:
                        std::cout << "      Call (unsupported)\n";
                        break;
                    case ir::StmtType::Unknown:
                        std::cout << "      Unknown/dropped statement\n";
                        break;
                }
            }

            // Detect back-edges: successor has smaller or equal block ID
            // and also appears in this block's successors from predecessor side
            for (ir::ID succId : block.successors) {
                bool isBackEdge = false;
                // A successor is a back-edge if the successor block has
                // this block in its predecessors AND successor's ID <= this block's ID
                // (heuristic for Clang CFG numbering) OR if successor is same block
                auto succIt = func.blocks.find(succId);
                if (succIt != func.blocks.end()) {
                    const auto& succBlock = succIt->second;
                    for (ir::ID predId : succBlock.predecessors) {
                        if (predId == block.id && succId <= block.id) {
                            isBackEdge = true;
                        }
                    }
                }
                if (isBackEdge) {
                    std::cout << "    *** BACK-EDGE: B" << block.id
                              << " -> B" << succId << " ***\n";
                }
            }
            std::cout << "\n";
        }
    }

    return 0;
}
