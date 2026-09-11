#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>

namespace Codegen {

using namespace X64OpcodeConstants;

std::size_t DecodedInstruction::_skipPrefixesAndRex(
    bool* outHasOperandSizePrefix,
    bool* outHasRepnePrefix,
    bool* outHasRepPrefix
) const {
    std::size_t pos = 0;
    *outHasOperandSizePrefix = false;
    *outHasRepnePrefix = false;
    *outHasRepPrefix = false;

    while (pos < Length) {
        const std::uint8_t b = Data[pos];

        if (b == PrefixRepne) {
            *outHasRepnePrefix = true;
            pos += 1;
            continue;
        }

        if (b == PrefixRep) {
            *outHasRepPrefix = true;
            pos += 1;
            continue;
        }

        if (b == PrefixLock ||
            b == PrefixSegCs || b == PrefixSegSs || b == PrefixSegDs || b == PrefixSegEs ||
            b == PrefixSegFs || b == PrefixSegGs) {
            pos += 1;
            continue;
        }

        if (b == PrefixOperandSize) {
            *outHasOperandSizePrefix = true;
            pos += 1;
            continue;
        }

        if (b == PrefixAddressSize) {
            pos += 1;
            continue;
        }

        break;
    }

    if (pos < Length && Data[pos] >= RexMin && Data[pos] <= RexMax) {
        pos += 1;
    }

    return pos;
}

bool DecodedInstruction::IsShaNi() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    if (Data[pos] != TwoByteOpcodeEscape) {
        return false;
    }

    if (Data[pos + 1] == ThreeByteEscape38) {
        const std::uint8_t opcode = Data[pos + 2];
        return opcode == 0xC8 || opcode == 0xC9 || opcode == 0xCA ||
               opcode == 0xCC || opcode == 0xCD;
    }

    if (Data[pos + 1] == ThreeByteEscape3A) {
        const std::uint8_t opcode = Data[pos + 2];
        return opcode == 0xCC;
    }

    return false;
}

bool DecodedInstruction::IsExtrq() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (!operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 1 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape &&
           (Data[pos + 1] == TwoByteExtrqInsertqImm8Imm8 || Data[pos + 1] == TwoByteExtrqInsertqModRm);
}

bool DecodedInstruction::IsInsertq() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || !repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 1 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape &&
           (Data[pos + 1] == TwoByteExtrqInsertqImm8Imm8 || Data[pos + 1] == TwoByteExtrqInsertqModRm);
}

bool DecodedInstruction::IsMonitorx() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == TwoByteGrp7 && Data[pos + 2] == 0xFA;
}

bool DecodedInstruction::IsMwaitx() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == TwoByteGrp7 && Data[pos + 2] == 0xFB;
}

bool DecodedInstruction::IsClzero() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == TwoByteGrp7 && Data[pos + 2] == 0xFC;
}

bool DecodedInstruction::IsRdpru() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == TwoByteGrp7 && Data[pos + 2] == 0xFD;
}

bool DecodedInstruction::IsMcommit() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || !repPrefix) {
        return false;
    }

    if (pos + 2 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == TwoByteGrp7 && Data[pos + 2] == 0xFA;
}

bool DecodedInstruction::IsMovntss() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || repnePrefix || !repPrefix) {
        return false;
    }

    if (pos + 1 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == 0x2B;
}

bool DecodedInstruction::IsMovntsd() const {
    bool operandSizeOverride = false;
    bool repnePrefix = false;
    bool repPrefix = false;
    const std::size_t pos = _skipPrefixesAndRex(&operandSizeOverride, &repnePrefix, &repPrefix);

    if (operandSizeOverride || !repnePrefix || repPrefix) {
        return false;
    }

    if (pos + 1 >= Length) {
        return false;
    }

    return Data[pos] == TwoByteOpcodeEscape && Data[pos + 1] == 0x2B;
}

}
