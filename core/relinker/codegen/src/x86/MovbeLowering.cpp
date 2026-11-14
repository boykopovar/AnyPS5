#include <codegen/x86/MovbeLowering.hpp>
#include <codegen/x86/ScalarPrefixes.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::int32_t kValueSlot = 0;
constexpr std::int32_t kSwapSlot = 8;
constexpr std::int32_t kStackResultSlot = 16;
constexpr std::int32_t kFrame = FrameSize + 16;
constexpr const char* kName = "MOVBE";

void _swap(StubBodyBuilder& body, const std::uint8_t width, const std::uint8_t value) {
    ByteSwap(body, width == 64, value);
    if (width != 16) return;
    StackSlot(body, 0x89, false, value, kSwapSlot);
    TwoByteStackSlot(body, 0xB7, value, kSwapSlot + 2);
}

void _writeStackPointer(StubBodyBuilder& body, const std::uint8_t width, const std::uint8_t value) {
    if (width != 16) {
        StackSlot(body, 0x89, true, value, kStackResultSlot);
        return;
    }
    StackSlot(body, 0x89, false, value, kSwapSlot);
    StackSlot(body, 0x8D, true, value, kFrame);
    StackSlot(body, 0x89, true, value, kStackResultSlot);
    TwoByteStackSlot(body, 0xB7, value, kSwapSlot);
    SizedStackSlot(body, 0x89, 16, value, kStackResultSlot);
}

}

std::optional<MovbeOperands> DecodeMovbe(const std::uint8_t* data, const std::size_t length) {
    const auto encoding = ReadLegacyEncoding(data, length);
    const auto opcode = encoding.Opcode;
    if (encoding.Repeat || encoding.RepeatNot || opcode + 4 > length || data[opcode] != 0x0F || data[opcode + 1] != 0x38 || (data[opcode + 2] != 0xF0 && data[opcode + 2] != 0xF1))
        return std::nullopt;
    const auto modrm = data[opcode + 3];
    if ((modrm >> 6) == 3) throw CodegenException("MOVBE requires a memory operand");
    MovbeOperands operands{
        data[opcode + 2] == 0xF1,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((encoding.Rex & 4) << 1)),
        static_cast<std::uint8_t>((encoding.Rex & 8) != 0 ? 64 : encoding.OperandSize ? 16 : 32),
        DecodeMemoryOperand(data, length, opcode + 3, static_cast<std::uint8_t>(encoding.Rex & 3), encoding.Segment)};
    if (opcode + 3 + operands.Memory.EncodedSize != length) throw CodegenException("MOVBE has trailing bytes");
    return operands;
}

void EmitMovbe(StubBodyBuilder& body, const MovbeOperands& operands) {
    auto used = UsedRegisters({operands.Register}, operands.Memory);
    const auto value = Scratch(used, kName);
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    StackSlot(body, 0x89, true, value, kValueSlot);
    if (operands.Store) {
        if (operands.Register == Stack)
            StackSlot(body, 0x8D, true, value, kFrame);
        else
            Register(body, 0x8B, operands.Width == 64, value, operands.Register);
        _swap(body, operands.Width, value);
        SizedMemory(body, 0x89, operands.Width, operands.Memory, value, kName, kFrame);
    } else {
        SizedMemory(body, 0x8B, operands.Width, operands.Memory, value, kName, kFrame);
        _swap(body, operands.Width, value);
        if (operands.Register == Stack)
            _writeStackPointer(body, operands.Width, value);
        else
            SizedRegister(body, 0x8B, operands.Width, operands.Register, value);
    }
    StackSlot(body, 0x8B, true, value, kValueSlot);
    if (!operands.Store && operands.Register == Stack)
        StackSlot(body, 0x8B, true, Stack, kStackResultSlot);
    else
        StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
