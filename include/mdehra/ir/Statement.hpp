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

class Statement {
public:
    virtual ~Statement() = default;
    virtual StmtType getType() const = 0;
};

class AssignStmt : public Statement {
public:
    Variable dest;
    Variable src;

    AssignStmt(Variable d, Variable s) : dest(std::move(d)), src(std::move(s)) {}
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

    StoreStmt(Variable b, Field f, Variable v) : base(std::move(b)), field(std::move(f)), value(std::move(v)) {}
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

class UnknownStmt : public Statement {
public:
    UnknownStmt() {}
    StmtType getType() const override { return StmtType::Unknown; }
};

} // namespace ir
} // namespace mdehra
