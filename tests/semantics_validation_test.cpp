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
#include <map>
#include <set>

using namespace mdehra;

// Canonicalizer for state comparison
std::string getCanonicalOutput(std::shared_ptr<const hra::HRAState> statePtr, const std::set<ir::Variable>& relevantVars) {
    if (!statePtr) return "Empty";
    const auto& state = *statePtr;

    std::map<hra::NodeID, hra::NodeID> canonicalIds;
    hra::NodeID nextId = 1;

    std::map<std::string, std::set<hra::NodeID>> activeBindings;
    for (const auto& var : relevantVars) {
        auto it = state.bindings.find(var);
        if (it != state.bindings.end() && !it->second.empty()) {
            activeBindings[var.name] = it->second;
        }
    }

    std::queue<hra::NodeID> q;
    for (const auto& kv : activeBindings) {
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
    for (const auto& kv : activeBindings) {
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

void extractStmtVars(const ir::Statement* stmt, std::set<ir::Variable>& vars) {
    if (!stmt) return;
    switch (stmt->getType()) {
        case ir::StmtType::Alloc: {
            const auto* a = static_cast<const ir::AllocStmt*>(stmt);
            vars.insert(a->dest);
            break;
        }
        case ir::StmtType::Assign: {
            const auto* a = static_cast<const ir::AssignStmt*>(stmt);
            vars.insert(a->dest);
            if (!a->isNull) vars.insert(a->src);
            break;
        }
        case ir::StmtType::Load: {
            const auto* l = static_cast<const ir::LoadStmt*>(stmt);
            vars.insert(l->dest);
            vars.insert(l->base);
            break;
        }
        case ir::StmtType::Store: {
            const auto* s = static_cast<const ir::StoreStmt*>(stmt);
            vars.insert(s->base);
            if (!s->isNull) vars.insert(s->value);
            break;
        }
        case ir::StmtType::Return: {
            const auto* r = static_cast<const ir::ReturnStmt*>(stmt);
            if (r->hasValue) vars.insert(r->value);
            break;
        }
        case ir::StmtType::Call: {
            const auto* c = static_cast<const ir::CallStmt*>(stmt);
            for (const auto& arg : c->arguments) vars.insert(arg);
            break;
        }
        default:
            break;
    }
}

const ir::Function* getFuncByName(const ir::Program& program, const std::string& name) {
    for (const auto& kv : program.functions) {
        if (kv.second.name == name) return &kv.second;
    }
    return nullptr;
}

void testProgramSemantics(const std::string& name, const std::string& code) {
    std::cout << "[Semantic Test] " << name << "..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    std::string fullCode = R"(
        typedef unsigned long long size_t;
        void* malloc(size_t);
        void* calloc(size_t, size_t);
        void* __builtin_malloc(size_t);
        struct Node {
            struct Node* next;
            struct Node* prev;
            int val;
        };
    )" + code;

    auto ast = clang::tooling::buildASTFromCodeWithArgs(fullCode, clangArgs);
    assert(ast != nullptr && "AST compilation failed");

    frontend::FrontendOptions opts;
    opts.strictMode = true;
    opts.trackSourceLocations = true;

    auto res = frontend::ClangFrontend::translate(ast->getASTContext(), opts);
    assert(res.isComplete && "Translation must be complete in strict mode");
    assert(res.program != nullptr);

    const ir::Function* func = nullptr;
    for (const auto& kv : res.program->functions) {
        if (!kv.second.blocks.empty()) {
            func = &kv.second;
            break;
        }
    }
    assert(func != nullptr);

    // Run Liveness
    liveness::LivenessSolver livenessSolver;
    auto livenessRes = livenessSolver.run(*func);

    // Compute live variables at function exit (union of liveOut across all exit blocks)
    std::set<ir::Variable> liveAtExit;
    std::set<ir::Variable> allFunctionVars;
    for (const auto& bkv : func->blocks) {
        for (const auto& stmt : bkv.second.statements) {
            extractStmtVars(stmt.get(), allFunctionVars);
        }
        if (bkv.second.successors.empty()) {
            const auto& lo = livenessRes.liveOut[bkv.first];
            liveAtExit.insert(lo.begin(), lo.end());
        }
    }

    // Run Config A: Baseline HRA (No liveness, no MDE)
    solver::BaselineHRAConfig configA;
    solver::HRASolver solverA;
    solverA.run(*func, &configA);
    auto stateA = solverA.getFunctionExitState(*func);

    // Run Config B: Liveness + HRA
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    solver::HRASolver solverB;
    solverB.run(*func, &configB);
    auto stateB = solverB.getFunctionExitState(*func);

    // Run Config C: Liveness + HRA + MDE
    mde_adapter::MDEGuidedHRAConfig configC(&livenessRes);
    solver::HRASolver solverC;
    solverC.run(*func, &configC);
    auto stateC = solverC.getFunctionExitState(*func);

    // Run Config D: Baseline HRA + MDE (no liveness)
    mde_adapter::MDEGuidedHRAConfig configD(nullptr);
    solver::HRASolver solverD;
    solverD.run(*func, &configD);
    auto stateD = solverD.getFunctionExitState(*func);

    // Primary Invariant 1: CanonicalResult(B) == CanonicalResult(C) (evaluated over live-at-exit variables)
    std::string canB = getCanonicalOutput(stateB, liveAtExit);
    std::string canC = getCanonicalOutput(stateC, liveAtExit);
    assert(canB == canC && "INVARIANT VIOLATION: CanonicalResult(B) != CanonicalResult(C)");

    // Primary Invariant 2: CanonicalResult(A) == CanonicalResult(D) (evaluated over all variables)
    std::string canA = getCanonicalOutput(stateA, allFunctionVars);
    std::string canD = getCanonicalOutput(stateD, allFunctionVars);
    assert(canA == canD && "INVARIANT VIOLATION: CanonicalResult(A) != CanonicalResult(D)");

    std::cout << "  [PASS] Canonical(B) == Canonical(C) and Canonical(A) == Canonical(D)" << std::endl;
}

void testDeadTemporaryPruning() {
    std::cout << "[Semantic Test] Dead Temporary Pruning..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    std::string code = R"(
        void* malloc(unsigned long long);
        struct Node { struct Node* next; };
        struct Node* test_pruning() {
            struct Node* ret = (struct Node*)malloc(sizeof(struct Node));
            struct Node* dead1 = (struct Node*)malloc(sizeof(struct Node));
            struct Node* dead2 = (struct Node*)malloc(sizeof(struct Node));
            dead1->next = dead2;
            return ret;
        }
    )";

    auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
    assert(ast != nullptr);

    frontend::FrontendOptions opts;
    opts.strictMode = true;
    auto res = frontend::ClangFrontend::translate(ast->getASTContext(), opts);
    assert(res.isComplete && res.program != nullptr);

    const auto* funcPtr = getFuncByName(*res.program, "test_pruning");
    assert(funcPtr != nullptr);
    const auto& func = *funcPtr;
    liveness::LivenessSolver ls;
    auto livenessRes = ls.run(func);

    // Find variables by name
    ir::Variable vDead1, vDead2, vRet;
    for (const auto& b : func.blocks) {
        for (const auto& stmt : b.second.statements) {
            std::set<ir::Variable> stmtVars;
            extractStmtVars(stmt.get(), stmtVars);
            for (const auto& v : stmtVars) {
                if (v.name == "dead1") vDead1 = v;
                if (v.name == "dead2") vDead2 = v;
                if (v.name == "ret") vRet = v;
            }
        }
    }

    // Config A (Baseline retains dead bindings)
    solver::BaselineHRAConfig configA;
    solver::HRASolver solverA;
    solverA.run(func, &configA);
    auto stateA = solverA.getFunctionExitState(func);

    // Config B (Liveness prunes dead bindings)
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    solver::HRASolver solverB;
    solverB.run(func, &configB);
    auto stateB = solverB.getFunctionExitState(func);

    // Verify dead1 and dead2 exist in stateA bindings, but are NOT in stateB
    assert(stateA->bindings.count(vDead1) > 0);
    assert(stateA->bindings.count(vDead2) > 0);
    assert(stateB->bindings.count(vDead1) == 0 || stateB->bindings.at(vDead1).empty());
    assert(stateB->bindings.count(vDead2) == 0 || stateB->bindings.at(vDead2).empty());
    assert(stateB->bindings.count(vRet) > 0);

    std::cout << "  [PASS] Dead temporaries correctly pruned in Liveness-guided state" << std::endl;
}

void testNullStoreEdgeRemoval() {
    std::cout << "[Semantic Test] Null Store Edge Removal..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    std::string code = R"(
        void* malloc(unsigned long long);
        struct Node { struct Node* next; };
        struct Node* test_null_store() {
            struct Node* p = (struct Node*)malloc(sizeof(struct Node));
            struct Node* q = (struct Node*)malloc(sizeof(struct Node));
            p->next = q;
            p->next = (struct Node*)0;
            return p;
        }
    )";

    auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
    assert(ast != nullptr);

    frontend::FrontendOptions opts;
    opts.strictMode = true;
    auto res = frontend::ClangFrontend::translate(ast->getASTContext(), opts);
    assert(res.isComplete && res.program != nullptr);

    const auto* funcPtr = getFuncByName(*res.program, "test_null_store");
    assert(funcPtr != nullptr);
    const auto& func = *funcPtr;

    ir::Variable vP;
    for (const auto& b : func.blocks) {
        for (const auto& stmt : b.second.statements) {
            std::set<ir::Variable> stmtVars;
            extractStmtVars(stmt.get(), stmtVars);
            for (const auto& v : stmtVars) {
                if (v.name == "p") vP = v;
            }
        }
    }

    solver::BaselineHRAConfig configA;
    solver::HRASolver solverA;
    solverA.run(func, &configA);
    auto stateA = solverA.getFunctionExitState(func);

    // Verify p has no outgoing 'next' edges in final exit state
    std::set<hra::NodeID> pNodes;
    auto it = stateA->bindings.find(vP);
    if (it != stateA->bindings.end()) pNodes = it->second;
    assert(!pNodes.empty());

    for (const auto& edge : stateA->graph.edges) {
        for (auto n : pNodes) {
            assert(!(edge.source == n && edge.field.name == "next") && "p->next edge should have been removed by null store");
        }
    }

    std::cout << "  [PASS] Outgoing edge successfully cleared upon NULL store" << std::endl;
}

int main() {
    std::cout << "=== Running Semantics & Invariants Validation Suite ===" << std::endl;

    testProgramSemantics("Aliasing and Loads", R"(
        struct Node* test_alias() {
            struct Node* a = (struct Node*)malloc(sizeof(struct Node));
            struct Node* b = (struct Node*)malloc(sizeof(struct Node));
            a->next = b;
            struct Node* c = a->next;
            return c;
        }
    )");

    testProgramSemantics("Multi-Exit Branching", R"(
        struct Node* test_multiexit(int cond) {
            struct Node* p = (struct Node*)malloc(sizeof(struct Node));
            if (cond) {
                p->next = (struct Node*)malloc(sizeof(struct Node));
                return p;
            } else {
                p->prev = (struct Node*)malloc(sizeof(struct Node));
                return p;
            }
        }
    )");

    testProgramSemantics("Loop Construction", R"(
        struct Node* test_loop(int n) {
            struct Node* head = (struct Node*)malloc(sizeof(struct Node));
            struct Node* curr = head;
            while (n > 0) {
                struct Node* temp = (struct Node*)malloc(sizeof(struct Node));
                curr->next = temp;
                curr = temp;
                n = n - 1;
            }
            return head;
        }
    )");

    testDeadTemporaryPruning();
    testNullStoreEdgeRemoval();

    std::cout << "=== All Semantics & Invariants Tests Passed! ===" << std::endl;
    return 0;
}
