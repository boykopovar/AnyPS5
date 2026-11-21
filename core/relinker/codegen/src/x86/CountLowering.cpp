#include <codegen/x86/CountLowering.hpp>
#include <codegen/x86/ScalarPrefixes.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::int32_t kResultSlot = 0;
constexpr std::int32_t kFallbackSlot = 8;
constexpr std::int32_t kZeroSlot = 16;
constexpr std::int32_t kMaskSlot = 24;
constexpr std::int32_t kStackResultSlot = 32;
constexpr std::int32_t kFrame = FrameSize + 32;

void _loadSource(StubBodyBuilder& body, const CountOperands& operands, const std::uint8_t result, const char* name) {
    if (operands.Width == 16)
        MoveImmediate32(body, result, 0);
    if (operands.Memory) {
        SizedMemory(body, 0x8B, operands.Width, *operands.Memory, result, name, kFrame);
        return;
    }
    if (operands.Source == Stack) {
        StackSlot(body, 0x8D, true, result, kFrame);
        if (operands.Width == 32)
            Register(body, 0x8B, false, result, result);
        else if (operands.Width == 16)
            TwoByteRegister(body, 0xB7, false, result, result);
        return;
    }
    SizedRegister(body, 0x8B, operands.Width, result, operands.Source);
}

void _count(StubBodyBuilder& body, const CountOperands& operands, const std::uint8_t result, const std::uint8_t fallback) {
    const bool wide = operands.Width == 64;
    const bool trailing = operands.Operation == CountOperation::TrailingZeros;
    TwoByteRegister(body, trailing ? 0xBC : 0xBD, wide, result, result);
    body.Raw(std::vector<std::uint8_t>{0x0F, 0x94, 0x84, 0x24, kZeroSlot, 0, 0, 0});
    MoveImmediate32(body, fallback, trailing ? operands.Width : 2u * operands.Width - 1u);
    TwoByteRegister(body, 0x44, wide, result, fallback);
    if (!trailing)
        body.Raw(std::vector<std::uint8_t>{Rex(false, 0, result), 0x83, static_cast<std::uint8_t>(0xF0 | (result & 7)), static_cast<std::uint8_t>(operands.Width - 1)});
}

void _flags(StubBodyBuilder& body, const std::uint8_t result, const std::uint8_t fallback) {
    TwoByteStackSlot(body, 0xB6, fallback, kZeroSlot);
    body.Raw(std::vector<std::uint8_t>{Rex(false, 0, fallback), 0xFF, static_cast<std::uint8_t>(0xC8 | (fallback & 7))});
    StackSlot(body, 0x89, false, fallback, kMaskSlot);
    std::vector<std::uint8_t> next{Rex(false, fallback, result), 0x8D, static_cast<std::uint8_t>(0x40 | ((fallback & 7) << 3) | (result & 7))};
    if ((result & 7) == Stack) next.push_back(0x24);
    next.push_back(1);
    body.Raw(next);
    StackSlot(body, 0x23, false, fallback, kMaskSlot);
    body.Raw(std::vector<std::uint8_t>{Rex(false, 0, fallback), 0x83, static_cast<std::uint8_t>(0xF8 | (fallback & 7)), 1});
}

void _store(StubBodyBuilder& body, const CountOperands& operands, const std::uint8_t result, const std::uint8_t fallback) {
    if (operands.Destination != Stack) {
        SizedRegister(body, 0x8B, operands.Width, operands.Destination, result);
        return;
    }
    StackSlot(body, 0x8D, true, fallback, kFrame);
    StackSlot(body, 0x89, true, fallback, kStackResultSlot);
    SizedStackSlot(body, 0x89, operands.Width == 16 ? 16 : 64, result, kStackResultSlot);
}

}

const char* CountName(const CountOperation operation) {
    switch (operation) {
    case CountOperation::TrailingZeros: return "TZCNT";
    case CountOperation::LeadingZeros: return "LZCNT";
    }
    throw CodegenException("unknown count operation");
}

std::optional<CountOperands> DecodeCount(const std::uint8_t* data, const std::size_t length) {
    const auto encoding = ReadLegacyEncoding(data, length);
    const auto opcode = encoding.Opcode;
    if (!encoding.Repeat || opcode + 3 > length || data[opcode] != 0x0F || (data[opcode + 1] != 0xBC && data[opcode + 1] != 0xBD))
        return std::nullopt;
    const auto operation = data[opcode + 1] == 0xBC ? CountOperation::TrailingZeros : CountOperation::LeadingZeros;
    const auto name = CountName(operation);
    if (encoding.RepeatNot) throw CodegenException(std::string(name) + " carries both F2 and F3");
    const auto modrm = data[opcode + 2];
    CountOperands operands{
        operation,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((encoding.Rex & 4) << 1)),
        static_cast<std::uint8_t>((modrm & 7) | ((encoding.Rex & 1) << 3)),
        static_cast<std::uint8_t>((encoding.Rex & 8) != 0 ? 64 : encoding.OperandSize ? 16 : 32),
        std::nullopt};
    std::size_t end = opcode + 3;
    if ((modrm >> 6) != 3) {
        operands.Memory = DecodeMemoryOperand(data, length, opcode + 2, static_cast<std::uint8_t>(encoding.Rex & 3), encoding.Segment);
        end = opcode + 2 + operands.Memory->EncodedSize;
    }
    if (end != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return operands;
}

void EmitCount(StubBodyBuilder& body, const CountOperands& operands) {
    const auto name = CountName(operands.Operation);
    auto used = operands.Memory ? UsedRegisters({operands.Destination}, operands.Memory)
                                : UsedRegisters({operands.Destination, operands.Source}, std::nullopt);
    const auto result = Scratch(used, name);
    const auto fallback = Scratch(used, name);
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    StackSlot(body, 0x89, true, result, kResultSlot);
    StackSlot(body, 0x89, true, fallback, kFallbackSlot);
    _loadSource(body, operands, result, name);
    _count(body, operands, result, fallback);
    _flags(body, result, fallback);
    _store(body, operands, result, fallback);
    StackSlot(body, 0x8B, true, fallback, kFallbackSlot);
    StackSlot(body, 0x8B, true, result, kResultSlot);
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, true, Stack, kStackResultSlot);
    else
        StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
