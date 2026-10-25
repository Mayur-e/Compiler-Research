#include "mdehra/frontend/ClangFrontend.hpp"
#include "clang/Analysis/CFG.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/SourceManager.h"
#include <iostream>
#include <unordered_map>

namespace mdehra {
namespace frontend {

using namespace clang;

class CFGTranslator {
    ir::Program& program;
    ir::Function& currentFunction;
    ASTContext& context;
    const FrontendOptions& options;
    TranslationResult& result;

    ir::ID nextVarId = 1;
    ir::ID nextAllocSite = 1;

    std::unordered_map<const VarDecl*, ir::Variable> varMap;
    std::unordered_map<const FieldDecl*, ir::Field> fieldMap;

public:
    CFGTranslator(ir::Program& prog, ir::Function& func, ASTContext& ctx,
                  const FrontendOptions& opts, TranslationResult& res)
        : program(prog), currentFunction(func), context(ctx), options(opts), result(res) {}

    ir::SourceLocation getLoc(const Stmt* S) {
        ir::SourceLocation loc;
        if (!S) return loc;
        clang::SourceLocation clangLoc = S->getBeginLoc();
        if (clangLoc.isValid()) {
            clang::SourceManager& SM = context.getSourceManager();
            clang::PresumedLoc PLoc = SM.getPresumedLoc(clangLoc);
            if (PLoc.isValid()) {
                loc.filename = PLoc.getFilename();
                loc.line = PLoc.getLine();
                loc.col = PLoc.getColumn();
            }
        }
        return loc;
    }

    void emitDiag(DiagnosticSeverity sev, const Stmt* S, const std::string& constructType, const std::string& msg) {
        ir::SourceLocation loc = getLoc(S);
        Diagnostic d;
        d.severity = sev;
        d.filename = loc.filename;
        d.line = loc.line;
        d.col = loc.col;
        d.constructType = constructType;
        d.message = msg;
        result.diagnostics.push_back(d);

        if (sev == DiagnosticSeverity::Unsupported || sev == DiagnosticSeverity::Error) {
            result.isComplete = false;
            if (options.strictMode) {
                result.success = false;
                result.status = "UNSUPPORTED";
            }
        }
    }

    ir::Variable getOrCreateVariable(const VarDecl* VD) {
        auto it = varMap.find(VD);
        if (it != varMap.end()) {
            return it->second;
        }

        ir::Variable v;
        v.id = nextVarId++;
        v.name = VD->getNameAsString();
        v.isPointer = VD->getType()->isPointerType();
        varMap[VD] = v;
        currentFunction.localVariables[v.id] = v;
        return v;
    }

    ir::Variable createTempPointerVariable(const std::string& prefix = "__tmp") {
        ir::Variable v;
        v.id = nextVarId++;
        v.name = prefix + "_" + std::to_string(v.id);
        v.isPointer = true;
        currentFunction.localVariables[v.id] = v;
        return v;
    }

    ir::Field getOrCreateField(const FieldDecl* FD) {
        auto it = fieldMap.find(FD);
        if (it != fieldMap.end()) {
            return it->second;
        }

        ir::Field f;
        f.id = program.globalFields.size() + 1;
        f.name = FD->getNameAsString();
        fieldMap[FD] = f;
        program.globalFields[f.id] = f;
        return f;
    }

    bool isNullExpr(const Expr* E) {
        if (!E) return false;
        E = E->IgnoreParenCasts();
        if (E->isNullPointerConstant(context, Expr::NPC_ValueDependentIsNotNull) != Expr::NPCK_NotNull) {
            return true;
        }
        if (auto* IL = dyn_cast<IntegerLiteral>(E)) {
            return IL->getValue() == 0;
        }
        return false;
    }

    bool isAllocCall(const Expr* E, std::string& outName) {
        if (!E) return false;
        E = E->IgnoreParenCasts();
        if (auto* CE = dyn_cast<CallExpr>(E)) {
            if (auto* callee = CE->getDirectCallee()) {
                std::string name = callee->getNameAsString();
                if (name == "malloc" || name == "__builtin_malloc" ||
                    name == "calloc" || name == "__builtin_calloc") {
                    outName = name;
                    return true;
                }
            }
        }
        return false;
    }

    // Resolves an expression to an ir::Variable for pointer reading.
    // Emits LoadStmt into irBlock for nested field accesses (e.g. p->next).
    bool resolvePointerRHS(const Expr* E, ir::BasicBlock& irBlock,
                           ir::Variable& outVar, bool& outIsNull, ir::ID& outAllocSite) {
        outIsNull = false;
        outAllocSite = 0;
        if (!E) return false;
        E = E->IgnoreParenCasts();

        if (isNullExpr(E)) {
            outIsNull = true;
            return true;
        }

        std::string allocName;
        if (isAllocCall(E, allocName)) {
            outAllocSite = nextAllocSite++;
            return true;
        }

        if (auto* DRE = dyn_cast<DeclRefExpr>(E)) {
            if (auto* VD = dyn_cast<VarDecl>(DRE->getDecl())) {
                outVar = getOrCreateVariable(VD);
                return true;
            }
        }

        if (auto* ME = dyn_cast<MemberExpr>(E)) {
            if (auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
                ir::Variable baseVar;
                bool baseIsNull = false;
                ir::ID baseAlloc = 0;
                if (resolvePointerRHS(ME->getBase(), irBlock, baseVar, baseIsNull, baseAlloc)) {
                    if (baseIsNull) {
                        emitDiag(DiagnosticSeverity::Warning, ME, "NullDereference", "Field load on null base pointer");
                    }
                    ir::Field field = getOrCreateField(FD);
                    ir::Variable tmp = createTempPointerVariable("__load_tmp");
                    auto loadStmt = std::make_unique<ir::LoadStmt>(tmp, baseVar, field);
                    if (options.trackSourceLocations) loadStmt->location = getLoc(ME);
                    irBlock.statements.push_back(std::move(loadStmt));
                    outVar = tmp;
                    return true;
                }
            }
        }

        // Check for unsupported pointer expressions
        if (E->getType()->isPointerType()) {
            if (isa<BinaryOperator>(E)) {
                emitDiag(DiagnosticSeverity::Unsupported, E, "PointerArithmetic",
                         "Pointer arithmetic is unsupported in abstract heap graph");
            } else if (isa<ArraySubscriptExpr>(E)) {
                emitDiag(DiagnosticSeverity::Unsupported, E, "ArraySubscript",
                         "Pointer array indexing is unsupported in abstract heap graph");
            } else if (isa<UnaryOperator>(E)) {
                auto* UO = cast<UnaryOperator>(E);
                if (UO->getOpcode() == UO_AddrOf) {
                    emitDiag(DiagnosticSeverity::Unsupported, E, "AddressOfOperator",
                             "Address-of local variable (&x) is unsupported");
                } else if (UO->getOpcode() == UO_Deref) {
                    emitDiag(DiagnosticSeverity::Unsupported, E, "MultiLevelDeref",
                             "Multi-level pointer dereference is unsupported");
                } else {
                    emitDiag(DiagnosticSeverity::Unsupported, E, "UnaryPointerOp",
                             "Unary pointer operator is unsupported");
                }
            } else {
                emitDiag(DiagnosticSeverity::Unsupported, E, "UnsupportedPointerConstruct",
                         std::string("Unsupported pointer expression construct: ") + E->getStmtClassName());
            }
            return false;
        }

        return false;
    }

    void translateDeclStmt(const DeclStmt* DS, ir::BasicBlock& irBlock) {
        for (auto* D : DS->decls()) {
            if (auto* VD = dyn_cast<VarDecl>(D)) {
                ir::Variable dest = getOrCreateVariable(VD);
                if (VD->hasInit()) {
                    const Expr* Init = VD->getInit();
                    if (VD->getType()->isPointerType()) {
                        bool isNull = false;
                        ir::ID allocSite = 0;
                        ir::Variable rhsVar;
                        if (resolvePointerRHS(Init, irBlock, rhsVar, isNull, allocSite)) {
                            if (isNull) {
                                auto s = ir::AssignStmt::createNull(dest);
                                if (options.trackSourceLocations) s->location = getLoc(DS);
                                irBlock.statements.push_back(std::move(s));
                            } else if (allocSite > 0) {
                                auto s = std::make_unique<ir::AllocStmt>(dest, allocSite);
                                if (options.trackSourceLocations) s->location = getLoc(DS);
                                irBlock.statements.push_back(std::move(s));
                            } else {
                                auto s = std::make_unique<ir::AssignStmt>(dest, rhsVar);
                                if (options.trackSourceLocations) s->location = getLoc(DS);
                                irBlock.statements.push_back(std::move(s));
                            }
                        }
                    } else {
                        emitDiag(DiagnosticSeverity::Info, DS, "IgnoredNonPointerConstruct",
                                 std::string("Scalar local initialization: ") + VD->getNameAsString());
                    }
                }
            }
        }
    }

    void translateAssign(const Expr* LHS, const Expr* RHS, const Stmt* S, ir::BasicBlock& irBlock) {
        LHS = LHS->IgnoreParenCasts();
        RHS = RHS->IgnoreParenCasts();

        // Case 1: LHS is a direct pointer variable
        if (auto* DRE_L = dyn_cast<DeclRefExpr>(LHS)) {
            if (auto* VD_L = dyn_cast<VarDecl>(DRE_L->getDecl())) {
                ir::Variable dest = getOrCreateVariable(VD_L);
                if (VD_L->getType()->isPointerType()) {
                    bool isNull = false;
                    ir::ID allocSite = 0;
                    ir::Variable rhsVar;
                    if (resolvePointerRHS(RHS, irBlock, rhsVar, isNull, allocSite)) {
                        if (isNull) {
                            auto s = ir::AssignStmt::createNull(dest);
                            if (options.trackSourceLocations) s->location = getLoc(S);
                            irBlock.statements.push_back(std::move(s));
                        } else if (allocSite > 0) {
                            auto s = std::make_unique<ir::AllocStmt>(dest, allocSite);
                            if (options.trackSourceLocations) s->location = getLoc(S);
                            irBlock.statements.push_back(std::move(s));
                        } else {
                            auto s = std::make_unique<ir::AssignStmt>(dest, rhsVar);
                            if (options.trackSourceLocations) s->location = getLoc(S);
                            irBlock.statements.push_back(std::move(s));
                        }
                    }
                } else {
                    emitDiag(DiagnosticSeverity::Info, S, "IgnoredNonPointerConstruct",
                             std::string("Scalar assignment to ") + VD_L->getNameAsString());
                }
                return;
            }
        }

        // Case 2: LHS is a field store (base->field = RHS)
        if (auto* ME_L = dyn_cast<MemberExpr>(LHS)) {
            if (auto* FD_L = dyn_cast<FieldDecl>(ME_L->getMemberDecl())) {
                ir::Variable baseVar;
                bool baseIsNull = false;
                ir::ID baseAlloc = 0;
                if (resolvePointerRHS(ME_L->getBase(), irBlock, baseVar, baseIsNull, baseAlloc)) {
                    if (baseIsNull) {
                        emitDiag(DiagnosticSeverity::Warning, ME_L, "NullDereference", "Field store on null base pointer");
                    }
                    ir::Field field = getOrCreateField(FD_L);

                    if (FD_L->getType()->isPointerType() || RHS->getType()->isPointerType()) {
                        bool rhsIsNull = false;
                        ir::ID rhsAlloc = 0;
                        ir::Variable rhsVar;
                        if (resolvePointerRHS(RHS, irBlock, rhsVar, rhsIsNull, rhsAlloc)) {
                            if (rhsIsNull) {
                                auto s = ir::StoreStmt::createNull(baseVar, field);
                                if (options.trackSourceLocations) s->location = getLoc(S);
                                irBlock.statements.push_back(std::move(s));
                            } else if (rhsAlloc > 0) {
                                ir::Variable tmp = createTempPointerVariable("__alloc_store_tmp");
                                auto allocS = std::make_unique<ir::AllocStmt>(tmp, rhsAlloc);
                                if (options.trackSourceLocations) allocS->location = getLoc(S);
                                irBlock.statements.push_back(std::move(allocS));

                                auto s = std::make_unique<ir::StoreStmt>(baseVar, field, tmp);
                                if (options.trackSourceLocations) s->location = getLoc(S);
                                irBlock.statements.push_back(std::move(s));
                            } else {
                                auto s = std::make_unique<ir::StoreStmt>(baseVar, field, rhsVar);
                                if (options.trackSourceLocations) s->location = getLoc(S);
                                irBlock.statements.push_back(std::move(s));
                            }
                        }
                    } else {
                        emitDiag(DiagnosticSeverity::Info, S, "IgnoredNonPointerConstruct",
                                 std::string("Scalar field store to ") + FD_L->getNameAsString());
                    }
                    return;
                }
            }
        }

        // Case 3: LHS is pointer dereference (*p = ...)
        if (auto* UO_L = dyn_cast<UnaryOperator>(LHS)) {
            if (UO_L->getOpcode() == UO_Deref) {
                if (LHS->getType()->isPointerType()) {
                    emitDiag(DiagnosticSeverity::Unsupported, S, "MultiLevelPointerStore",
                             "Store through multi-level pointer (*p = q) is unsupported");
                } else {
                    emitDiag(DiagnosticSeverity::Info, S, "IgnoredNonPointerConstruct",
                             "Scalar store through pointer dereference (*p = val)");
                }
                return;
            }
        }

        emitDiag(DiagnosticSeverity::Warning, S, "UnhandledAssignment",
                 std::string("Unhandled assignment construct with LHS: ") + LHS->getStmtClassName());
    }

    void translateStmt(const Stmt* S, ir::BasicBlock& irBlock) {
        if (!S) return;

        if (auto* DS = dyn_cast<DeclStmt>(S)) {
            translateDeclStmt(DS, irBlock);
            return;
        }

        if (auto* BO = dyn_cast<BinaryOperator>(S)) {
            if (BO->getOpcode() == BO_Assign) {
                translateAssign(BO->getLHS(), BO->getRHS(), BO, irBlock);
                return;
            } else if (BO->isCompoundAssignmentOp()) {
                if (BO->getType()->isPointerType() || BO->getLHS()->getType()->isPointerType()) {
                    emitDiag(DiagnosticSeverity::Unsupported, BO, "PointerCompoundAssign",
                             "Compound pointer assignment is unsupported");
                } else {
                    emitDiag(DiagnosticSeverity::Info, BO, "IgnoredNonPointerConstruct",
                             "Scalar compound assignment");
                }
                return;
            } else {
                emitDiag(DiagnosticSeverity::Info, BO, "IgnoredNonPointerConstruct",
                         std::string("Scalar binary operator: ") + BO->getOpcodeStr().str());
                return;
            }
        }

        if (auto* RS = dyn_cast<ReturnStmt>(S)) {
            const Expr* retVal = RS->getRetValue();
            if (!retVal) {
                auto s = std::make_unique<ir::ReturnStmt>();
                if (options.trackSourceLocations) s->location = getLoc(RS);
                irBlock.statements.push_back(std::move(s));
                return;
            }

            retVal = retVal->IgnoreParenCasts();
            if (retVal->getType()->isPointerType()) {
                bool isNull = false;
                ir::ID allocSite = 0;
                ir::Variable rhsVar;
                if (resolvePointerRHS(retVal, irBlock, rhsVar, isNull, allocSite)) {
                    if (isNull) {
                        auto s = std::make_unique<ir::ReturnStmt>();
                        if (options.trackSourceLocations) s->location = getLoc(RS);
                        irBlock.statements.push_back(std::move(s));
                    } else if (allocSite > 0) {
                        ir::Variable tmp = createTempPointerVariable("__ret_alloc_tmp");
                        auto allocS = std::make_unique<ir::AllocStmt>(tmp, allocSite);
                        if (options.trackSourceLocations) allocS->location = getLoc(RS);
                        irBlock.statements.push_back(std::move(allocS));

                        auto s = std::make_unique<ir::ReturnStmt>(tmp);
                        if (options.trackSourceLocations) s->location = getLoc(RS);
                        irBlock.statements.push_back(std::move(s));
                    } else {
                        auto s = std::make_unique<ir::ReturnStmt>(rhsVar);
                        if (options.trackSourceLocations) s->location = getLoc(RS);
                        irBlock.statements.push_back(std::move(s));
                    }
                }
            } else {
                // Non-pointer return (e.g. return 0; or return same ? 0 : 1;)
                auto s = std::make_unique<ir::ReturnStmt>();
                if (options.trackSourceLocations) s->location = getLoc(RS);
                irBlock.statements.push_back(std::move(s));
                emitDiag(DiagnosticSeverity::Info, RS, "IgnoredNonPointerConstruct", "Non-pointer return value");
            }
            return;
        }

        if (auto* CE = dyn_cast<CallExpr>(S)) {
            if (auto* callee = CE->getDirectCallee()) {
                std::string funcName = callee->getNameAsString();
                if (funcName == "free") {
                    std::vector<ir::Variable> args;
                    for (unsigned i = 0; i < CE->getNumArgs(); ++i) {
                        const Expr* arg = CE->getArg(i)->IgnoreParenCasts();
                        if (auto* DRE = dyn_cast<DeclRefExpr>(arg)) {
                            if (auto* VD = dyn_cast<VarDecl>(DRE->getDecl())) {
                                args.push_back(getOrCreateVariable(VD));
                            }
                        }
                    }
                    auto callS = std::make_unique<ir::CallStmt>("free", std::move(args));
                    if (options.trackSourceLocations) callS->location = getLoc(CE);
                    irBlock.statements.push_back(std::move(callS));
                    return;
                } else if (funcName == "malloc" || funcName == "__builtin_malloc" ||
                           funcName == "calloc" || funcName == "__builtin_calloc") {
                    // Alloc call without assignment: emit Info
                    emitDiag(DiagnosticSeverity::Info, CE, "UnassignedAlloc", "Unassigned memory allocation call");
                    return;
                } else {
                    emitDiag(DiagnosticSeverity::Unsupported, CE, "UnmodeledFunctionCall",
                             std::string("Unmodeled function call: ") + funcName);
                    return;
                }
            } else {
                emitDiag(DiagnosticSeverity::Unsupported, CE, "IndirectCall", "Indirect function pointer call is unsupported");
                return;
            }
        }

        if (auto* UO = dyn_cast<UnaryOperator>(S)) {
            if (UO->isIncrementDecrementOp()) {
                if (UO->getType()->isPointerType()) {
                    emitDiag(DiagnosticSeverity::Unsupported, UO, "PointerIncrement", "Pointer increment/decrement is unsupported");
                } else {
                    emitDiag(DiagnosticSeverity::Info, UO, "IgnoredNonPointerConstruct", "Scalar increment/decrement");
                }
                return;
            }
        }

        // Informative logging for control-flow and other non-statement AST nodes
        if (isa<IfStmt>(S) || isa<WhileStmt>(S) || isa<ForStmt>(S) || isa<DoStmt>(S) ||
            isa<BreakStmt>(S) || isa<ContinueStmt>(S) || isa<DefaultStmt>(S) || isa<CaseStmt>(S)) {
            // These structure the CFG blocks; statements inside them are in CFG blocks
            return;
        }

        emitDiag(DiagnosticSeverity::Info, S, "IgnoredConstruct",
                 std::string("Construct not affecting heap reference graph: ") + S->getStmtClassName());
    }

    void translateCFG(std::unique_ptr<CFG>& cfg) {
        for (const auto* block : *cfg) {
            ir::BasicBlock irBlock(block->getBlockID());

            // Successors
            for (auto succ : block->succs()) {
                if (succ.isReachable() && succ.getReachableBlock()) {
                    irBlock.successors.push_back(succ.getReachableBlock()->getBlockID());
                }
            }

            // Predecessors
            for (auto pred : block->preds()) {
                if (pred.isReachable() && pred.getReachableBlock()) {
                    irBlock.predecessors.push_back(pred.getReachableBlock()->getBlockID());
                }
            }

            // Elements
            for (const auto& elem : *block) {
                if (auto cfgStmt = elem.getAs<CFGStmt>()) {
                    const Stmt* S = cfgStmt->getStmt();
                    translateStmt(S, irBlock);
                }
            }

            currentFunction.blocks[irBlock.id] = std::move(irBlock);
        }
    }
};

TranslationResult ClangFrontend::translate(ASTContext& context, const FrontendOptions& options) {
    TranslationResult result;
    result.program = std::make_unique<ir::Program>();
    result.success = true;
    result.isComplete = true;
    result.status = "OK";

    ir::ID nextFuncId = 1;

    for (auto* D : context.getTranslationUnitDecl()->decls()) {
        if (auto* FD = dyn_cast<FunctionDecl>(D)) {
            if (FD->hasBody()) {
                if (context.getSourceManager().isInSystemHeader(FD->getLocation())) {
                    continue;
                }
                ir::Function func(nextFuncId++, FD->getNameAsString());

                CFG::BuildOptions BuildOpts;
                BuildOpts.PruneTriviallyFalseEdges = true;
                BuildOpts.AddInitializers = true;
                BuildOpts.AddImplicitDtors = true;
                BuildOpts.AddTemporaryDtors = true;
                BuildOpts.AddStaticInitBranches = true;
                BuildOpts.AddCXXNewAllocator = true;

                if (std::unique_ptr<CFG> cfg = CFG::buildCFG(FD, FD->getBody(), &context, BuildOpts)) {
                    func.entryBlockId = cfg->getEntry().getBlockID();

                    CFGTranslator translator(*result.program, func, context, options, result);
                    translator.translateCFG(cfg);

                    result.program->functions[func.id] = std::move(func);
                }
            }
        }
    }

    if (!result.success) {
        result.status = "UNSUPPORTED";
    }

    return result;
}

} // namespace frontend
} // namespace mdehra
