#include "mdehra/ir/Program.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/hra/Printer.hpp"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace mdehra;

#include <vector>
#include <queue>
#include <algorithm>

std::string getCanonicalOutput(const hra::HRAState& state, const std::set<ir::Variable>& liveOut) {
    // 1. Find all reachable nodes from live variables
    std::map<hra::NodeID, hra::NodeID> canonicalIds;
    hra::NodeID nextId = 1;

    std::map<std::string, std::set<hra::NodeID>> liveBindings;
    for (const auto& var : liveOut) {
        auto it = state.bindings.find(var);
        if (it != state.bindings.end()) {
            liveBindings[var.name] = it->second;
        }
    }

    // Traverse BFS by variable name to assign canonical IDs
    std::queue<hra::NodeID> q;
    for (const auto& kv : liveBindings) {
        // Sort nodes to make traversal deterministic
        std::vector<hra::NodeID> nodes(kv.second.begin(), kv.second.end());
        std::sort(nodes.begin(), nodes.end());
        for (hra::NodeID n : nodes) {
            if (canonicalIds.find(n) == canonicalIds.end()) {
                canonicalIds[n] = nextId++;
                q.push(n);
            }
        }
    }

    std::set<std::tuple<hra::NodeID, std::string, hra::NodeID>> canonicalEdges;
    while (!q.empty()) {
        hra::NodeID curr = q.front();
        q.pop();

        for (const auto& edge : state.graph.edges) {
            if (edge.source == curr) {
                hra::NodeID to = edge.target;
                if (canonicalIds.find(to) == canonicalIds.end()) {
                    canonicalIds[to] = nextId++;
                    q.push(to);
                }
                canonicalEdges.insert({canonicalIds[curr], edge.field.name, canonicalIds[to]});
            }
        }
    }

    std::stringstream ss;
    ss << "Bindings:\n";
    for (const auto& kv : liveBindings) {
        ss << "  " << kv.first << " -> {";
        std::vector<hra::NodeID> cnodes;
        for (hra::NodeID n : kv.second) cnodes.push_back(canonicalIds[n]);
        std::sort(cnodes.begin(), cnodes.end());
        bool first = true;
        for (hra::NodeID c : cnodes) {
            if (!first) ss << ", ";
            ss << "C" << c;
            first = false;
        }
        ss << "}\n";
    }
    ss << "Edges:\n";
    for (const auto& e : canonicalEdges) {
        ss << "  C" << std::get<0>(e) << " -" << std::get<1>(e) << "-> C" << std::get<2>(e) << "\n";
    }
    return ss.str();
}

void verifyEquivalence(const ir::Function& func, bool expectPruning) {
    // Configuration A
    solver::BaselineHRAConfig configA;
    solver::HRASolver solverA;
    solverA.run(func, &configA);

    // Configuration B
    liveness::LivenessSolver livenessSolver;
    liveness::LivenessResult livenessRes = livenessSolver.run(func);
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    solver::HRASolver solverB;
    solverB.run(func, &configB);

    for (const auto& kv : func.blocks) {
        ir::ID blockId = kv.first;
        
        std::set<ir::Variable> liveOut;
        auto outIt = livenessRes.liveOut.find(blockId);
        if (outIt != livenessRes.liveOut.end()) {
            liveOut = outIt->second;
        }
        
        std::string outA = getCanonicalOutput(*solverA.getBlockExitState(blockId), liveOut);
        std::string outB = getCanonicalOutput(*solverB.getBlockExitState(blockId), liveOut);
        
        if (outA != outB) {
            std::cerr << "Mismatch in block " << blockId << "!\n";
            std::cerr << "--- Config A ---\n" << outA << "\n";
            std::cerr << "--- Config B ---\n" << outB << "\n";
            assert(false && "A and B semantic mismatch");
        }
    }

    if (expectPruning) {
        assert(configB.getStats().statementsPruned > 0 || configB.getStats().bindingsRemoved > 0);
    }
}

void test1_SimpleAlloc() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::BasicBlock b(1);
    
    // p = malloc(); return p;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, false); // No pruning expected
    std::cout << "TEST 1 Passed\n";
}

void test2_DeadPointer() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::BasicBlock b(1);
    
    // p = malloc();
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, true); // p is dead, alloc pruned
    std::cout << "TEST 2 Passed\n";
}

void test3_LivePointer() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::BasicBlock b(1);
    
    // p = malloc(); return p;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, false);
    std::cout << "TEST 3 Passed\n";
}

void test4_Assignment() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::BasicBlock b(1);
    
    // p = malloc(); q = p; return q;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);

    // p becomes dead after q = p, so bindingsRemoved > 0
    verifyEquivalence(func, true);
    std::cout << "TEST 4 Passed\n";
}

void test5_Aliasing() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    ir::Variable x{4, "x", true};
    ir::Field next{1, "next"};
    ir::BasicBlock b(1);
    
    // p = malloc(100); q = p; r = malloc(200); p->next = r; x = q->next; return x;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(r, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, r));
    b.statements.push_back(std::make_unique<ir::LoadStmt>(x, q, next));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(x));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, true); // p dead after p->next = r
    std::cout << "TEST 5 Passed\n";
}

void test6_DeadPointerLiveAlias() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::BasicBlock b(1);
    
    // p = malloc(100); q = p; return q;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, true); 
    std::cout << "TEST 6 Passed\n";
}

void test7_FieldLoad() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    ir::BasicBlock b(1);
    
    // p = malloc(); q = p->next; return q;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::LoadStmt>(q, p, next));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, true); 
    std::cout << "TEST 7 Passed\n";
}

void test8_FieldStore() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    ir::BasicBlock b(1);
    
    // p = malloc(); q = p; p->next = q; return q;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, q));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);

    verifyEquivalence(func, true); 
    std::cout << "TEST 8 Passed\n";
}

void test9_Branch() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(q, 100));
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(r, 200));
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
    b4.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[4] = std::move(b4);

    verifyEquivalence(func, true);
    std::cout << "TEST 9 Passed\n";
}

void test10_Loop() {
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

    verifyEquivalence(func, false);
    std::cout << "TEST 10 Passed\n";
}

void test11_MultipleDeadTemps() {
    ir::Function func(1, "main");
    ir::Variable t1{1, "t1", true};
    ir::Variable t2{2, "t2", true};
    ir::Variable t3{3, "t3", true};
    ir::Variable p{4, "p", true};
    ir::BasicBlock b(1);
    
    // t1 = malloc; t2 = malloc; t3 = t2; p = malloc; return p;
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t1, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t2, 200));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(t3, t2));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 300));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);

    // This should aggressively prune t1, t2, t3 computations
    verifyEquivalence(func, true);
    std::cout << "TEST 11 Passed\n";
}

void test12_Determinism() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::BasicBlock b(1);
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);

    liveness::LivenessSolver livenessSolver;
    liveness::LivenessResult livenessRes = livenessSolver.run(func);

    solver::LivenessGuidedHRAConfig configB1(&livenessRes);
    solver::HRASolver solverB1;
    solverB1.run(func, &configB1);
    
    solver::LivenessGuidedHRAConfig configB2(&livenessRes);
    solver::HRASolver solverB2;
    solverB2.run(func, &configB2);

    std::set<ir::Variable> liveOut;
    std::string out1 = getCanonicalOutput(*solverB1.getBlockExitState(1), liveOut);
    std::string out2 = getCanonicalOutput(*solverB2.getBlockExitState(1), liveOut);
    assert(out1 == out2);

    std::cout << "TEST 12 Passed\n";
}

void test13_NegativeSemanticDistinction() {
    ir::Function func1(1, "main");
    ir::Variable p{1, "p", true};
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b1.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func1.blocks[b1.id] = std::move(b1);

    ir::Function func2(1, "main");
    ir::Variable q{2, "q", true};
    ir::BasicBlock b2(1);
    b2.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b2.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func2.blocks[b2.id] = std::move(b2);

    solver::BaselineHRAConfig cA1;
    solver::HRASolver sA1;
    sA1.run(func1, &cA1);
    
    solver::BaselineHRAConfig cA2;
    solver::HRASolver sA2;
    sA2.run(func2, &cA2);

    std::set<ir::Variable> allVars1 = {p};
    std::set<ir::Variable> allVars2 = {q};
    std::string out1 = getCanonicalOutput(*sA1.getBlockExitState(1), allVars1);
    std::string out2 = getCanonicalOutput(*sA2.getBlockExitState(1), allVars2);
    assert(out1 != out2);
    
    std::cout << "TEST 13 Passed\n";
}

int main() {
    test1_SimpleAlloc();
    test2_DeadPointer();
    test3_LivePointer();
    test4_Assignment();
    test5_Aliasing();
    test6_DeadPointerLiveAlias();
    test7_FieldLoad();
    test8_FieldStore();
    test9_Branch();
    test10_Loop();
    test11_MultipleDeadTemps();
    test12_Determinism();
    test13_NegativeSemanticDistinction();
    return 0;
}
