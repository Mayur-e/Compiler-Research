#pragma once
#include <memory>
#include <vector>
#include "mdehra/ir/ProgramPoint.hpp"
#include "mdehra/ir/Variable.hpp"

namespace mdehra {
namespace ir {

enum class StmtType {
    Assign,
    Load,
    Store,
    Alloc,
    Return,
    Call,
    Unknown
};

struct SourceLocation {
    std::string filename;
    unsigned line = 0;
    unsigned col = 0;
};

class Statement {
public:
    virtual ~Statement() = default;
    virtual StmtType getType() const = 0;
    SourceLocation location;
};

class AssignStmt : public Statement {
public:
    Variable dest;
    Variable src;
    bool isNull = false;

    AssignStmt(Variable d, Variable s) : dest(std::move(d)), src(std::move(s)), isNull(false) {}
    static std::unique_ptr<AssignStmt> createNull(Variable d) {
        auto s = std::make_unique<AssignStmt>(std::move(d), Variable());
        s->isNull = true;
        return s;
    }
    StmtType getType() const override { return StmtType::Assign; }
};

class LoadStmt : public Statement {
public:
    Variable dest;
    Variable base;
    Field field;

    LoadStmt(Variable d, Variable b, Field f) : dest(std::move(d)), base(std::move(b)), field(std::move(f)) {}
    StmtType getType() const override { return StmtType::Load; }
};

class StoreStmt : public Statement {
public:
    Variable base;
    Field field;
    Variable value;
    bool isNull = false;

    StoreStmt(Variable b, Field f, Variable v) : base(std::move(b)), field(std::move(f)), value(std::move(v)), isNull(false) {}
    static std::unique_ptr<StoreStmt> createNull(Variable b, Field f) {
        auto s = std::make_unique<StoreStmt>(std::move(b), std::move(f), Variable());
        s->isNull = true;
        return s;
    }
    StmtType getType() const override { return StmtType::Store; }
};

class AllocStmt : public Statement {
public:
    Variable dest;
    ID allocSite;

    AllocStmt(Variable d, ID site) : dest(std::move(d)), allocSite(site) {}
    StmtType getType() const override { return StmtType::Alloc; }
};

class ReturnStmt : public Statement {
public:
    Variable value; // Can be invalid if returning void
    bool hasValue;

    ReturnStmt() : hasValue(false) {}
    ReturnStmt(Variable v) : value(std::move(v)), hasValue(true) {}
    StmtType getType() const override { return StmtType::Return; }
};

class CallStmt : public Statement {
public:
    std::string calleeName;
    std::vector<Variable> arguments;

    CallStmt(std::string name, std::vector<Variable> args)
        : calleeName(std::move(name)), arguments(std::move(args)) {}
    StmtType getType() const override { return StmtType::Call; }
};

class UnknownStmt : public Statement {
public:
    std::string description;
    UnknownStmt(std::string desc = "") : description(std::move(desc)) {}
    StmtType getType() const override { return StmtType::Unknown; }
};

} // namespace ir
} // namespace mdehra
