#pragma once
#include <cstdint>
#include <string>

namespace mdehra {
namespace ir {

// A stable identifier type
using ID = std::uint32_t;
const ID INVALID_ID = 0xFFFFFFFF;

// Program Point definition
struct ProgramPoint {
    ID functionId = INVALID_ID;
    ID blockId = INVALID_ID;
    ID stmtIndex = INVALID_ID;

    bool operator==(const ProgramPoint& other) const {
        return functionId == other.functionId &&
               blockId == other.blockId &&
               stmtIndex == other.stmtIndex;
    }

    bool operator<(const ProgramPoint& other) const {
        if (functionId != other.functionId) return functionId < other.functionId;
        if (blockId != other.blockId) return blockId < other.blockId;
        return stmtIndex < other.stmtIndex;
    }
};

} // namespace ir
} // namespace mdehra
