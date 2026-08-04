#ifndef CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP
#define CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP

#include <codegen/IInstructionRewriter.hpp>
#include <cstdint>

namespace Codegen {

class X64InstructionRewriter : public IInstructionRewriter {
public:
    [[nodiscard]] RewriteResult Rewrite(
        const std::vector<std::uint8_t>& codeSection,
        const RewriteRequest& request
    ) const override;

private:
    enum class ReferenceKind {
        None,
        Rel8Branch,
        Rel32Branch,
        RipRelativeDisp32
    };

    struct ReferenceSite {
        ReferenceKind Kind;
        std::size_t FieldOffset;
    };

    static constexpr std::uint8_t OpcodePrefixLock = 0xF0;
    static constexpr std::uint8_t OpcodePrefixRepne = 0xF2;
    static constexpr std::uint8_t OpcodePrefixRep = 0xF3;
    static constexpr std::uint8_t OpcodePrefixSegCs = 0x2E;
    static constexpr std::uint8_t OpcodePrefixSegSs = 0x36;
    static constexpr std::uint8_t OpcodePrefixSegDs = 0x3E;
    static constexpr std::uint8_t OpcodePrefixSegEs = 0x26;
    static constexpr std::uint8_t OpcodePrefixSegFs = 0x64;
    static constexpr std::uint8_t OpcodePrefixSegGs = 0x65;
    static constexpr std::uint8_t OpcodePrefixOperandSize = 0x66;
    static constexpr std::uint8_t OpcodePrefixAddressSize = 0x67;

    static constexpr std::uint8_t RexMin = 0x40;
    static constexpr std::uint8_t RexMax = 0x4F;

    static constexpr std::uint8_t TwoByteOpcodeEscape = 0x0F;

    static constexpr std::uint8_t OpcodeCallRel32 = 0xE8;
    static constexpr std::uint8_t OpcodeJmpRel32 = 0xE9;
    static constexpr std::uint8_t OpcodeJmpRel8 = 0xEB;
    static constexpr std::uint8_t OpcodeJccRel8Min = 0x70;
    static constexpr std::uint8_t OpcodeJccRel8Max = 0x7F;
    static constexpr std::uint8_t OpcodeJccRel32Min = 0x80;
    static constexpr std::uint8_t OpcodeJccRel32Max = 0x8F;

    static constexpr std::size_t Rel32InstructionLength = 5;
    static constexpr std::size_t Rel8FieldSize = 1;
    static constexpr std::size_t Rel32FieldSize = 4;
    static constexpr std::size_t Disp32FieldSize = 4;

    static constexpr std::uint8_t ModRmModShift = 6;
    static constexpr std::uint8_t ModRmModMask = 0x03;
    static constexpr std::uint8_t ModRmRmMask = 0x07;
    static constexpr std::uint8_t ModRmModIndirect = 0x00;
    static constexpr std::uint8_t ModRmRmRipRelative = 0x05;

    [[nodiscard]] static ReferenceSite ClassifyInstruction(
        const std::uint8_t* data,
        std::size_t length
    );

    [[nodiscard]] static std::int32_t ReadInt32(const std::uint8_t* data);

    static void WriteInt32(std::uint8_t* data, std::int32_t value);

    [[nodiscard]] static std::int64_t AdjustedDelta(
        std::int64_t instrPosition,
        std::int64_t targetPosition,
        std::int64_t cutStart,
        std::int64_t delta
    );
};

}

#endif
