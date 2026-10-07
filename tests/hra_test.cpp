#include "mdehra/ir/Program.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/hra/Printer.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace mdehra;

std::string getCanonicalOutput(const hra::HRAState& state) {
    std::stringstream ss;
    hra::HRAPrinter::print(ss, state);
    return ss.str();
}

void testSingleAllocation() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    std::string out = getCanonicalOutput(state);
    
    assert(state.graph.nodes.size() == 1);
    assert(state.bindings.at(p).size() == 1);
    assert(out.find("p -> {N100}") != std::string::npos);
    std::cout << "Test 1 Passed\n";
}

void testTwoAllocations() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    assert(state.graph.nodes.size() == 2);
    assert(state.bindings.at(p).size() == 1);
    assert(state.bindings.at(q).size() == 1);
    std::cout << "Test 2 Passed\n";
}

void testPointerAssignment() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    assert(state.bindings.at(p) == state.bindings.at(q));
    std::cout << "Test 3 Passed\n";
}

void testFieldStore() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, q));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    assert(state.graph.edges.size() == 1);
    auto edge = *state.graph.edges.begin();
    assert(edge.source == 100 && edge.target == 200 && edge.field.name == "next");
    std::cout << "Test 4 Passed\n";
}

void testFieldLoad() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    ir::Field next{1, "next"};
    
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, q));
    b.statements.push_back(std::make_unique<ir::LoadStmt>(r, p, next));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    assert(state.bindings.at(r).count(200));
    std::cout << "Test 5 Passed\n";
}

void testLinkedListSequence() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p1{1, "p1", true};
    ir::Variable p2{2, "p2", true};
    ir::Variable p3{3, "p3", true};
    ir::Field next{1, "next"};
    
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p1, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p2, 200));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p3, 300));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p1, next, p2));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p2, next, p3));
    
    ir::Variable t1{4, "t1", true};
    ir::Variable t2{5, "t2", true};
    b.statements.push_back(std::make_unique<ir::LoadStmt>(t1, p1, next));
    b.statements.push_back(std::make_unique<ir::LoadStmt>(t2, t1, next));
    
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(1);
    assert(state.bindings.at(t2).count(300));
    std::cout << "Test 6 Passed\n";
}

void testBranchJoin() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(r, 300));
    b1.successors = {2, 3};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1};
    b2.statements.push_back(std::make_unique<ir::AssignStmt>(p, q));
    b2.successors = {4};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {1};
    b3.statements.push_back(std::make_unique<ir::AssignStmt>(p, r));
    b3.successors = {4};
    func.blocks[3] = std::move(b3);
    
    ir::BasicBlock b4(4);
    b4.predecessors = {2, 3};
    func.blocks[4] = std::move(b4);

    solver::HRASolver solver;
    solver.run(func);

    const auto& state = *solver.getBlockExitState(4);
    assert(state.bindings.at(p).size() == 2);
    assert(state.bindings.at(p).count(200));
    assert(state.bindings.at(p).count(300));
    std::cout << "Test 7 Passed\n";
}

void testLoopTermination() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Field next{1, "next"};
    
    ir::BasicBlock b1(1); // p = malloc
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b1.successors = {2};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2); // loop header
    b2.predecessors = {1, 3};
    b2.successors = {3, 4};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3); // loop body: p = p->next
    b3.predecessors = {2};
    b3.statements.push_back(std::make_unique<ir::LoadStmt>(p, p, next));
    b3.successors = {2};
    func.blocks[3] = std::move(b3);
    
    ir::BasicBlock b4(4); // exit
    b4.predecessors = {2};
    func.blocks[4] = std::move(b4);

    solver::HRASolver solver;
    solver.run(func); // Must terminate

    const auto& state = *solver.getBlockExitState(4);
    std::cout << "Test 8 Passed\n";
}

void testDeterminism() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, q));
    func.blocks[b.id] = std::move(b);

    solver::HRASolver solver1, solver2;
    solver1.run(func);
    solver2.run(func);

    std::string out1 = getCanonicalOutput(*solver1.getBlockExitState(1));
    std::string out2 = getCanonicalOutput(*solver2.getBlockExitState(1));
    assert(out1 == out2);
    std::cout << "Test 9 Passed\n";
}

void testNegativeSemanticDistinction() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    
    state1.bindings[p].insert(100);
    state2.bindings[p].insert(200); // Different allocation site
    
    std::string out1 = getCanonicalOutput(state1);
    std::string out2 = getCanonicalOutput(state2);
    assert(out1 != out2);
    assert(state1 != state2);
    std::cout << "Test 10 Passed\n";
}

int main() {
    testSingleAllocation();
    testTwoAllocations();
    testPointerAssignment();
    testFieldStore();
    testFieldLoad();
    testLinkedListSequence();
    testBranchJoin();
    testLoopTermination();
    testDeterminism();
    testNegativeSemanticDistinction();
    return 0;
}
