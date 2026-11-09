#include <codegen/x86/ClzeroOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>

namespace Codegen {

using namespace X64OpcodeConstants;

namespace {

constexpr std::size_t kMaxInstructionLength = 15;

}

ClzeroOperands DecodeClzero(const std::uint8_t* data, const std::size_t length) {
    if (length > kMaxInstructionLength) {
        throw CodegenException("CLZERO longer than 15 bytes");
    }

    ClzeroOperands operands{};
    std::size_t pos = 0;

    while (pos < length) {
        const std::uint8_t b = data[pos];
        if (b >= RexMin && b <= RexMax) {
            pos += 1;
            continue;
        }
        if (b == PrefixLock) {
            throw CodegenException("CLZERO with a LOCK prefix");
        }
        if (b == PrefixSegFs || b == PrefixSegGs) {
            throw CodegenException("CLZERO with an FS or GS segment override");
        }
        if (b == PrefixOperandSize || b == PrefixRepne || b == PrefixRep) {
            throw CodegenException("Not a CLZERO instruction");
        }
        if (b == PrefixAddressSize) {
            operands.AddressSize32 = true;
        } else if (b != PrefixSegCs && b != PrefixSegSs && b != PrefixSegDs && b != PrefixSegEs) {
            break;
        }
        pos += 1;
    }

    if (pos + 3 != length || data[pos] != TwoByteOpcodeEscape || data[pos + 1] != TwoByteGrp7 || data[pos + 2] != 0xFC) {
        throw CodegenException("Not a CLZERO instruction");
    }

    return operands;
}

}
