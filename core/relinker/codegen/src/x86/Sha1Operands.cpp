#include <codegen/x86/Sha1Operands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <utility>
#include <vector>

namespace Codegen {

using namespace X64OpcodeConstants;

Sha1Operands DecodeSha1(const std::uint8_t* data, const std::size_t length) {
    std::size_t pos = 0;
    std::uint8_t rex = 0;
    std::vector<std::uint8_t> prefixes;

    while (pos < length) {
        const std::uint8_t b = data[pos];
        if (b >= RexMin && b <= RexMax) {
            rex = b;
            pos += 1;
            continue;
        }
        if (b == PrefixOperandSize || b == PrefixRepne || b == PrefixRep) {
            throw CodegenException("Not a SHA-1 instruction");
        }
        if (b != PrefixLock && b != PrefixAddressSize &&
            b != PrefixSegCs && b != PrefixSegSs && b != PrefixSegDs &&
            b != PrefixSegEs && b != PrefixSegFs && b != PrefixSegGs) {
            break;
        }
        if (b != PrefixLock)
            prefixes.push_back(b);
        rex = 0;
        pos += 1;
    }

    if (pos + 4 > length || data[pos] != TwoByteOpcodeEscape) {
        throw CodegenException("Not a SHA-1 instruction");
    }

    Sha1Operands operands{};
    if (data[pos + 1] == ThreeByteEscape3A && data[pos + 2] == 0xCC) {
        operands.Operation = Sha1Operation::Rnds4;
    } else if (data[pos + 1] == ThreeByteEscape38 && data[pos + 2] == 0xC8) {
        operands.Operation = Sha1Operation::Nexte;
    } else if (data[pos + 1] == ThreeByteEscape38 && data[pos + 2] == 0xC9) {
        operands.Operation = Sha1Operation::Msg1;
    } else if (data[pos + 1] == ThreeByteEscape38 && data[pos + 2] == 0xCA) {
        operands.Operation = Sha1Operation::Msg2;
    } else {
        throw CodegenException("Not a SHA-1 instruction");
    }

    const std::size_t modRmOffset = pos + 3;
    const std::uint8_t modrm = data[modRmOffset];
    std::size_t immediateOffset = modRmOffset + 1;
    operands.Destination = static_cast<std::uint8_t>(((modrm >> ModRmRegShift) & ModRmRegMask) | (((rex & 0x4) != 0) ? 8 : 0));
    if (((modrm >> ModRmModShift) & ModRmModMask) != ModRmModRegister) {
        operands.Memory = DecodeMemoryOperand(data, length, modRmOffset, rex, std::move(prefixes));
        immediateOffset = modRmOffset + operands.Memory->EncodedSize;
    } else {
        operands.Source = static_cast<std::uint8_t>((modrm & ModRmRmMask) | (((rex & 0x1) != 0) ? 8 : 0));
    }
    if (operands.Operation == Sha1Operation::Rnds4) {
        if (immediateOffset >= length) {
            throw CodegenException("SHA1RNDS4 truncated before its immediate");
        }
        operands.Function = static_cast<std::uint8_t>(data[immediateOffset] & 3);
    }
    if (operands.Memory && operands.Memory->RipRelative && immediateOffset + (operands.Operation == Sha1Operation::Rnds4 ? 1 : 0) != length)
        throw CodegenException("RIP-relative SHA-1 operand does not end the instruction");
    return operands;
}

}
