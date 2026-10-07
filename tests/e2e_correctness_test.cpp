#include "mdehra/ir/Program.hpp"
#include "mdehra/ir/Printer.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <queue>
#include <algorithm>

using namespace mdehra;

// Re-using the canonicalizer from integration_test/config_c_test
std::string getCanonicalOutput(std::shared_ptr<const hra::HRAState> statePtr, const std::set<ir::Variable>& liveOut) {
    if (!statePtr) return "Empty";
    const auto& state = *statePtr;
    
    std::map<hra::NodeID, hra::NodeID> canonicalIds;
    hra::NodeID nextId = 1;

    std::map<std::string, std::set<hra::NodeID>> liveBindings;
    for (const auto& var : liveOut) {
        auto it = state.bindings.find(var);
        if (it != state.bindings.end() && !it->second.empty()) {
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

void printStateRaw(std::shared_ptr<const hra::HRAState> statePtr) {
    if (!statePtr) return;
    const auto& state = *statePtr;
    for (auto& kv : state.bindings) {
        std::cout << kv.first.name << " -> {";
        for (auto n : kv.second) std::cout << n << ",";
        std::cout << "}\n";
    }
    for (auto& e : state.graph.edges) {
        std::cout << e.source << " -" << e.field.name << "-> " << e.target << "\n";
    }
}

// ------------------------------------------------------------------
// E2E Test Runner
// ------------------------------------------------------------------
struct E2EResult {
    bool AB_Match;
    bool BC_Match;
    int iterations;
    int mdeHits;
    int mdeMisses;
    std::string canonicalA;
    std::string canonicalB;
    std::string canonicalC;
};

E2EResult runE2E(const std::string& code, const std::string& testName, bool expectDifferent = false) {
    std::string fullCode = "void* __builtin_malloc(unsigned long long);\nstruct Node { struct Node* next; struct Node* prev; int data; };\n" + code;
    std::unique_ptr<clang::ASTUnit> ast = clang::tooling::buildASTFromCode(fullCode);
    assert(ast != nullptr && "Failed to build AST from code");

    auto program = frontend::ClangFrontend::translate(ast->getASTContext());
    assert(program != nullptr && "Failed to translate to IR");

    // Only test the first function that's not empty
    const ir::Function* funcToTest = nullptr;
    for (const auto& kv : program->functions) {
        if (!kv.second.blocks.empty()) {
            funcToTest = &kv.second;
            break;
        }
    }
    assert(funcToTest && "No function found in program");

    // Run Liveness
    liveness::LivenessSolver ls;
    auto livenessRes = ls.run(*funcToTest);
    
    // Find exit block ID to extract live-out
    ir::ID exitBlockId = 0;
    for (const auto& kv : funcToTest->blocks) {
        if (kv.second.successors.empty()) {
            exitBlockId = kv.first;
            break;
        }
    }

    std::set<ir::Variable> liveAtExit = livenessRes.liveOut[exitBlockId];

    // Config A (Baseline)
    solver::BaselineHRAConfig configA;
    solver::HRASolver solverA;
    solverA.run(*funcToTest, &configA);

    // Config B (Liveness Guided)
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    solver::HRASolver solverB;
    solverB.run(*funcToTest, &configB);

    // Config C (MDE Guided)
    mde_adapter::MDEGuidedHRAConfig configC(&livenessRes);
    solver::HRASolver solverC;
    solverC.run(*funcToTest, &configC);

    auto stateA = solverA.getBlockExitState(exitBlockId);
    auto stateB = solverB.getBlockExitState(exitBlockId);
    auto stateC = solverC.getBlockExitState(exitBlockId);

    std::string canA = getCanonicalOutput(stateA, liveAtExit);
    std::string canB = getCanonicalOutput(stateB, liveAtExit);
    std::string canC = getCanonicalOutput(stateC, liveAtExit);

    E2EResult result;
    result.canonicalA = canA;
    result.canonicalB = canB;
    result.canonicalC = canC;
    result.AB_Match = (canA == canB);
    result.BC_Match = (canB == canC);
    result.mdeHits = configC.mdeHits;
    result.mdeMisses = configC.mdeMisses;
    
    std::cout << "Test: " << testName << " | A=B: " << (result.AB_Match ? "PASS" : "FAIL") 
              << " | B=C: " << (result.BC_Match ? "PASS" : "FAIL") 
              << " | MDE Hits: " << result.mdeHits << ", Misses: " << result.mdeMisses << "\n";
              
    if (!result.BC_Match) {
        std::cerr << "--- B != C MISMATCH ---\n";
        std::cerr << "Config B:\n" << canB << "\n";
        std::cerr << "Config C:\n" << canC << "\n";
        assert(false && "B != C");
    }
    if (!result.AB_Match && !expectDifferent) { // A and B should match on live outputs
        std::cerr << "--- A != B MISMATCH ---\n";
        std::cerr << "Config A:\n" << canA << "\n";
        std::cerr << "Config B:\n" << canB << "\n";
        std::cerr << "Live At Exit: ";
        for (auto v : liveAtExit) std::cerr << v.name << " ";
        std::cerr << "\n";
        assert(false && "A != B");
    }

    if (testName == "Negative X (p->next=q)") {
        std::cout << "DEBUG Negative X:\n";
        
        std::cout << "IR Program:\n";
        ir::Printer::print(std::cout, *program);
        
        std::cout << "LiveAtExit: ";
        for (auto v : liveAtExit) std::cout << v.name << " ";
        std::cout << "\nRaw State C:\n";
        printStateRaw(stateC);
    }

    return result;
}

void e2eTests() {
    // 1. Single allocation
    runE2E(R"(
        void test1() {
            struct Node* p;
            p = (struct Node*)__builtin_malloc(8);
        }
    )", "1. Single Allocation");

    // 2. Allocation followed by pointer use
    runE2E(R"(
        struct Node* test2() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = p;
            return q;
        }
    )", "2. Alloc and Use");

    // 3. Pointer assignment
    runE2E(R"(
        struct Node* test3(struct Node* x) {
            struct Node* q;
            q = x;
            return q;
        }
    )", "3. Pointer assignment");

    // 4. Multiple allocations
    runE2E(R"(
        struct Node* test4() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            return p;
        }
    )", "4. Multiple allocations");

    // 5. Linked-list construction
    runE2E(R"(
        struct Node* test5() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            return p;
        }
    )", "5. Linked-list construction");

    // 6. Linked-list traversal/load
    runE2E(R"(
        struct Node* test6() {
            struct Node* p;
            struct Node* q;
            struct Node* r;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            r = p->next;
            return r;
        }
    )", "6. Linked-list traversal/load");

    // 7. Branch
    runE2E(R"(
        struct Node* test7(int c) {
            struct Node* p;
            if (c) {
                p = (struct Node*)__builtin_malloc(8);
            } else {
                p = (struct Node*)__builtin_malloc(8);
            }
            return p;
        }
    )", "7. Branch");

    // 8. Loop
    runE2E(R"(
        struct Node* test8(struct Node* p) {
            while (p != 0) {
                p = p->next;
            }
            return p;
        }
    )", "8. Loop");

    // 9. Dead pointer temporary
    runE2E(R"(
        struct Node* test9() {
            struct Node* p;
            struct Node* t;
            p = (struct Node*)__builtin_malloc(8);
            t = (struct Node*)__builtin_malloc(8);
            return p;
        }
    )", "9. Dead pointer temporary");

    // 10. Multiple dead temporaries
    runE2E(R"(
        struct Node* test10() {
            struct Node* p;
            struct Node* t1;
            struct Node* t2;
            p = (struct Node*)__builtin_malloc(8);
            t1 = (struct Node*)__builtin_malloc(8);
            t2 = (struct Node*)__builtin_malloc(8);
            t1->next = t2;
            return p;
        }
    )", "10. Multiple dead temporaries");

    // 11. Aliasing
    runE2E(R"(
        struct Node* test11() {
            struct Node* p;
            struct Node* q;
            struct Node* r;
            p = (struct Node*)__builtin_malloc(8);
            q = p;
            r = q;
            return r;
        }
    )", "11. Aliasing");

    // 12. Shared heap target
    runE2E(R"(
        struct Node* test12() {
            struct Node* p;
            struct Node* q;
            struct Node* t;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            t = (struct Node*)__builtin_malloc(8);
            p->next = t;
            q->next = t;
            return p; // q goes dead, but graph might still contain t if p relies on it
        }
    )", "12. Shared heap target");

    // 13. Different field labels where supported
    runE2E(R"(
        struct Node* test13() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            p->prev = q;
            return p;
        }
    )", "13. Different field labels");

    // 14. Self-cycle
    runE2E(R"(
        struct Node* test14() {
            struct Node* p;
            p = (struct Node*)__builtin_malloc(8);
            p->next = p;
            return p;
        }
    )", "14. Self-cycle");

    // 15. Two-node cycle
    runE2E(R"(
        struct Node* test15() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            q->prev = p;
            return p;
        }
    )", "15. Two-node cycle");

    // 16. Branch followed by heap mutation
    runE2E(R"(
        struct Node* test16(int c) {
            struct Node* p;
            struct Node* q;
            if (c) {
                p = (struct Node*)__builtin_malloc(8);
            } else {
                p = (struct Node*)__builtin_malloc(8);
            }
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            return p;
        }
    )", "16. Branch + Heap mutation");

    // 17. Loop followed by pointer use
    runE2E(R"(
        struct Node* test17(struct Node* p) {
            struct Node* head;
            head = p;
            while (p != 0) {
                p = p->next;
            }
            return head;
        }
    )", "17. Loop + pointer use");

    // 18. Dead state followed by a live alias
    runE2E(R"(
        struct Node* test18() {
            struct Node* p;
            struct Node* q;
            struct Node* live_alias;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            live_alias = p;
            p = 0; // p dies here
            return live_alias;
        }
    )", "18. Dead state + live alias");

    // 19. Negative test X vs Y
    std::cout << "\nRunning Negative Tests\n";
    auto resX = runE2E(R"(
        struct Node* testNegX() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->next = q;
            return p;
        }
    )", "Negative X (p->next=q)", true);
    
    auto resY = runE2E(R"(
        struct Node* testNegY() {
            struct Node* p;
            struct Node* q;
            p = (struct Node*)__builtin_malloc(8);
            q = (struct Node*)__builtin_malloc(8);
            p->prev = q;
            return p;
        }
    )", "Negative Y (p->prev=q)", true);

    if (resX.canonicalC == resY.canonicalC) {
        std::cerr << "NEGATIVE TEST FAILED: X and Y have same canonical representation!\n";
        std::cerr << "X:\n" << resX.canonicalC << "\n";
        std::cerr << "Y:\n" << resY.canonicalC << "\n";
        assert(false && "Negative test failed");
    }
    std::cout << "Negative test X vs Y passed!\n";
}

int main() {
    std::cout << "Starting E2E Correctness Validations (Phase 9)...\n\n";
    e2eTests();
    std::cout << "\nALL E2E PIPELINE TESTS PASSED\n";
    return 0;
}
