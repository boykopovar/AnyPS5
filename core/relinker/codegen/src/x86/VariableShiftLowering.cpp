#include <codegen/x86/VariableShiftLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <string>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::uint8_t kCountRegister = 1;
constexpr std::int32_t kValueSlot = 8;
constexpr std::int32_t kSaturatedSlot = 16;
constexpr std::int32_t kCountSlot = 24;
constexpr std::int32_t kSourceSlot = 32;
constexpr std::int32_t kCountsSlot = 64;
constexpr std::int32_t kVectorSlot = 96;
constexpr std::int32_t kStorage = 128;
constexpr std::int32_t kRedZone = 128;
constexpr std::int32_t kFrame = kRedZone + kStorage;
constexpr std::uint8_t kPushFlags = 0x9C;
constexpr std::uint8_t kPopFlags = 0x9D;
constexpr std::uint8_t kLoadEffectiveAddress = 0x8D;
constexpr std::uint8_t kMoveToRegister = 0x8B;
constexpr std::uint8_t kMoveFromRegister = 0x89;
constexpr std::uint8_t kShiftByCount = 0xD3;
constexpr std::uint8_t kShiftByImmediate = 0xC1;
constexpr std::uint8_t kCompareImmediate = 0x83;
constexpr std::uint8_t kExclusiveOr = 0x31;
constexpr std::uint8_t kCompareExtension = 7;
constexpr std::uint8_t kConditionalMoveAboveOrEqual = 0x43;
constexpr Form kStore128{Map0F, PrefixF3, false, false, 0x7F};

std::uint8_t _shiftExtension(const VariableShiftOperation operation) {
    switch (operation) {
    case VariableShiftOperation::Left: return 4;
    case VariableShiftOperation::LogicalRight: return 5;
    case VariableShiftOperation::ArithmeticRight: return 7;
    }
    throw CodegenException("unknown variable shift operation");
}

std::uint8_t _vectorScratch(std::array<bool, 16>& used, const char* instruction) {
    for (std::uint8_t reg = 16; reg-- > 0;) {
        if (used[reg]) continue;
        used[reg] = true;
        return reg;
    }
    throw CodegenException(std::string(instruction) + " has no scratch vector register");
}

void _conditionalMove(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source, const bool wide) {
    body.Raw(std::array{Rex(wide, destination, source), std::uint8_t{0x0F}, kConditionalMoveAboveOrEqual,
        static_cast<std::uint8_t>(0xC0 | ((destination & 7) << 3) | (source & 7))});
}

void _element(StubBodyBuilder& body, const VariableShiftOperands& operands, const std::uint8_t value, const std::uint8_t saturated, const std::int32_t offset) {
    const bool wide = operands.Wide;
    const auto extension = _shiftExtension(operands.Operation);
    StackSlot(body, kMoveToRegister, wide, kCountRegister, kCountsSlot + offset);
    StackSlot(body, kMoveToRegister, wide, value, kSourceSlot + offset);
    if (operands.Operation == VariableShiftOperation::ArithmeticRight) {
        Register(body, kMoveFromRegister, wide, value, saturated);
        Register(body, kShiftByImmediate, wide, extension, saturated);
        body.Raw(std::array{static_cast<std::uint8_t>(wide ? 63 : 31)});
    } else {
        Register(body, kExclusiveOr, false, saturated, saturated);
    }
    Register(body, kShiftByCount, wide, extension, value);
    Register(body, kCompareImmediate, wide, kCompareExtension, kCountRegister);
    body.Raw(std::array{static_cast<std::uint8_t>(wide ? 64 : 32)});
    _conditionalMove(body, value, saturated, wide);
    StackSlot(body, kMoveFromRegister, wide, value, kSourceSlot + offset);
}

}

const char* VariableShiftName(const VariableShiftOperation operation, const bool wide) {
    switch (operation) {
    case VariableShiftOperation::Left: return wide ? "VPSLLVQ" : "VPSLLVD";
    case VariableShiftOperation::LogicalRight: return wide ? "VPSRLVQ" : "VPSRLVD";
    case VariableShiftOperation::ArithmeticRight: return "VPSRAVD";
    }
    throw CodegenException("unknown variable shift operation");
}

std::optional<VariableShiftOperands> DecodeVariableShift(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66) return std::nullopt;
    VariableShiftOperation operation;
    if (instruction->Opcode == 0x47) operation = VariableShiftOperation::Left;
    else if (instruction->Opcode == 0x45) operation = VariableShiftOperation::LogicalRight;
    else if (instruction->Opcode == 0x46) operation = VariableShiftOperation::ArithmeticRight;
    else return std::nullopt;
    if (operation == VariableShiftOperation::ArithmeticRight && instruction->Wide) return std::nullopt;
    const auto name = VariableShiftName(operation, instruction->Wide);
    if (instruction->End != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return VariableShiftOperands{name, operation, instruction->Wide, instruction->Vector256,
        instruction->Reg, instruction->Vvvv, instruction->Rm, instruction->Memory};
}

void EmitVariableShift(StubBodyBuilder& body, const VariableShiftOperands& operands) {
    auto usedGeneral = UsedRegisters({kCountRegister}, operands.Memory);
    const auto value = Scratch(usedGeneral, operands.Name);
    const auto saturated = Scratch(usedGeneral, operands.Name);
    std::array<bool, 16> usedVector{};
    usedVector[operands.Destination] = true;
    usedVector[operands.Source] = true;
    if (!operands.Memory) usedVector[operands.Counts] = true;
    const auto store = operands.Vector256 ? MoveUnaligned256Store : kStore128;
    const auto load = operands.Vector256 ? MoveUnaligned256Load : MoveUnaligned128Load;
    StackSlot(body, kLoadEffectiveAddress, true, Stack, 8 - kFrame);
    body.Raw(std::array{kPushFlags});
    StackSlot(body, kMoveFromRegister, true, value, kValueSlot);
    StackSlot(body, kMoveFromRegister, true, saturated, kSaturatedSlot);
    StackSlot(body, kMoveFromRegister, true, kCountRegister, kCountSlot);
    StackOperand(body, store, operands.Source, 0, kSourceSlot);
    if (operands.Memory) {
        const auto scratch = _vectorScratch(usedVector, operands.Name);
        StackOperand(body, MoveUnaligned256Store, scratch, 0, kVectorSlot);
        Memory(body, load, scratch, 0, *operands.Memory, kFrame, operands.Name);
        StackOperand(body, store, scratch, 0, kCountsSlot);
        StackOperand(body, MoveUnaligned256Load, scratch, 0, kVectorSlot);
    } else {
        StackOperand(body, store, operands.Counts, 0, kCountsSlot);
    }
    const std::int32_t size = operands.Wide ? 8 : 4;
    const std::int32_t width = operands.Vector256 ? 32 : 16;
    for (std::int32_t offset = 0; offset < width; offset += size)
        _element(body, operands, value, saturated, offset);
    StackOperand(body, load, operands.Destination, 0, kSourceSlot);
    StackSlot(body, kMoveToRegister, true, kCountRegister, kCountSlot);
    StackSlot(body, kMoveToRegister, true, saturated, kSaturatedSlot);
    StackSlot(body, kMoveToRegister, true, value, kValueSlot);
    body.Raw(std::array{kPopFlags});
    StackSlot(body, kLoadEffectiveAddress, true, Stack, kFrame - 8);
}

}
