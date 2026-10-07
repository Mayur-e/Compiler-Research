#include <iostream>
#include <sstream>
#include <cassert>
#include "clang/Tooling/Tooling.h"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/ir/Printer.hpp"

using namespace mdehra::frontend;
using namespace mdehra::ir;

void testFrontend() {
    std::string code = R"(
        struct Node {
            struct Node* next;
        };
        void test() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            p->next = q;
            q = p->next;
        }
    )";

    std::unique_ptr<clang::ASTUnit> ast = clang::tooling::buildASTFromCode(code);
    assert(ast != nullptr && "Failed to build AST from code");

    auto program = ClangFrontend::translate(ast->getASTContext());
    assert(program != nullptr && "Failed to translate to IR");

    std::stringstream ss;
    Printer::print(ss, *program);
    std::string out = ss.str();

    std::cout << "Frontend IR Output:\n" << out << std::endl;

    assert(out.find("Function: test") != std::string::npos);
    assert(out.find("alloc#") != std::string::npos);
    assert(out.find("->next =") != std::string::npos); // Store
    assert(out.find("= ") != std::string::npos); // Assignments and Loads
    
    std::cout << "Frontend test passed!" << std::endl;
}

int main() {
    testFrontend();
    return 0;
}
