#ifndef CODEGEN_X86_STUBBODYBUILDER_HPP
#define CODEGEN_X86_STUBBODYBUILDER_HPP

#include <codegen/CodegenTypes.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTypes.hpp>
#include <domain/Types.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

namespace Codegen {

struct LoweredBody {
    std::vector<std::uint8_t> Bytes;
    std::size_t ReturnBranchOffset;
    std::vector<StubRelocation> Relocations = {};
};

using StubConstant = std::array<std::uint8_t, 16>;

struct MemoryOperand {
    std::vector<std::uint8_t> Prefixes;
    std::uint8_t RexIndexBase;
    std::uint8_t Mod;
    std::uint8_t Rm;
    std::uint8_t Sib;
    std::int32_t Displacement;
    bool StackBase;
    std::size_t EncodedSize;
    bool RipRelative;
    std::size_t InstructionLength;
};

[[nodiscard]] MemoryOperand DecodeMemoryOperand(const std::uint8_t* data, std::size_t length, std::size_t modRmOffset, std::uint8_t rex, std::vector<std::uint8_t> prefixes);

void EmitSse(std::vector<std::uint8_t>& out, std::uint8_t prefix, std::initializer_list<std::uint8_t> opcode, std::uint8_t dst, std::uint8_t src);
void ApplyStubRelocations(std::span<std::uint8_t> body, std::span<const StubRelocation> relocations, std::uint64_t siteAddress, std::uint64_t stubAddress, Domain::FileByteOffset failureOffset);

class StubBodyBuilder {
public:
    void Sse(std::uint8_t prefix, std::initializer_list<std::uint8_t> opcode, std::uint8_t dst, std::uint8_t src);
    void SsePlain(std::initializer_list<std::uint8_t> opcode, std::uint8_t dst, std::uint8_t src);
    void SseImm(std::uint8_t prefix, std::initializer_list<std::uint8_t> opcode, std::uint8_t dst, std::uint8_t src, std::uint8_t imm);
    void ShiftImm(std::uint8_t extension, std::uint8_t reg, std::uint8_t imm);
    void ShiftDwordImm(std::uint8_t extension, std::uint8_t reg, std::uint8_t imm);
    void RipOperand(std::initializer_list<std::uint8_t> opcode, std::uint8_t reg, const StubConstant& constant);
    void Load(std::uint8_t reg, const MemoryOperand& operand);
    void Spill(std::uint8_t reg);
    void Restore(std::uint8_t reg);
    void Raw(std::span<const std::uint8_t> bytes);
    void Move(std::span<const std::uint8_t> instruction, std::optional<std::size_t> ripDisplacementOffset);
    void RipRelative(std::span<const std::uint8_t> instruction, std::size_t displacementOffset, std::int64_t target);
    void MoveControlFlow(std::span<const std::uint8_t> instruction, const DecodedInstructionInfo& info);
    void JumpTo(std::int64_t siteTarget);
    void Seek(std::int64_t siteOffset);
    void Advance(std::size_t length);
    [[nodiscard]] LoweredBody Finish();

private:
    struct Fixup {
        std::size_t DisplacementOffset;
        std::size_t InstructionEnd;
        std::size_t ConstantIndex;
    };

    void _rel32(std::initializer_list<std::uint8_t> opcode, std::int64_t siteTarget);
    void _pushReturnAddress(std::int64_t siteTarget);

    std::vector<std::uint8_t> _bytes;
    std::vector<Fixup> _fixups;
    std::vector<StubConstant> _constants;
    std::vector<StubRelocation> _relocations;
    std::size_t _stackDepth = 0;
    std::int64_t _siteOffset = 0;
};

}

#endif
