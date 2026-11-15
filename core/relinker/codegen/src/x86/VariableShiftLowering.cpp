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
constexpr Form kShiftWholeImmediate{Map0F, Prefix66, false, false, 0x73};
constexpr Form kBlendWords{Map0F3A, Prefix66, false, false, 0x0E};
constexpr Form kMoveAligned128{Map0F, Prefix66, false, false, 0x6F};
constexpr std::uint8_t kShiftQwordsRightExtension = 2;
constexpr std::uint8_t kShiftBytesRightExtension = 3;

void _shiftRightBytes(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source, const std::uint8_t bytes) {
    Register(body, kShiftWholeImmediate, kShiftBytesRightExtension, destination, source, bytes);
}

void _countOf(StubBodyBuilder& body, const VariableShiftOperands& operands, const std::uint8_t count, const std::uint8_t counts, const unsigned element) {
    if (operands.Quadword) {
        _shiftRightBytes(body, count, counts, static_cast<std::uint8_t>(element * 8));
        return;
    }
    switch (element) {
    case 0:
        Register(body, kZeroExtendDwords, count, 0, counts);
        return;
    case 1:
        Register(body, kShiftWholeImmediate, kShiftQwordsRightExtension, count, counts, std::uint8_t{32});
        return;
    case 2:
        _shiftRightBytes(body, count, counts, 8);
        Register(body, kZeroExtendDwords, count, 0, count);
        return;
    default:
        _shiftRightBytes(body, count, counts, 12);
        return;
    }
}

void _lane(StubBodyBuilder& body, const VariableShiftOperands& operands, const std::uint8_t result, const std::uint8_t source, const std::uint8_t counts, const std::uint8_t count, const std::uint8_t shifted) {
    const Form shift{Map0F, Prefix66, false, false, operands.ShiftOpcode};
    const unsigned elements = operands.Quadword ? 2 : 4;
    const unsigned words = 8 / elements;
    for (unsigned element = 0; element < elements; ++element) {
        _countOf(body, operands, count, counts, element);
        if (element == 0) {
            Register(body, shift, result, source, count);
            continue;
        }
        Register(body, shift, shifted, source, count);
        const auto mask = static_cast<std::uint8_t>(((1u << words) - 1) << (element * words));
        Register(body, kBlendWords, result, result, shifted, mask);
    }
}

}

std::optional<VariableShiftOperands> DecodeVariableShift(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66) return std::nullopt;
    VariableShiftOperands operands{nullptr, 0, instruction->Wide, instruction->Vector256, instruction->Reg, instruction->Vvvv, instruction->Rm, instruction->Memory};
    switch (instruction->Opcode) {
    case 0x45:
        operands.Name = instruction->Wide ? "VPSRLVQ" : "VPSRLVD";
        operands.ShiftOpcode = instruction->Wide ? std::uint8_t{0xD3} : std::uint8_t{0xD2};
        break;
    case 0x46:
        if (instruction->Wide) return std::nullopt;
        operands.Name = "VPSRAVD";
        operands.ShiftOpcode = 0xE2;
        break;
    case 0x47:
        operands.Name = instruction->Wide ? "VPSLLVQ" : "VPSLLVD";
        operands.ShiftOpcode = instruction->Wide ? std::uint8_t{0xF3} : std::uint8_t{0xF2};
        break;
    default:
        return std::nullopt;
    }
    if (instruction->End != length) throw CodegenException(std::string(operands.Name) + " has trailing bytes");
    return operands;
}

void EmitVariableShift(StubBodyBuilder& body, const VariableShiftOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Source] = true;
    if (!operands.Memory) used[operands.Count] = true;
    std::array<std::uint8_t, kScratchCount> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < scratch.size(); --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const auto loaded = scratch[0];
    const auto count = scratch[1];
    const auto shifted = scratch[2];
    const auto lowResult = scratch[3];
    const auto highResult = scratch[4];
    const auto highSource = scratch[5];
    const auto highCounts = scratch[6];
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    auto counts = operands.Count;
    if (operands.Memory) {
        Memory(body, operands.Vector256 ? MoveUnaligned256Load : MoveUnaligned128Load, loaded, 0, *operands.Memory, kFrame, operands.Name);
        counts = loaded;
    }
    _lane(body, operands, lowResult, operands.Source, counts, count, shifted);
    if (operands.Vector256) {
        ExtractUpper(body, highSource, operands.Source);
        ExtractUpper(body, highCounts, counts);
        _lane(body, operands, highResult, highSource, highCounts, count, shifted);
        InsertUpper(body, operands.Destination, lowResult, highResult);
    } else {
        Register(body, kMoveAligned128, operands.Destination, 0, lowResult);
    }
    for (std::size_t index = 0; index < scratch.size(); ++index)
        StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
