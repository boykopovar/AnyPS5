#ifndef CODEGEN_CODEGENTYPES_HPP
#define CODEGEN_CODEGENTYPES_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <vector>

namespace Codegen {

struct InstructionMatch {
    Domain::FileByteOffset Offset;
    std::size_t Length;
};

struct RewriteRequest {
    Domain::FileByteOffset Offset;
    std::vector<std::uint8_t> NewBytes;
};

struct AddressAdjustment {
    Domain::FileByteOffset OriginalOffset;
    Domain::FileByteOffset AdjustedOffset;
    std::int64_t Delta;
};

struct RewriteResult {
    std::vector<std::uint8_t> Bytes;
    std::vector<AddressAdjustment> Adjustments;
};

struct ConvertResult {
    std::vector<std::uint8_t> Bytes;
    std::size_t ReplacedCount;
};

enum class ControlFlowKind : std::uint8_t {
    Sequential,
    ConditionalBranch,
    UnconditionalJump,
    Call,
    Return,
    IndirectJump,
    IndirectCall,
};

struct DecodedInstructionInfo {
    std::size_t Length;
    ControlFlowKind FlowKind;
    bool HasRipRelativeDisp;
    std::size_t RipRelativeDispOffset;
    bool HasModRm;
    std::uint8_t ModRmByte;
    bool HasBranchTarget;
    std::int64_t BranchDisp;
};

}

#endif
