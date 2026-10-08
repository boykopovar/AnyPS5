#ifndef CODEGEN_CODEGENTYPES_HPP
#define CODEGEN_CODEGENTYPES_HPP

#include <codegen/x86/Amd64OnlySubstitutionTypes.hpp>
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

struct TrampolineSite {
    Domain::FileByteOffset Offset;
    Domain::VirtualAddress Address;
    std::size_t Length;
    std::vector<std::uint8_t> OriginalBytes;
    std::vector<std::uint8_t> Body;
    std::size_t ReturnBranchOffset;
    std::vector<StubRelocation> Relocations = {};
};

struct ConvertResult {
    std::vector<std::uint8_t> Bytes;
    std::size_t ReplacedCount;
    std::vector<TrampolineSite> Trampolines;
    std::vector<Amd64OnlySubstitutionReport> Reports;
    std::size_t KeptCount = 0;
};

enum class ControlFlowKind : std::uint8_t {
    Sequential,
    ConditionalBranch,
    UnconditionalJump,
    Call,
    Return,
    IndirectJump,
    IndirectCall,
    Trap,
};

struct DecodedInstructionInfo {
    std::size_t Length;
    std::size_t OpcodeOffset;
    std::uint8_t SegmentPrefix;
    std::uint8_t RexPrefix;
    ControlFlowKind FlowKind;
    bool HasRipRelativeDisp;
    std::size_t RipRelativeDispOffset;
    bool HasModRm;
    std::uint8_t ModRmByte;
    bool HasBranchTarget;
    std::int64_t BranchDisp;
    bool IsTwoByteOpcode;
    std::uint8_t Opcode;
    std::uint8_t ModRmRegField;
};

}

#endif
