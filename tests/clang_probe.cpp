#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <memory>
#include <string>

using namespace clang;
using namespace clang::tooling;

class CFGProbeVisitor : public ASTConsumer {
public:
    void HandleTranslationUnit(ASTContext &Context) override {
        for (auto *D : Context.getTranslationUnitDecl()->decls()) {
            if (auto *FD = dyn_cast<FunctionDecl>(D)) {
                if (FD->hasBody()) {
                    std::cout << "Successfully found function: " << FD->getNameAsString() << std::endl;
                    
                    CFG::BuildOptions BuildOpts;
                    std::unique_ptr<CFG> cfg = CFG::buildCFG(
                        FD, FD->getBody(), &Context, BuildOpts);
                    
                    if (cfg) {
                        std::cout << "Successfully built CFG for " << FD->getNameAsString() << " with " << cfg->size() << " blocks." << std::endl;
                    } else {
                        std::cout << "Failed to build CFG." << std::endl;
                    }
                }
            }
        }
    }
};

class CFGProbeAction : public ASTFrontendAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        return std::make_unique<CFGProbeVisitor>();
    }
};

int main() {
    std::string code = R"(
        int f(int x) {
            if (x)
                return 1;
            return 0;
        }
    )";
    
    std::cout << "Running Clang Probe..." << std::endl;
    bool success = runToolOnCode(std::make_unique<CFGProbeAction>(), code);
    
    if (success) {
        std::cout << "Clang Probe Completed Successfully." << std::endl;
        return 0;
    } else {
        std::cerr << "Clang Probe Failed." << std::endl;
        return 1;
    }
}
