#include <codegen/x86/ShiftLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::uint8_t kCountRegister = 1;
constexpr std::int32_t kResultSlot = 0;
constexpr std::int32_t kCountSlot = 8;
constexpr std::int32_t kFlagsSlot = 16;
constexpr std::int32_t kStackResultSlot = 24;
constexpr std::uint8_t kPushFlags = 0x9C;
constexpr std::uint8_t kPopFlags = 0x9D;
constexpr std::int32_t kFrame = FrameSize + 16;

std::uint8_t _shiftDigit(const ShiftOperation operation) {
    switch (operation) {
    case ShiftOperation::Left: return 4;
    case ShiftOperation::LogicalRight: return 5;
    case ShiftOperation::ArithmeticRight: return 7;
    case ShiftOperation::RotateRight: return 1;
    }
    throw CodegenException("unknown shift operation");
}

void _loadSource(StubBodyBuilder& body, const ShiftOperands& operands, const std::uint8_t result, const char* name) {
    if (operands.Memory)
        Memory(body, 0x8B, operands.Wide, *operands.Memory, result, name, kFrame);
    else if (operands.Source == Stack)
        StackSlot(body, 0x8D, true, result, kFrame);
    else
        Register(body, 0x8B, operands.Wide, result, operands.Source);
}

}

const char* ShiftName(const ShiftOperation operation) {
    switch (operation) {
    case ShiftOperation::Left: return "SHLX";
    case ShiftOperation::LogicalRight: return "SHRX";
    case ShiftOperation::ArithmeticRight: return "SARX";
    case ShiftOperation::RotateRight: return "RORX";
    }
    throw CodegenException("unknown shift operation");
}

std::optional<ShiftOperands> DecodeShift(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4) return std::nullopt;
    const auto map = static_cast<std::uint8_t>(data[vex + 1] & 0x1F);
    const auto pp = static_cast<std::uint8_t>(data[vex + 2] & 3);
    ShiftOperation operation;
    if (map == 2 && data[vex + 3] == 0xF7 && pp != 0)
        operation = pp == 1 ? ShiftOperation::Left : pp == 2 ? ShiftOperation::ArithmeticRight : ShiftOperation::LogicalRight;
    else if (map == 3 && data[vex + 3] == 0xF0 && pp == 3)
        operation = ShiftOperation::RotateRight;
    else
        return std::nullopt;
    const auto name = ShiftName(operation);
    if ((data[vex + 2] & 4) != 0) throw CodegenException(std::string(name) + " requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    const auto vvvv = static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15);
    if (operation == ShiftOperation::RotateRight && vvvv != 0) throw CodegenException("RORX requires VEX.vvvv=1111");
    ShiftOperands operands{
        operation,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        vvvv,
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    std::size_t end = vex + 5;
    if ((modrm >> 6) != 3) {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        end = vex + 4 + operands.Memory->EncodedSize;
    }
    if (operation == ShiftOperation::RotateRight) {
        if (end >= length) throw CodegenException("RORX is missing its immediate");
        operands.Count = data[end];
        ++end;
    }
    if (end != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return operands;
}

void EmitShift(StubBodyBuilder& body, const ShiftOperands& operands) {
    const auto name = ShiftName(operands.Operation);
    const bool rotate = operands.Operation == ShiftOperation::RotateRight;
    std::array<bool, 16> used{};
    if (rotate)
        used = operands.Memory ? UsedRegisters({operands.Destination}, operands.Memory) : UsedRegisters({operands.Destination, operands.Source}, std::nullopt);
    else
        used = operands.Memory ? UsedRegisters({operands.Destination, operands.Count, kCountRegister}, operands.Memory)
                               : UsedRegisters({operands.Destination, operands.Source, operands.Count, kCountRegister}, std::nullopt);
    const auto result = Scratch(used, name);
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    StackSlot(body, 0x89, true, result, kResultSlot);
    if (!rotate) StackSlot(body, 0x89, true, kCountRegister, kCountSlot);
    body.Raw(std::array{kPushFlags});
    StackSlot(body, 0x8F, false, 0, kFlagsSlot);
    _loadSource(body, operands, result, name);
    if (rotate) {
        Register(body, 0xC1, operands.Wide, _shiftDigit(operands.Operation), result);
        body.Raw(std::array{operands.Count});
    } else {
        if (operands.Count == Stack)
            StackSlot(body, 0x8D, true, kCountRegister, kFrame);
        else if (operands.Count != kCountRegister)
            Register(body, 0x8B, true, kCountRegister, operands.Count);
        Register(body, 0xD3, operands.Wide, _shiftDigit(operands.Operation), result);
    }
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, result, kStackResultSlot);
    else if (!rotate && operands.Destination == kCountRegister)
        StackSlot(body, 0x89, true, result, kCountSlot);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, result);
    StackSlot(body, 0xFF, false, 6, kFlagsSlot);
    body.Raw(std::array{kPopFlags});
    if (!rotate) StackSlot(body, 0x8B, true, kCountRegister, kCountSlot);
    StackSlot(body, 0x8B, true, result, kResultSlot);
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, kStackResultSlot);
    else
        StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
