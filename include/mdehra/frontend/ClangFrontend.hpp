#pragma once
#include <memory>
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "mdehra/ir/Program.hpp"

namespace mdehra {
namespace frontend {

class ClangFrontend {
public:
    // Translates the given Clang AST Translation Unit into a Custom IR Program
    static std::unique_ptr<ir::Program> translate(clang::ASTContext& context);
};

} // namespace frontend
} // namespace mdehra
