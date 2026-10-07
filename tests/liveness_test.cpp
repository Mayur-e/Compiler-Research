#include "mdehra/ir/Program.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/liveness/Printer.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace mdehra;
using namespace mdehra::liveness;

std::string getCanonicalOutput(const LivenessResult& result, const ir::Function& func) {
    std::stringstream ss;
    LivenessPrinter::print(ss, result, func);
    return ss.str();
}

void testSimpleUse() {
    // p = malloc(...)
    // return p
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    
    auto s1 = std::make_unique<ir::AllocStmt>(p, 100);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    auto s2 = std::make_unique<ir::ReturnStmt>(p);
    const ir::Statement* s2_ptr = s2.get();
    b.statements.push_back(std::move(s2));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(!res.isLiveBefore(s1_ptr, p));
    assert(res.isLiveAfter(s1_ptr, p));
    assert(res.isLiveBefore(s2_ptr, p));
    assert(res.isLiveAfter(s2_ptr, p));
    
    std::cout << "Test 1 Passed\n";
}

void testDeadPointer() {
    // p = malloc(...)
    // no later use
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    
    auto s1 = std::make_unique<ir::AllocStmt>(p, 100);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(!res.isLiveAfter(s1_ptr, p));
    
    std::cout << "Test 2 Passed\n";
}

void testAssignment() {
    // p = q;
    // return p;
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    
    auto s1 = std::make_unique<ir::AssignStmt>(p, q);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    auto s2 = std::make_unique<ir::ReturnStmt>(p);
    const ir::Statement* s2_ptr = s2.get();
    b.statements.push_back(std::move(s2));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveBefore(s1_ptr, q));
    assert(!res.isLiveBefore(s1_ptr, p));
    assert(res.isLiveAfter(s1_ptr, p));
    
    // p is returned, so it is live after the return statement
    assert(res.isLiveAfter(s2_ptr, p));
    
    std::cout << "Test 3 Passed\n";
}

void testRedefinition() {
    // p = q;
    // p = r;
    // return r;
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    
    auto s1 = std::make_unique<ir::AssignStmt>(p, q);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    auto s2 = std::make_unique<ir::AssignStmt>(p, r);
    const ir::Statement* s2_ptr = s2.get();
    b.statements.push_back(std::move(s2));
    
    auto s3 = std::make_unique<ir::ReturnStmt>(r);
    b.statements.push_back(std::move(s3));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    // p is never used, so it's not live anywhere
    assert(!res.isLiveAfter(s1_ptr, p));
    // q is used in s1 so it is live before s1, even though the store to p is dead
    assert(res.isLiveBefore(s1_ptr, q));
    
    std::cout << "Test 4 Passed\n";
}

void testFieldLoad() {
    // p = q->next;
    // return p;
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    
    auto s1 = std::make_unique<ir::LoadStmt>(p, q, next);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    auto s2 = std::make_unique<ir::ReturnStmt>(p);
    const ir::Statement* s2_ptr = s2.get();
    b.statements.push_back(std::move(s2));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveBefore(s1_ptr, q));
    assert(!res.isLiveBefore(s1_ptr, p));
    assert(res.isLiveAfter(s1_ptr, p));
    
    assert(res.isLiveAfter(s2_ptr, p));
    
    std::cout << "Test 5 Passed\n";
}

void testFieldStore() {
    // p->next = q;
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    
    auto s1 = std::make_unique<ir::StoreStmt>(p, next, q);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveBefore(s1_ptr, p));
    assert(res.isLiveBefore(s1_ptr, q));
    
    std::cout << "Test 6 Passed\n";
}

void testBranch() {
    // if (cond) return p; else return q;
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    
    ir::BasicBlock b1(1);
    b1.successors = {2, 3};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1};
    b2.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {1};
    b3.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[3] = std::move(b3);
    
    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveOut(1, p));
    assert(res.isLiveOut(1, q));
    assert(res.isLiveIn(2, p));
    assert(res.isLiveIn(3, q));
    
    std::cout << "Test 7 Passed\n";
}

void testLoop() {
    // b1: p = malloc();
    // b2 (loop): p = p->next;
    // b3 (exit): return p;
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Field next{1, "next"};
    
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b1.successors = {2};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1, 2}; // self loop
    b2.statements.push_back(std::make_unique<ir::LoadStmt>(p, p, next));
    b2.successors = {2, 3};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {2};
    b3.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[3] = std::move(b3);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveIn(2, p));
    assert(res.isLiveOut(2, p));
    assert(res.isLiveIn(3, p));
    assert(res.isLiveOut(3, p)); // live at exit due to return
    
    std::cout << "Test 8 Passed\n";
}

void testUseAfterLoop() {
    // b1: p = malloc()
    // b2 (loop): r = malloc()
    // b3 (exit): return p
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable r{2, "r", true};
    
    ir::BasicBlock b1(1);
    auto s1 = std::make_unique<ir::AllocStmt>(p, 100);
    const ir::Statement* s1_ptr = s1.get();
    b1.statements.push_back(std::move(s1));
    b1.successors = {2};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1, 2}; 
    b2.statements.push_back(std::make_unique<ir::AllocStmt>(r, 200));
    b2.successors = {2, 3};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {2};
    b3.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[3] = std::move(b3);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveIn(2, p));
    assert(res.isLiveOut(2, p)); // p goes through the loop
    assert(res.isLiveAfter(s1_ptr, p));
    
    std::cout << "Test 9 Passed\n";
}

void testReturn() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    
    auto s1 = std::make_unique<ir::ReturnStmt>(p);
    const ir::Statement* s1_ptr = s1.get();
    b.statements.push_back(std::move(s1));
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver;
    LivenessResult res = solver.run(func);

    assert(res.isLiveBefore(s1_ptr, p));
    assert(res.isLiveAfter(s1_ptr, p));
    
    std::cout << "Test 10 Passed\n";
}

void testDeterminism() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);

    LivenessSolver solver1, solver2;
    LivenessResult res1 = solver1.run(func);
    LivenessResult res2 = solver2.run(func);

    std::string out1 = getCanonicalOutput(res1, func);
    std::string out2 = getCanonicalOutput(res2, func);
    assert(out1 == out2);
    std::cout << "Test 11 Passed\n";
}

void testNegativeDistinction() {
    ir::Function func1(1, "main");
    ir::BasicBlock b1(1);
    ir::Variable p{1, "p", true};
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b1.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func1.blocks[b1.id] = std::move(b1);

    ir::Function func2(1, "main");
    ir::BasicBlock b2(1);
    ir::Variable q{2, "q", true};
    b2.statements.push_back(std::make_unique<ir::AllocStmt>(q, 100));
    // missing return, q is dead
    func2.blocks[b2.id] = std::move(b2);

    LivenessSolver solver;
    std::string out1 = getCanonicalOutput(solver.run(func1), func1);
    std::string out2 = getCanonicalOutput(solver.run(func2), func2);
    
    assert(out1 != out2);
    std::cout << "Test 12 Passed\n";
}

int main() {
    testSimpleUse();
    testDeadPointer();
    testAssignment();
    testRedefinition();
    testFieldLoad();
    testFieldStore();
    testBranch();
    testLoop();
    testUseAfterLoop();
    testReturn();
    testDeterminism();
    testNegativeDistinction();
    return 0;
}
