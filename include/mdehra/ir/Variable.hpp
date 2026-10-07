#pragma once
#include <string>
#include "mdehra/ir/ProgramPoint.hpp"

namespace mdehra {
namespace ir {

struct Variable {
    ID id = INVALID_ID;
    std::string name;
    bool isPointer = false;

    bool operator==(const Variable& other) const {
        return id == other.id;
    }

    bool operator<(const Variable& other) const {
        if (id != other.id) return id < other.id;
        return name < other.name;
    }
};

struct Field {
    ID id = INVALID_ID;
    std::string name;

    bool operator==(const Field& other) const {
        return id == other.id;
    }

    bool operator<(const Field& other) const {
        if (id != other.id) return id < other.id;
        return name < other.name;
    }
};

} // namespace ir
} // namespace mdehra
