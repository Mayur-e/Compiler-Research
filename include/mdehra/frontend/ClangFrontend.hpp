#pragma once
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "mdehra/ir/Program.hpp"
#include "mdehra/ir/Statement.hpp"

namespace mdehra {
namespace frontend {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Unsupported,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::string filename;
    unsigned line = 0;
    unsigned col = 0;
    std::string constructType;
    std::string message;

    std::string severityString() const {
        switch (severity) {
            case DiagnosticSeverity::Info:        return "INFO";
            case DiagnosticSeverity::Warning:     return "WARNING";
            case DiagnosticSeverity::Unsupported: return "UNSUPPORTED";
            case DiagnosticSeverity::Error:       return "ERROR";
        }
        return "UNKNOWN";
    }

    std::string toString() const {
        std::stringstream ss;
        if (!filename.empty()) {
            ss << filename << ":" << line << ":" << col << ": ";
        }
        ss << "[" << severityString() << "] [" << constructType << "] " << message;
        return ss.str();
    }
};

struct FrontendOptions {
    bool strictMode = false;           // Fails on unsupported pointer/heap constructs
    bool trackSourceLocations = true;  // Preserves file:line:col metadata on statements
};

struct TranslationResult {
    std::unique_ptr<ir::Program> program;
    std::vector<Diagnostic> diagnostics;
    bool success = true;
    bool isComplete = true; // false if permissive mode skipped any construct
    std::string status = "OK"; // "OK", "UNSUPPORTED", "ANALYSIS_ERROR"

    size_t countSeverity(DiagnosticSeverity sev) const {
        size_t c = 0;
        for (const auto& d : diagnostics) if (d.severity == sev) c++;
        return c;
    }
};

class ClangFrontend {
public:
    // Translates Clang AST into Custom IR Program with structured diagnostics and strict/permissive mode
    static TranslationResult translate(clang::ASTContext& context, const FrontendOptions& options);

    // Backward-compatible overload
    static std::unique_ptr<ir::Program> translate(clang::ASTContext& context) {
        return translate(context, FrontendOptions{}).program;
    }
};

} // namespace frontend
} // namespace mdehra
