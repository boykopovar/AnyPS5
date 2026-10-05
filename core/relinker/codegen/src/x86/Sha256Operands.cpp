#include <codegen/x86/Sha256Operands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <utility>
#include <vector>

namespace Codegen {

using namespace X64OpcodeConstants;

Sha256Operands DecodeSha256(const std::uint8_t* data, const std::size_t length) {
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
            throw CodegenException("Not a SHA-256 instruction");
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

    if (pos + 4 > length || data[pos] != TwoByteOpcodeEscape || data[pos + 1] != ThreeByteEscape38) {
        throw CodegenException("Not a SHA-256 instruction");
    }

    Sha256Operands operands{};
    switch (data[pos + 2]) {
    case 0xCB:
        operands.Operation = Sha256Operation::Rnds2;
        break;
    case 0xCC:
        operands.Operation = Sha256Operation::Msg1;
        break;
    case 0xCD:
        operands.Operation = Sha256Operation::Msg2;
        break;
    default:
        throw CodegenException("Not a SHA-256 instruction");
    }

    const std::uint8_t modrm = data[pos + 3];
    operands.Destination = static_cast<std::uint8_t>(((modrm >> ModRmRegShift) & ModRmRegMask) | (((rex & 0x4) != 0) ? 8 : 0));
    if (((modrm >> ModRmModShift) & ModRmModMask) != ModRmModRegister) {
        operands.Memory = DecodeMemoryOperand(data, length, pos + 3, rex, std::move(prefixes));
        return operands;
    }
    operands.Source = static_cast<std::uint8_t>((modrm & ModRmRmMask) | (((rex & 0x1) != 0) ? 8 : 0));
    return operands;
}

}
