#include <codegen/x86/VariableShiftLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <string>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::size_t kScratchCount = 7;
constexpr std::int32_t kFrame = 128 + static_cast<std::int32_t>(kScratchCount) * 32;
constexpr Form kZeroExtendDwords{Map0F38, Prefix66, false, false, 0x35};
constexpr Form kShiftBytesRight{Map0F, Prefix66, false, false, 0x73};
constexpr Form kBlendSingles{Map0F3A, Prefix66, false, false, 0x0C};
constexpr Form kBlendDoubles{Map0F3A, Prefix66, false, false, 0x0D};
constexpr Form kMoveAligned128{Map0F, Prefix66, false, false, 0x6F};
constexpr std::uint8_t kShiftRightBytesExtension = 3;

struct Kind {
    const char* Dword;
    const char* Qword;
    std::uint8_t DwordUniform;
    std::uint8_t QwordUniform;
};

std::optional<Kind> _kind(const std::uint8_t opcode) {
    if (opcode == 0x45) return Kind{"VPSRLVD", "VPSRLVQ", 0xD2, 0xD3};
    if (opcode == 0x46) return Kind{"VPSRAVD", nullptr, 0xE2, 0};
    if (opcode == 0x47) return Kind{"VPSLLVD", "VPSLLVQ", 0xF2, 0xF3};
    return std::nullopt;
}

struct Work {
    std::uint8_t Count;
    std::uint8_t Shifted;
};

void _shiftRightBytes(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source) {
    Register(body, kShiftBytesRight, kShiftRightBytesExtension, destination, source, std::uint8_t{8});
}

void _half(StubBodyBuilder& body, const VariableShiftOperands& operands, const std::uint8_t result, const std::uint8_t source, const std::uint8_t counts, const Work& work) {
    const Form uniform{Map0F, Prefix66, false, false, operands.UniformOpcode};
    if (operands.Quadword) {
        Register(body, uniform, result, source, counts);
        _shiftRightBytes(body, work.Count, counts);
        Register(body, uniform, work.Shifted, source, work.Count);
        Register(body, kBlendDoubles, result, result, work.Shifted, std::uint8_t{2});
        return;
    }
    Register(body, kZeroExtendDwords, work.Count, 0, counts);
    Register(body, uniform, result, source, work.Count);
    _shiftRightBytes(body, work.Count, work.Count);
    Register(body, uniform, work.Shifted, source, work.Count);
    Register(body, kBlendSingles, result, result, work.Shifted, std::uint8_t{2});
    _shiftRightBytes(body, work.Count, counts);
    Register(body, kZeroExtendDwords, work.Count, 0, work.Count);
    Register(body, uniform, work.Shifted, source, work.Count);
    Register(body, kBlendSingles, result, result, work.Shifted, std::uint8_t{4});
    _shiftRightBytes(body, work.Count, work.Count);
    Register(body, uniform, work.Shifted, source, work.Count);
    Register(body, kBlendSingles, result, result, work.Shifted, std::uint8_t{8});
}

}

std::optional<VariableShiftOperands> DecodeVariableShift(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66) return std::nullopt;
    const auto kind = _kind(instruction->Opcode);
    if (!kind) return std::nullopt;
    const char* name = instruction->Wide ? kind->Qword : kind->Dword;
    if (name == nullptr) return std::nullopt;
    if (instruction->End != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return VariableShiftOperands{name, instruction->Wide ? kind->QwordUniform : kind->DwordUniform, instruction->Wide, instruction->Vector256,
        instruction->Reg, instruction->Vvvv, instruction->Rm, instruction->Memory};
}

void EmitVariableShift(StubBodyBuilder& body, const VariableShiftOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Source] = true;
    if (!operands.Memory) used[operands.Counts] = true;
    std::array<std::uint8_t, kScratchCount> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < scratch.size(); --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const Work work{scratch[0], scratch[1]};
    const auto result = scratch[2];
    const auto loaded = scratch[3];
    const auto sourceHigh = scratch[4];
    const auto countsHigh = scratch[5];
    const auto resultHigh = scratch[6];
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    auto counts = operands.Counts;
    if (operands.Memory) {
        Memory(body, operands.Vector256 ? MoveUnaligned256Load : MoveUnaligned128Load, loaded, 0, *operands.Memory, kFrame, operands.Name);
        counts = loaded;
    }
    if (operands.Vector256) {
        ExtractUpper(body, sourceHigh, operands.Source);
        ExtractUpper(body, countsHigh, counts);
        _half(body, operands, resultHigh, sourceHigh, countsHigh, work);
    }
    _half(body, operands, result, operands.Source, counts, work);
    if (operands.Vector256)
        InsertUpper(body, operands.Destination, result, resultHigh);
    else
        Register(body, kMoveAligned128, operands.Destination, 0, result);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
