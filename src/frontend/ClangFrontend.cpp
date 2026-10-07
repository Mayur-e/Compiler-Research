#include "mdehra/frontend/ClangFrontend.hpp"
#include "clang/Analysis/CFG.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/Expr.h"
#include <iostream>

namespace mdehra {
namespace frontend {

using namespace clang;

class CFGTranslator {
    ir::Program& program;
    ir::Function& currentFunction;
    ASTContext& context;

    ir::ID nextVarId = 1;
    ir::ID nextAllocSite = 1;

    ir::Variable getOrCreateVariable(const VarDecl* VD) {
        // Use the memory address of the decl as a stable ID for the translation run.
        // Wait, memory addresses aren't deterministic between runs.
        // Let's use name or deterministic increment based on visit order.
        // For deterministic IDs, we can map VarDecl* -> ID.
        // Since Clang traverses AST deterministically, visit order is deterministic.
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

    std::unordered_map<const VarDecl*, ir::Variable> varMap;
    std::unordered_map<const FieldDecl*, ir::Field> fieldMap;

public:
    CFGTranslator(ir::Program& prog, ir::Function& func, ASTContext& ctx)
        : program(prog), currentFunction(func), context(ctx) {}

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

private:
    const VarDecl* getBaseVar(const Expr* E) {
        E = E->IgnoreParenCasts();
        if (auto* DRE = dyn_cast<DeclRefExpr>(E)) {
            if (auto* VD = dyn_cast<VarDecl>(DRE->getDecl())) {
                return VD;
            }
        }
        return nullptr;
    }

    void translateStmt(const Stmt* S, ir::BasicBlock& irBlock) {
        if (auto* BO = dyn_cast<BinaryOperator>(S)) {
            if (BO->getOpcode() == BO_Assign) {
                const Expr* LHS = BO->getLHS()->IgnoreParenCasts();
                const Expr* RHS = BO->getRHS()->IgnoreParenCasts();

                // Check LHS
                if (auto* DRE_L = dyn_cast<DeclRefExpr>(LHS)) {
                    if (auto* VD_L = dyn_cast<VarDecl>(DRE_L->getDecl())) {
                        ir::Variable dest = getOrCreateVariable(VD_L);

                        // RHS can be Variable, Field Load, or Alloc
                        if (auto* DRE_R = dyn_cast<DeclRefExpr>(RHS)) {
                            if (auto* VD_R = dyn_cast<VarDecl>(DRE_R->getDecl())) {
                                irBlock.statements.push_back(
                                    std::make_unique<ir::AssignStmt>(dest, getOrCreateVariable(VD_R)));
                            }
                        } else if (auto* ME_R = dyn_cast<MemberExpr>(RHS)) {
                            if (auto* FD_R = dyn_cast<FieldDecl>(ME_R->getMemberDecl())) {
                                if (const VarDecl* baseVar = getBaseVar(ME_R->getBase())) {
                                    irBlock.statements.push_back(
                                        std::make_unique<ir::LoadStmt>(dest, getOrCreateVariable(baseVar), getOrCreateField(FD_R)));
                                }
                            }
                        } else if (auto* CE_R = dyn_cast<CallExpr>(RHS)) {
                            if (auto* FD_R = CE_R->getDirectCallee()) {
                                std::string funcName = FD_R->getNameInfo().getAsString();
                                if (funcName == "malloc" || funcName == "__builtin_malloc") {
                                    irBlock.statements.push_back(
                                        std::make_unique<ir::AllocStmt>(dest, nextAllocSite++));
                                }
                            }
                        }
                    }
                } else if (auto* ME_L = dyn_cast<MemberExpr>(LHS)) {
                    if (auto* FD_L = dyn_cast<FieldDecl>(ME_L->getMemberDecl())) {
                        if (const VarDecl* baseVarL = getBaseVar(ME_L->getBase())) {
                            ir::Variable base = getOrCreateVariable(baseVarL);
                            ir::Field field = getOrCreateField(FD_L);

                            if (auto* DRE_R = dyn_cast<DeclRefExpr>(RHS)) {
                                if (auto* VD_R = dyn_cast<VarDecl>(DRE_R->getDecl())) {
                                    irBlock.statements.push_back(
                                        std::make_unique<ir::StoreStmt>(base, field, getOrCreateVariable(VD_R)));
                                }
                            }
                        }
                    }
                }
            }
        } else if (auto* RS = dyn_cast<ReturnStmt>(S)) {
            if (const Expr* retVal = RS->getRetValue()) {
                if (const VarDecl* VD = getBaseVar(retVal)) {
                    irBlock.statements.push_back(
                        std::make_unique<ir::ReturnStmt>(getOrCreateVariable(VD)));
                }
            }
        }
    }
};

std::unique_ptr<ir::Program> ClangFrontend::translate(ASTContext& context) {
    auto program = std::make_unique<ir::Program>();
    ir::ID nextFuncId = 1;

    for (auto* D : context.getTranslationUnitDecl()->decls()) {
        if (auto* FD = dyn_cast<FunctionDecl>(D)) {
            if (FD->hasBody()) {
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
                    
                    CFGTranslator translator(*program, func, context);
                    translator.translateCFG(cfg);
                    
                    program->functions[func.id] = std::move(func);
                }
            }
        }
    }

    return program;
}

} // namespace frontend
} // namespace mdehra
