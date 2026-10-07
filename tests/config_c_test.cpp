#include "mdehra/ir/Program.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include <iostream>
#include <cassert>
#include <sstream>
#include <queue>
#include <algorithm>

using namespace mdehra;

std::string getCanonicalOutput(const hra::HRAState& state, const std::set<ir::Variable>& liveOut) {
    std::map<hra::NodeID, hra::NodeID> canonicalIds;
    hra::NodeID nextId = 1;

    std::map<std::string, std::set<hra::NodeID>> liveBindings;
    for (const auto& var : liveOut) {
        auto it = state.bindings.find(var);
        if (it != state.bindings.end()) {
            liveBindings[var.name] = it->second;
        }
    }

    std::queue<hra::NodeID> q;
    for (const auto& kv : liveBindings) {
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

void verifyEquivalence(const ir::Function& func) {
    liveness::LivenessSolver livenessSolver;
    liveness::LivenessResult livenessRes = livenessSolver.run(func);

    // Configuration B: Liveness + HRA
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    solver::HRASolver solverB;
    solverB.run(func, &configB);

    // Configuration C: Liveness + HRA + MDE
    mde_adapter::MDEGuidedHRAConfig configC(&livenessRes);
    solver::HRASolver solverC;
    solverC.run(func, &configC);

    for (const auto& kv : func.blocks) {
        ir::ID blockId = kv.first;
        
        std::set<ir::Variable> liveOut;
        auto outIt = livenessRes.liveOut.find(blockId);
        if (outIt != livenessRes.liveOut.end()) {
            liveOut = outIt->second;
        }
        
        std::string outB = getCanonicalOutput(*solverB.getBlockExitState(blockId), liveOut);
        std::string outC = getCanonicalOutput(*solverC.getBlockExitState(blockId), liveOut);
        
        if (outB != outC) {
            std::cerr << "Mismatch in block " << blockId << "!\n";
            std::cerr << "--- Config B ---\n" << outB << "\n";
            std::cerr << "--- Config C ---\n" << outC << "\n";
            assert(false && "B and C semantic mismatch");
        }
    }
    
    // We expect some MDE reuse if the program does anything meaningful
    // Not strictly true for empty program, so we just print stats
    std::cout << "  MDE Hits: " << configC.mdeHits << " | Misses: " << configC.mdeMisses << "\n";
}

// 1. Empty program/state
void test1_Empty() {
    ir::Function func(1, "main");
    verifyEquivalence(func);
    std::cout << "Test 1 (Empty) Passed\n";
}

// 2. Single allocation
void test2_SingleAlloc() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 2 (SingleAlloc) Passed\n";
}

// 3. Dead allocation
void test3_DeadAlloc() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100)); // Dead
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 3 (DeadAlloc) Passed\n";
}

// 4. Live allocation (same as test 2 but returning something else, testing semantics)
void test4_LiveAlloc() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 4 (LiveAlloc) Passed\n";
}

// 5. Pointer assignment
void test5_PtrAssign() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 5 (PtrAssign) Passed\n";
}

// 6. Aliasing
void test6_Aliasing() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(q, p));
    b.statements.push_back(std::make_unique<ir::AssignStmt>(r, q));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(r));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 6 (Aliasing) Passed\n";
}

// 7. Field store
void test7_FieldStore() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field f{1, "f"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, f, q));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 7 (FieldStore) Passed\n";
}

// 8. Field load
void test8_FieldLoad() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    ir::Field f{1, "f"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, f, q));
    b.statements.push_back(std::make_unique<ir::LoadStmt>(r, p, f));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(r));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 8 (FieldLoad) Passed\n";
}

// 9. Linked-list construction
void test9_LinkedList() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable t1{2, "t1", true};
    ir::Variable t2{3, "t2", true};
    ir::Field next{1, "next"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t1, 200));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t2, 300));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, t1));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(t1, next, t2));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 9 (LinkedList) Passed\n";
}

// 10. Branch
void test10_Branch() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(q, 100));
    b1.successors = {2, 3};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1};
    b2.statements.push_back(std::make_unique<ir::AssignStmt>(p, q));
    b2.successors = {4};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {1};
    b3.statements.push_back(std::make_unique<ir::AllocStmt>(p, 200));
    b3.successors = {4};
    func.blocks[3] = std::move(b3);
    
    ir::BasicBlock b4(4);
    b4.predecessors = {2, 3};
    b4.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[4] = std::move(b4);

    verifyEquivalence(func);
    std::cout << "Test 10 (Branch) Passed\n";
}

// 11. Loop
void test11_Loop() {
    ir::Function func(1, "main");
    ir::Variable p{1, "p", true};
    ir::Field next{1, "next"};
    
    ir::BasicBlock b1(1);
    b1.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b1.successors = {2};
    func.blocks[1] = std::move(b1);
    
    ir::BasicBlock b2(2);
    b2.predecessors = {1, 3};
    b2.successors = {3, 4};
    func.blocks[2] = std::move(b2);
    
    ir::BasicBlock b3(3);
    b3.predecessors = {2};
    b3.statements.push_back(std::make_unique<ir::LoadStmt>(p, p, next));
    b3.successors = {2};
    func.blocks[3] = std::move(b3);
    
    ir::BasicBlock b4(4);
    b4.predecessors = {2};
    b4.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[4] = std::move(b4);

    verifyEquivalence(func);
    std::cout << "Test 11 (Loop) Passed\n";
}

// 12. Dead temporary
void test12_DeadTemp() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable t{2, "t", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t, 200)); // dead
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 12 (DeadTemp) Passed\n";
}

// 13. Multiple dead temporaries
void test13_MultipleDeadTemps() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable t1{2, "t1", true};
    ir::Variable t2{3, "t2", true};
    ir::Variable t3{4, "t3", true};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t1, 200)); 
    b.statements.push_back(std::make_unique<ir::AllocStmt>(t2, 300)); 
    b.statements.push_back(std::make_unique<ir::AssignStmt>(t3, t2)); 
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 13 (MultipleDeadTemps) Passed\n";
}

// 14. Shared heap target
void test14_SharedTarget() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Variable r{3, "r", true};
    ir::Field f1{1, "f1"};
    ir::Field f2{2, "f2"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(r, 300));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, f1, r));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(q, f2, r));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(q));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 14 (SharedTarget) Passed\n";
}

// 15. Self-cycle
void test15_SelfCycle() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Field next{1, "next"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 15 (SelfCycle) Passed\n";
}

// 16. Two-node cycle
void test16_TwoNodeCycle() {
    ir::Function func(1, "main");
    ir::BasicBlock b(1);
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    ir::Field next{1, "next"};
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100));
    b.statements.push_back(std::make_unique<ir::AllocStmt>(q, 200));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(p, next, q));
    b.statements.push_back(std::make_unique<ir::StoreStmt>(q, next, p));
    b.statements.push_back(std::make_unique<ir::ReturnStmt>(p));
    func.blocks[b.id] = std::move(b);
    verifyEquivalence(func);
    std::cout << "Test 16 (TwoNodeCycle) Passed\n";
}

// 17. Negative - Different field labels
void test17_DiffFieldLabel() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    state1.bindings[p] = {100};
    state2.bindings[p] = {100};
    
    ir::Field f1{1, "f1"};
    ir::Field f2{2, "f2"};
    
    state1.graph.addEdge(100, f1, 200);
    state2.graph.addEdge(100, f2, 200);
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(nullptr);
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) != config.encodeState(state2));
    assert(p1.get() != p2.get());
    std::cout << "Test 17 (DiffFieldLabel) Passed\n";
}

// 18. Negative - Different allocation sites
void test18_DiffAllocSites() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    state1.bindings[p] = {100};
    state2.bindings[p] = {200}; // diff root alloc
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(nullptr);
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) != config.encodeState(state2));
    assert(p1.get() != p2.get());
    std::cout << "Test 18 (DiffAllocSites) Passed\n";
}

// 19. Negative - Different graph topology
void test19_DiffTopology() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    state1.bindings[p] = {100};
    state2.bindings[p] = {100};
    
    ir::Field next{1, "next"};
    state1.graph.addEdge(100, next, 200);
    state2.graph.addEdge(100, next, 300); // points to 300 instead
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(nullptr);
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) != config.encodeState(state2));
    assert(p1.get() != p2.get());
    std::cout << "Test 19 (DiffTopology) Passed\n";
}

// 20. Negative - Different variable root
void test20_DiffVariableRoot() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true};
    state1.bindings[p] = {100};
    state2.bindings[q] = {100}; // diff var root
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(nullptr);
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) != config.encodeState(state2));
    assert(p1.get() != p2.get());
    std::cout << "Test 20 (DiffVariableRoot) Passed\n";
}

// 21. Positive - Exact same state
void test21_PositiveIdentical() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    ir::Field next{1, "next"};
    
    state1.bindings[p] = {100, 200};
    state1.graph.addEdge(100, next, 300);
    
    state2.bindings[p] = {100, 200};
    state2.graph.addEdge(100, next, 300);
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(nullptr);
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) == config.encodeState(state2));
    assert(p1.get() == p2.get());
    std::cout << "Test 21 (PositiveIdentical) Passed\n";
}

// 22. Liveness Interaction - Dead bindings do not differentiate states
void test22_LivenessInteraction() {
    hra::HRAState state1, state2;
    ir::Variable p{1, "p", true};
    ir::Variable q{2, "q", true}; // dead variable
    
    state1.bindings[p] = {100};
    state1.bindings[q] = {200}; // different here!
    
    state2.bindings[p] = {100};
    state2.bindings[q] = {300}; // different here!
    
    liveness::LivenessResult lres;
    // Let's pretend block 1 has only 'p' live
    lres.liveOut[1] = {p};
    
    mdehra::mde_adapter::MDEGuidedHRAConfig config(&lres);
    
    solver::HRASolverStats stats;
    // We simulate what the solver does:
    // 1. Process statement
    // 2. Post-process state (pruning)
    // 3. Canonicalize state
    
    // We construct a mock statement just to trigger postProcessState
    ir::BasicBlock b(1);
    b.statements.push_back(std::make_unique<ir::AllocStmt>(p, 100)); // statement doesn't matter much
    
    // In our simplified liveness, the solver uses the statement pointer
    lres.liveAfter[b.statements.back().get()] = {p};
    
    config.postProcessState(b.statements.back().get(), state1, stats);
    config.postProcessState(b.statements.back().get(), state2, stats);
    
    // Now both states should only have 'p'
    auto p1 = config.canonicalizeState(state1);
    auto p2 = config.canonicalizeState(state2);
    
    assert(config.encodeState(state1) == config.encodeState(state2));
    assert(p1.get() == p2.get());
    std::cout << "Test 22 (LivenessInteraction) Passed\n";
}

int main() {
    std::cout << "Starting Configuration C Tests...\n";
    test1_Empty();
    test2_SingleAlloc();
    test3_DeadAlloc();
    test4_LiveAlloc();
    test5_PtrAssign();
    test6_Aliasing();
    test7_FieldStore();
    test8_FieldLoad();
    test9_LinkedList();
    test10_Branch();
    test11_Loop();
    test12_DeadTemp();
    test13_MultipleDeadTemps();
    test14_SharedTarget();
    test15_SelfCycle();
    test16_TwoNodeCycle();
    test17_DiffFieldLabel();
    test18_DiffAllocSites();
    test19_DiffTopology();
    test20_DiffVariableRoot();
    test21_PositiveIdentical();
    test22_LivenessInteraction();
    
    std::cout << "ALL CONFIGURATION C TESTS PASSED\n";
    return 0;
}
