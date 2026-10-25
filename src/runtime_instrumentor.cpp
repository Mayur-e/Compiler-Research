/**
 * runtime_instrumentor.cpp
 *
 * Clang AST-based source instrumentation tool for Phase 14.
 *
 * Takes a benchmark C source file and produces an instrumented version
 * that links against runtime_events.[h/c] to record concrete execution
 * events: malloc, pointer assignment, field load, field store, branch,
 * loop iteration, and return.
 *
 * The instrumented file can be compiled with GCC/Clang and executed
 * to produce a runtime_events.json for Phase 14 correlation.
 *
 * SUPPORTED CONSTRUCTS (matching benchmark subset BM01–BM08):
 *   - malloc / __builtin_malloc
 *   - pointer assignment (ptr = ...)
 *   - field load (dst = base->field)
 *   - field store (base->field = value)
 *   - while loop body (loop iteration counter)
 *   - if condition (branch taken/not-taken)
 *   - return (pointer return value)
 *
 * UNSUPPORTED (not in benchmark subset):
 *   - pointer arithmetic
 *   - unions
 *   - arbitrary casts
 *   - recursion
 *   - interprocedural patterns
 *
 * Output: <basename>_instrumented.c (ready to compile and run)
 *
 * Usage:
 *   runtime-instrumentor <source.c> [-o output.c]
 */

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Decl.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <iostream>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory InstrOpts("runtime-instrumentor options");
static llvm::cl::opt<std::string> OutputFile("o",
    llvm::cl::desc("Output instrumented C file"),
    llvm::cl::value_desc("output.c"),
    llvm::cl::cat(InstrOpts));

// ============================================================
// Instrumentation context shared across the AST visit
// ============================================================
struct InstrContext {
    int nextStmtId       = 1;
    int nextAllocSite    = 1;
    int nextLoopId       = 1;
    int paramCount       = 0;
    std::string funcName;
};

// ============================================================
// Helper: get source text for an expression
// ============================================================
static std::string getSourceText(const Expr* E,
                                  const SourceManager& SM,
                                  const LangOptions& LO) {
    CharSourceRange range = CharSourceRange::getTokenRange(E->getSourceRange());
    return std::string(Lexer::getSourceText(range, SM, LO));
}

// ============================================================
// Helper: strip outermost parentheses and casts
// ============================================================
static const Expr* stripCasts(const Expr* E) {
    while (true) {
        E = E->IgnoreParens();
        if (auto* ce = dyn_cast<CastExpr>(E)) { E = ce->getSubExpr(); continue; }
        break;
    }
    return E;
}

// ============================================================
// Check if an expression is a call to __builtin_malloc or malloc
// ============================================================
static bool isMallocCall(const Expr* E) {
    const Expr* stripped = stripCasts(E);
    if (const auto* call = dyn_cast<CallExpr>(stripped)) {
        if (const auto* ref = dyn_cast<DeclRefExpr>(call->getCallee()->IgnoreImpCasts())) {
            std::string name = ref->getNameInfo().getName().getAsString();
            return (name == "__builtin_malloc" || name == "malloc");
        }
    }
    return false;
}

// ============================================================
// Check if LHS is a member expression (field store/load)
// ============================================================
static bool isMemberAccess(const Expr* E) {
    return isa<MemberExpr>(stripCasts(E));
}

// ============================================================
// Generate a unique temp variable name
// ============================================================
static std::string tmpVar(int id, const std::string& suffix = "") {
    return "_rt_tmp_" + std::to_string(id) + suffix;
}

// ============================================================
// The main AST visitor that performs instrumentation
// ============================================================
class InstrumentVisitor : public RecursiveASTVisitor<InstrumentVisitor> {
public:
    Rewriter&        RW;
    ASTContext&      Ctx;
    InstrContext&    Instr;
    const SourceManager& SM;
    const LangOptions&   LO;
    // Track which compound stmts we've already processed
    std::set<Stmt*> processedBodies;

    InstrumentVisitor(Rewriter& rw, ASTContext& ctx, InstrContext& instr)
        : RW(rw), Ctx(ctx), Instr(instr),
          SM(ctx.getSourceManager()), LO(ctx.getLangOpts()) {}

    // ---- Function declarations ----------------------------------------
    bool VisitFunctionDecl(FunctionDecl* FD) {
        if (!FD->hasBody()) return true;
        if (!SM.isInMainFile(FD->getLocation())) return true;
        Instr.funcName   = FD->getNameAsString();
        Instr.paramCount = FD->getNumParams();

        // Inject rt_function_enter at top of body
        CompoundStmt* body = dyn_cast<CompoundStmt>(FD->getBody());
        if (!body) return true;
        int line = SM.getSpellingLineNumber(body->getLBracLoc());
        std::string inject = "\n    rt_function_enter(" + std::to_string(line)
                           + ", \"" + Instr.funcName + "\");";
        RW.InsertTextAfterToken(body->getLBracLoc(), inject);
        return true;
    }

    // ---- Binary operators (assignments) ----------------------------------
    bool VisitBinaryOperator(BinaryOperator* BO) {
        if (!SM.isInMainFile(BO->getOperatorLoc())) return true;
        if (!BO->isAssignmentOp()) return true;

        const Expr* LHS = BO->getLHS()->IgnoreParens();
        const Expr* RHS = BO->getRHS();
        int stmtId = Instr.nextStmtId++;
        int line   = SM.getSpellingLineNumber(BO->getOperatorLoc());

        // ---- Case A: field store:  base->field = value ----
        if (isMemberAccess(LHS)) {
            const MemberExpr* ME = cast<MemberExpr>(LHS->IgnoreParens());
            std::string base  = getSourceText(ME->getBase(), SM, LO);
            std::string field = ME->getMemberNameInfo().getName().getAsString();
            std::string val   = getSourceText(RHS, SM, LO);

            std::string tmpBase = tmpVar(stmtId, "_base");
            std::string tmpVal  = tmpVar(stmtId, "_val");
            std::string fullStmt =
                "{ struct Node* " + tmpBase + " = (struct Node*)(" + base + "); "
                + "__typeof__(" + val + ") " + tmpVal + " = (" + val + "); "
                + tmpBase + "->" + field + " = " + tmpVal + "; "
                + "rt_field_store(" + std::to_string(stmtId)
                + ", " + std::to_string(line)
                + ", \"" + Instr.funcName + "\""
                + ", (void*)(" + tmpBase + ")"
                + ", \"" + field + "\""
                + ", (void*)(uintptr_t)(" + tmpVal + ")); }";

            RW.ReplaceText(BO->getSourceRange(), fullStmt);
            return true;
        }

        // --- Get variable name for LHS (must be a DeclRefExpr) ---
        std::string varName;
        if (const auto* DRE = dyn_cast<DeclRefExpr>(LHS)) {
            varName = DRE->getNameInfo().getName().getAsString();
        } else {
            return true; // unsupported LHS form
        }

        // ---- Case B: malloc assignment: ptr = (type*)malloc(sz) ----
        if (isMallocCall(RHS)) {
            int site = Instr.nextAllocSite++;
            // Extract size argument
            const CallExpr* call = dyn_cast<CallExpr>(stripCasts(RHS));
            std::string sizeExpr = "0";
            if (call && call->getNumArgs() > 0)
                sizeExpr = getSourceText(call->getArg(0), SM, LO);

            std::string inject =
                "; rt_malloc(" + std::to_string(stmtId)
                + ", " + std::to_string(line)
                + ", " + std::to_string(site)
                + ", \"" + Instr.funcName + "\""
                + ", \"" + varName + "\""
                + ", (void*)(" + varName + ")"
                + ", (size_t)(" + sizeExpr + "));"
                + " rt_ptr_assign(" + std::to_string(stmtId)
                + ", " + std::to_string(line)
                + ", \"" + Instr.funcName + "\""
                + ", \"" + varName + "\""
                + ", (void*)(" + varName + "));";

            SourceLocation end = Lexer::getLocForEndOfToken(
                BO->getEndLoc(), 0, SM, LO);
            RW.InsertTextAfter(end, inject);
            return true;
        }

        // ---- Case C: field load: dst = base->field ----
        if (isMemberAccess(RHS)) {
            const MemberExpr* ME = cast<MemberExpr>(stripCasts(RHS));
            std::string base  = getSourceText(ME->getBase(), SM, LO);
            std::string field = ME->getMemberNameInfo().getName().getAsString();

            std::string tmpBase = tmpVar(stmtId, "_base");
            std::string fullStmt =
                "{ struct Node* " + tmpBase + " = (struct Node*)(" + base + "); "
                + varName + " = " + tmpBase + "->" + field + "; "
                + "rt_field_load(" + std::to_string(stmtId)
                + ", " + std::to_string(line)
                + ", \"" + Instr.funcName + "\""
                + ", \"" + varName + "\""
                + ", (void*)" + tmpBase
                + ", \"" + field + "\""
                + ", (void*)(uintptr_t)(" + varName + ")); }";

            RW.ReplaceText(BO->getSourceRange(), fullStmt);
            return true;
        }

        // ---- Case D: simple pointer assignment: dst = src ----
        std::string inject =
            "; rt_ptr_assign(" + std::to_string(stmtId)
            + ", " + std::to_string(line)
            + ", \"" + Instr.funcName + "\""
            + ", \"" + varName + "\""
            + ", (void*)(" + varName + "));";

        SourceLocation end = Lexer::getLocForEndOfToken(
            BO->getEndLoc(), 0, SM, LO);
        RW.InsertTextAfter(end, inject);
        return true;
    }

    // ---- If statements --------------------------------------------------
    bool VisitIfStmt(IfStmt* IS) {
        if (!SM.isInMainFile(IS->getIfLoc())) return true;
        const Expr* cond = IS->getCond();
        int stmtId = Instr.nextStmtId++;
        int line   = SM.getSpellingLineNumber(IS->getIfLoc());
        std::string condText = getSourceText(cond, SM, LO);
        std::string tmpCond = tmpVar(stmtId, "_cond");
        std::string wrappedCond =
            "({ __typeof__(" + condText + ") " + tmpCond + " = (" + condText + "); "
            + "rt_branch(" + std::to_string(stmtId) + ", " + std::to_string(line)
            + ", \"" + Instr.funcName + "\", (int)!!(" + tmpCond + ")); "
            + tmpCond + "; })";
        RW.ReplaceText(cond->getSourceRange(), wrappedCond);
        return true;
    }

    // ---- While loops -----------------------------------------------------
    bool VisitWhileStmt(WhileStmt* WS) {
        if (!SM.isInMainFile(WS->getWhileLoc())) return true;

        const Expr* cond = WS->getCond();
        int stmtId = Instr.nextStmtId++;
        int line   = SM.getSpellingLineNumber(WS->getWhileLoc());
        int loopId = Instr.nextLoopId++;

        // Inject branch record on each condition evaluation
        // Single-evaluation using GNU statement expression
        std::string condText = getSourceText(cond, SM, LO);
        std::string tmpCond = tmpVar(stmtId, "_cond");
        std::string wrappedCond =
            "({ __typeof__(" + condText + ") " + tmpCond + " = (" + condText + "); "
            + "rt_branch(" + std::to_string(stmtId) + ", " + std::to_string(line)
            + ", \"" + Instr.funcName + "\", (int)!!(" + tmpCond + ")); "
            + tmpCond + "; })";
        RW.ReplaceText(cond->getSourceRange(), wrappedCond);

        // Inject loop iteration counter at TOP of body
        Stmt* body = WS->getBody();
        if (!body) return true;
        if (processedBodies.count(body)) return true;
        processedBodies.insert(body);

        // Declare loop counter before the while
        std::string counterDecl =
            "{ int _rt_loop_iter_" + std::to_string(loopId) + " = 0; ";
        RW.InsertTextBefore(WS->getWhileLoc(), counterDecl);

        // Insert at start of body
        if (CompoundStmt* cs = dyn_cast<CompoundStmt>(body)) {
            std::string inject =
                "\n        rt_loop_iter(" + std::to_string(stmtId)
                + ", " + std::to_string(line)
                + ", \"" + Instr.funcName + "\""
                + ", ++_rt_loop_iter_" + std::to_string(loopId) + ");";
            RW.InsertTextAfterToken(cs->getLBracLoc(), inject);
        }

        // Close the wrapper block after the while
        SourceLocation end = Lexer::getLocForEndOfToken(
            WS->getEndLoc(), 0, SM, LO);
        RW.InsertTextAfter(end, " }");
        return true;
    }

    // ---- Return statements -----------------------------------------------
    bool VisitReturnStmt(ReturnStmt* RS) {
        if (!SM.isInMainFile(RS->getReturnLoc())) return true;
        if (!RS->getRetValue()) return true;

        int stmtId = Instr.nextStmtId++;
        int line   = SM.getSpellingLineNumber(RS->getReturnLoc());
        std::string retText = getSourceText(RS->getRetValue(), SM, LO);

        // Save return value in temp once, record, then return temp
        std::string tmpRet = tmpVar(stmtId, "_ret");
        std::string before =
            "{ __typeof__(" + retText + ") " + tmpRet + " = (" + retText + "); "
            "rt_return(" + std::to_string(stmtId)
            + ", " + std::to_string(line)
            + ", \"" + Instr.funcName + "\""
            + ", \"return\""
            + ", (void*)(uintptr_t)(" + tmpRet + ")); "
            "rt_function_exit(" + std::to_string(line)
            + ", \"" + Instr.funcName + "\"); return " + tmpRet + "; }";

        // Replace the entire return statement
        RW.ReplaceText(RS->getSourceRange(), before);
        return true;
    }
};

// ============================================================
// AST Consumer
// ============================================================
class InstrumentConsumer : public ASTConsumer {
    Rewriter     RW;
    InstrContext Instr;
    std::string  OutputPath;

public:
    InstrumentConsumer(ASTContext& ctx, std::string outPath)
        : OutputPath(std::move(outPath)) {
        RW.setSourceMgr(ctx.getSourceManager(), ctx.getLangOpts());
    }

    void HandleTranslationUnit(ASTContext& ctx) override {
        InstrumentVisitor visitor(RW, ctx, Instr);
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());

        // Get the instrumented source
        const SourceManager& SM = ctx.getSourceManager();
        FileID mainFID = SM.getMainFileID();
        const auto* buf = RW.getRewriteBufferFor(mainFID);

        std::string instrumented;
        if (buf) {
            instrumented = std::string(buf->begin(), buf->end());
        } else {
            // No changes — use original
            bool invalid = false;
            llvm::StringRef origBuf = SM.getBufferData(mainFID, &invalid);
            if (!invalid) {
                instrumented = origBuf.str();
            }
        }

        // Prepend the runtime_events.h include and preamble
        std::string header =
            "/* AUTO-GENERATED INSTRUMENTED FILE — DO NOT EDIT */\n"
            "/* Original: see source file */\n"
            "/* Phase 14: Runtime-Static Correlation */\n\n"
            "#include <stddef.h>\n"
            "#include <stdint.h>\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include \"mdehra/runtime/runtime_events.h\"\n\n"
            "struct Node { struct Node* next; struct Node* prev; int data; };\n"
            "#define __builtin_malloc(sz) malloc(sz)\n\n";

        // Append a main() that drives the benchmark and flushes events
        std::string footer =
            "\n\n/* --- Phase 14 harness main() --- */\n"
            "int main(int argc, char** argv) {\n"
            "    const char* out = (argc > 1) ? argv[1] : \"runtime_events.json\";\n"
            "    rt_init();\n"
            "    rt_program_start(\"benchmark\");\n"
            "    rt_program_end();\n"
            "    rt_flush(out);\n"
            "    return 0;\n"
            "}\n";

        if (!Instr.funcName.empty()) {
            std::string callArg = (Instr.paramCount > 0) ? "1" : "";
            footer =
                "\n\n/* --- Phase 14 harness main() --- */\n"
                "int main(int argc, char** argv) {\n"
                "    const char* out = (argc > 1) ? argv[1] : \"runtime_events.json\";\n"
                "    rt_init();\n"
                "    rt_program_start(\"" + Instr.funcName + "\");\n"
                "    void* ret = " + Instr.funcName + "(" + callArg + ");\n"
                "    rt_program_end();\n"
                "    rt_flush(out);\n"
                "    (void)ret;\n"
                "    return 0;\n"
                "}\n";
        }

        std::string fullOutput = header + instrumented + footer;

        if (!OutputPath.empty()) {
            std::ofstream ofs(OutputPath);
            if (ofs) {
                ofs << fullOutput;
                llvm::errs() << "[instrumentor] Wrote: " << OutputPath << "\n";
            } else {
                llvm::errs() << "[instrumentor] ERROR: Cannot write to " << OutputPath << "\n";
            }
        } else {
            llvm::outs() << fullOutput;
        }
    }
};

// ============================================================
// Frontend Action
// ============================================================
class InstrumentAction : public ASTFrontendAction {
    std::string OutputPath;
public:
    InstrumentAction(std::string outPath) : OutputPath(std::move(outPath)) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI,
                                                    StringRef) override {
        return std::make_unique<InstrumentConsumer>(CI.getASTContext(), OutputPath);
    }
};

class InstrumentActionFactory : public FrontendActionFactory {
    std::string OutputPath;
public:
    InstrumentActionFactory(std::string outPath) : OutputPath(std::move(outPath)) {}

    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<InstrumentAction>(OutputPath);
    }
};

// ============================================================
// main
// ============================================================
int main(int argc, const char** argv) {
    auto expectedParser =
        CommonOptionsParser::create(argc, argv, InstrOpts);
    if (!expectedParser) {
        llvm::errs() << expectedParser.takeError();
        return 1;
    }
    CommonOptionsParser& optParser = expectedParser.get();

    const auto& sources = optParser.getSourcePathList();
    if (sources.empty()) {
        llvm::errs() << "Usage: runtime-instrumentor <source.c> [-o output.c]\n";
        return 1;
    }

    std::string outPath = OutputFile.getValue();
    if (outPath.empty()) {
        // Default: <basename>_instrumented.c in current directory
        std::string src = sources[0];
        size_t dot = src.rfind('.');
        size_t sep = src.find_last_of("/\\");
        std::string base = (sep == std::string::npos) ? src : src.substr(sep + 1);
        if (dot != std::string::npos)
            base = base.substr(0, base.rfind('.'));
        outPath = base + "_instrumented.c";
    }

    ClangTool tool(optParser.getCompilations(), sources);

    tool.mapVirtualFile("preamble.h",
        "void* __builtin_malloc(unsigned long long);\n"
        "struct Node { struct Node* next; struct Node* prev; int data; };\n");

    tool.appendArgumentsAdjuster([](const CommandLineArguments& Args, StringRef) {
        CommandLineArguments NewArgs = Args;
        NewArgs.insert(NewArgs.begin() + 1, "-w");
        NewArgs.insert(NewArgs.begin() + 1, "-xc");
        NewArgs.insert(NewArgs.begin() + 1, "preamble.h");
        NewArgs.insert(NewArgs.begin() + 1, "-include");
        return NewArgs;
    });

    return tool.run(new InstrumentActionFactory(outPath));
}
