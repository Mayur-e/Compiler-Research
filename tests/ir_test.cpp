#include <iostream>
#include <sstream>
#include <cassert>
#include "mdehra/ir/Program.hpp"
#include "mdehra/ir/Printer.hpp"

using namespace mdehra::ir;

void testIR() {
    Program prog;
    Function func(1, "main");
    
    BasicBlock block1(1);
    Variable p{1, "p", true};
    Variable q{2, "q", true};
    Field next{1, "next"};

    block1.statements.push_back(std::make_unique<AllocStmt>(p, 100));
    block1.statements.push_back(std::make_unique<AssignStmt>(q, p));
    block1.statements.push_back(std::make_unique<StoreStmt>(p, next, q));
    block1.statements.push_back(std::make_unique<ReturnStmt>(q));
    
    func.blocks[block1.id] = std::move(block1);
    prog.functions[func.id] = std::move(func);

    std::stringstream ss;
    Printer::print(ss, prog);
    
    std::string out = ss.str();
    std::cout << "IR Output:\n" << out << std::endl;

    assert(out.find("Function: main") != std::string::npos);
    assert(out.find("p* = alloc#100") != std::string::npos);
    assert(out.find("q* = p*") != std::string::npos);
    assert(out.find("p*->next = q*") != std::string::npos);
    assert(out.find("return q*") != std::string::npos);
    
    std::cout << "IR test passed!" << std::endl;
}

int main() {
    testIR();
    return 0;
}
