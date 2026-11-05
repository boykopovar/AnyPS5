#include <codegen/x86/AndnLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>

namespace Codegen {

using namespace ScalarLowering;

std::optional<AndnOperands> DecodeAndn(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF2)
        return std::nullopt;
    if ((data[vex + 2] & 7) != 0)
        throw CodegenException("ANDN requires VEX.L=0 and VEX.pp=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    AndnOperands operands{
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    if ((modrm >> 6) == 3) {
        if (vex + 5 != length) throw CodegenException("ANDN register operand has trailing bytes");
    } else {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        if (vex + 4 + operands.Memory->EncodedSize != length)
            throw CodegenException("ANDN memory operand has trailing bytes");
    }
    return operands;
}

void EmitAndn(StubBodyBuilder& body, const AndnOperands& operands) {
    auto used = operands.Memory ? UsedRegisters({operands.Destination, operands.InvertedSource}, operands.Memory)
                                : UsedRegisters({operands.Destination, operands.InvertedSource, operands.Source}, std::nullopt);
    const auto scratch = Scratch(used, "ANDN");
    StackSlot(body, 0x8D, true, Stack, -FrameSize);
    StackSlot(body, 0x89, true, scratch, 0);
    if (!operands.Memory && operands.Source == Stack) {
        StackSlot(body, 0x8D, true, scratch, FrameSize);
        StackSlot(body, 0x89, true, scratch, 8);
    }
    if (operands.InvertedSource == Stack)
        StackSlot(body, 0x8D, true, scratch, FrameSize);
    else
        Register(body, 0x8B, operands.Wide, scratch, operands.InvertedSource);
    Register(body, 0xF7, operands.Wide, 2, scratch);
    if (operands.Memory)
        Memory(body, 0x23, operands.Wide, *operands.Memory, scratch, "ANDN");
    else if (operands.Source == Stack)
        StackSlot(body, 0x23, operands.Wide, scratch, 8);
    else
        Register(body, 0x23, operands.Wide, scratch, operands.Source);
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, scratch, 8);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, scratch);
    StackSlot(body, 0x8B, true, scratch, 0);
    StackSlot(body, 0x8D, true, Stack, FrameSize);
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, 8 - FrameSize);
}

}
