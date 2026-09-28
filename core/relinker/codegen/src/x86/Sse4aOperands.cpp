#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>

namespace Codegen {

using namespace X64OpcodeConstants;

Sse4aOperands DecodeSse4a(const std::uint8_t* data, const std::size_t length) {
    std::size_t pos = 0;
    bool operandSizeOverride = false;
    bool repnePrefix = false;

    while (pos < length) {
        const std::uint8_t b = data[pos];
        if (b == PrefixOperandSize) {
            operandSizeOverride = true;
        } else if (b == PrefixRepne) {
            repnePrefix = true;
        } else if (b != PrefixLock && b != PrefixRep && b != PrefixAddressSize &&
                   b != PrefixSegCs && b != PrefixSegSs && b != PrefixSegDs &&
                   b != PrefixSegEs && b != PrefixSegFs && b != PrefixSegGs) {
            break;
        }
        pos += 1;
    }

    std::uint8_t rex = 0;
    if (pos < length && data[pos] >= RexMin && data[pos] <= RexMax) {
        rex = data[pos];
        pos += 1;
    }

    if (pos + 3 > length || data[pos] != TwoByteOpcodeEscape ||
        (data[pos + 1] != TwoByteExtrqInsertqImm8Imm8 && data[pos + 1] != TwoByteExtrqInsertqModRm) ||
        (!operandSizeOverride && !repnePrefix)) {
        throw CodegenException("Not an SSE4a instruction");
    }

    const std::uint8_t opcode = data[pos + 1];
    const std::uint8_t modrm = data[pos + 2];
    pos += 3;

    if (((modrm >> ModRmModShift) & ModRmModMask) != ModRmModRegister) {
        throw CodegenException("SSE4a instruction with a memory operand");
    }

    const auto regField = static_cast<std::uint8_t>((modrm >> ModRmRegShift) & ModRmRegMask);
    const auto reg = static_cast<std::uint8_t>(regField | (((rex & 0x4) != 0) ? 8 : 0));
    const auto rm = static_cast<std::uint8_t>((modrm & ModRmRmMask) | (((rex & 0x1) != 0) ? 8 : 0));

    Sse4aOperands operands{};
    operands.Insertq = repnePrefix;
    operands.RegisterForm = opcode == TwoByteExtrqInsertqModRm;
    operands.Destination = reg;
    operands.Source = rm;

    if (operands.RegisterForm) {
        return operands;
    }

    if (pos + 2 > length) {
        throw CodegenException("SSE4a instruction truncated before its immediates");
    }

    if (!operands.Insertq) {
        if (regField != 0) {
            throw CodegenException("EXTRQ immediate form with a non-zero reg field");
        }
        operands.Destination = rm;
    }

    const auto rawLength = static_cast<std::uint8_t>(data[pos] & 0x3F);
    operands.Length = rawLength == 0 ? 64 : rawLength;
    operands.Index = static_cast<std::uint8_t>(data[pos + 1] & 0x3F);

    if (static_cast<unsigned>(operands.Length) + operands.Index > 64) {
        throw CodegenException("SSE4a field exceeds 64 bits");
    }

    return operands;
}

}
