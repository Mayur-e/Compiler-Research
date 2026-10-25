#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/ir/Printer.hpp"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>

using namespace mdehra::frontend;
using namespace mdehra::ir;

void testStrictModeRejections() {
    std::cout << "[Test] Strict Mode Rejection on Unsupported Constructs..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    FrontendOptions strictOpts;
    strictOpts.strictMode = true;
    strictOpts.trackSourceLocations = true;

    // 1. Pointer arithmetic
    {
        std::string code = R"(
            struct Node { struct Node* next; };
            void test_arithmetic(struct Node* p) {
                struct Node* q;
                q = p + 1;
            }
        )";
        auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
        assert(ast != nullptr);

        auto result = ClangFrontend::translate(ast->getASTContext(), strictOpts);
        assert((!result.isComplete || !result.success) && "Strict mode must reject pointer arithmetic");
        assert(!result.diagnostics.empty() && "Diagnostics should not be empty");
        bool foundArith = false;
        for (const auto& diag : result.diagnostics) {
            if (diag.constructType.find("PointerArithmetic") != std::string::npos ||
                diag.severity == DiagnosticSeverity::Unsupported ||
                diag.severity == DiagnosticSeverity::Error) {
                foundArith = true;
                break;
            }
        }
        assert(foundArith && "Expected PointerArithmetic unsupported diagnostic");
        std::cout << "  - Pointer arithmetic correctly rejected in strict mode" << std::endl;
    }

    // 2. Array subscript
    {
        std::string code = R"(
            struct Node { struct Node* next; };
            void test_array(struct Node** arr) {
                struct Node* q;
                q = arr[0];
            }
        )";
        auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
        assert(ast != nullptr);

        auto result = ClangFrontend::translate(ast->getASTContext(), strictOpts);
        assert((!result.isComplete || !result.success) && "Strict mode must reject array subscripting");
        bool foundSubscript = false;
        for (const auto& diag : result.diagnostics) {
            if (diag.constructType.find("ArraySubscript") != std::string::npos ||
                diag.severity == DiagnosticSeverity::Unsupported ||
                diag.severity == DiagnosticSeverity::Error) {
                foundSubscript = true;
                break;
            }
        }
        assert(foundSubscript && "Expected ArraySubscript unsupported diagnostic");
        std::cout << "  - Array subscripting correctly rejected in strict mode" << std::endl;
    }

    // 3. Unmodeled function call
    {
        std::string code = R"(
            struct Node { struct Node* next; };
            void external_call(struct Node* p);
            void test_call(struct Node* p) {
                external_call(p);
            }
        )";
        auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
        assert(ast != nullptr);

        auto result = ClangFrontend::translate(ast->getASTContext(), strictOpts);
        assert((!result.isComplete || !result.success) && "Strict mode must reject unmodeled external call");
        bool foundCall = false;
        for (const auto& diag : result.diagnostics) {
            if (diag.constructType.find("Call") != std::string::npos ||
                diag.severity == DiagnosticSeverity::Unsupported ||
                diag.severity == DiagnosticSeverity::Error) {
                foundCall = true;
                break;
            }
        }
        assert(foundCall && "Expected UnmodeledCallExpr error diagnostic");
        std::cout << "  - Unmodeled call correctly rejected in strict mode" << std::endl;
    }
}

void testPermissiveModeGracefulDegradation() {
    std::cout << "[Test] Permissive Mode Handling..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    FrontendOptions permissiveOpts;
    permissiveOpts.strictMode = false;
    permissiveOpts.trackSourceLocations = true;

    std::string code = R"(
        struct Node { struct Node* next; };
        void unmodeled_work(struct Node* p);
        void test_mixed(struct Node* p) {
            struct Node* q;
            q = (struct Node*)__builtin_malloc(sizeof(struct Node));
            unmodeled_work(p);
            q->next = p;
        }
    )";
    auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
    assert(ast != nullptr);

    auto result = ClangFrontend::translate(ast->getASTContext(), permissiveOpts);
    assert(!result.isComplete && "Permissive mode should report isComplete=false when unmodeled constructs exist");
    assert(result.program != nullptr && "Permissive mode must produce a valid IR program");
    assert(!result.diagnostics.empty() && "Diagnostics should record warning");
    
    // Check that modeled statements still got translated
    bool foundAlloc = false;
    bool foundStore = false;
    for (const auto& funcEntry : result.program->functions) {
        for (const auto& blockEntry : funcEntry.second.blocks) {
            for (const auto& stmt : blockEntry.second.statements) {
                if (stmt->getType() == StmtType::Alloc) foundAlloc = true;
                if (stmt->getType() == StmtType::Store) foundStore = true;
            }
        }
    }
    assert(foundAlloc && "Alloc statement must be preserved in permissive mode");
    assert(foundStore && "Store statement must be preserved in permissive mode");
    std::cout << "  - Permissive mode gracefully degrades and retains modeled statements" << std::endl;
}

void testExtendedConstructsAndSourceLocations() {
    std::cout << "[Test] Extended Constructs: DeclStmt, Calloc, Null, Nested Member Access..." << std::endl;

    std::vector<std::string> clangArgs = {
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };

    FrontendOptions opts;
    opts.strictMode = true;
    opts.trackSourceLocations = true;

    std::string code = R"(
        typedef unsigned long long size_t;
        void* calloc(size_t nmemb, size_t size);
        struct Node {
            struct Node* next;
            struct Node* prev;
        };

        void test_extended() {
            struct Node* a = (struct Node*)calloc(1, sizeof(struct Node));
            struct Node* b = (struct Node*)0;
            struct Node* c = a;
            a->next = c->prev;
        }
    )";

    auto ast = clang::tooling::buildASTFromCodeWithArgs(code, clangArgs);
    assert(ast != nullptr);

    auto result = ClangFrontend::translate(ast->getASTContext(), opts);
    assert(result.isComplete && "Valid extended constructs should translate completely in strict mode");
    assert(result.program != nullptr);

    // Verify source locations on statements
    int locCount = 0;
    bool hasAlloc = false;
    bool hasNullAssign = false;
    bool hasNestedStore = false;

    for (const auto& funcEntry : result.program->functions) {
        if (funcEntry.second.name != "test_extended") continue;
        for (const auto& blockEntry : funcEntry.second.blocks) {
            for (const auto& stmt : blockEntry.second.statements) {
                if (stmt->location.line > 0) locCount++;
                if (stmt->getType() == StmtType::Alloc) hasAlloc = true;
                if (stmt->getType() == StmtType::Assign) {
                    const auto* assign = static_cast<const AssignStmt*>(stmt.get());
                    if (assign && assign->isNull) hasNullAssign = true;
                }
                if (stmt->getType() == StmtType::Store) hasNestedStore = true;
            }
        }
    }

    assert(locCount > 0 && "Source locations should be tracked and > 0");
    assert(hasAlloc && "calloc should be translated to alloc statement");
    assert(hasNullAssign && "Assignment of 0 should produce isNull=true AssignStmt");
    assert(hasNestedStore && "Nested member assignment should produce valid store statement");
    std::cout << "  - Extended constructs and source locations verified successfully" << std::endl;
}

int main() {
    std::cout << "=== Running Frontend Diagnostics & Coverage Tests ===" << std::endl;
    testStrictModeRejections();
    testPermissiveModeGracefulDegradation();
    testExtendedConstructsAndSourceLocations();
    std::cout << "=== All Frontend Diagnostics Tests Passed! ===" << std::endl;
    return 0;
}
