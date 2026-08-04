#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>

namespace Codegen {

using namespace X64OpcodeConstants;

std::size_t X64InstructionDecoder::Decode(const std::uint8_t* data, std::size_t available) const {
    std::size_t pos = 0;
    bool rexPresent = false;
    std::uint8_t rex = 0;
    bool operandSizeOverride = false;

    while (pos < available) {
        const std::uint8_t b = data[pos];

        if (b == PrefixLock || b == PrefixRepne || b == PrefixRep ||
            b == PrefixSegCs || b == PrefixSegSs || b == PrefixSegDs || b == PrefixSegEs ||
            b == PrefixSegFs || b == PrefixSegGs) {
            pos += 1;
            continue;
        }

        if (b == PrefixOperandSize) {
            operandSizeOverride = true;
            pos += 1;
            continue;
        }

        if (b == PrefixAddressSize) {
            pos += 1;
            continue;
        }

        break;
    }

    if (pos < available && data[pos] >= RexMin && data[pos] <= RexMax) {
        rexPresent = true;
        rex = data[pos];
        pos += 1;
    }

    if (pos >= available) {
        throw CodegenException("Instruction truncated after prefixes");
    }

    std::uint8_t opcode = data[pos];
    pos += 1;
    bool twoByteOpcode = false;

    if (opcode == TwoByteOpcodeEscape) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated after two-byte opcode escape");
        }
        twoByteOpcode = true;
        opcode = data[pos];
        pos += 1;
    }

    bool hasModRm = false;
    std::size_t immediateSize = ImmSizeNone;

    if (!twoByteOpcode) {
        if ((opcode >= OneByteModRmRangeAMin && opcode <= OneByteModRmRangeAMax) ||
            (opcode >= OneByteModRmRangeBMin && opcode <= OneByteModRmRangeBMax) ||
            (opcode >= OneByteModRmRangeCMin && opcode <= OneByteModRmRangeCMax) ||
            (opcode >= OneByteModRmRangeDMin && opcode <= OneByteModRmRangeDMax) ||
            (opcode >= OneByteModRmRangeEMin && opcode <= OneByteModRmRangeEMax) ||
            (opcode >= OneByteModRmRangeFMin && opcode <= OneByteModRmRangeFMax) ||
            (opcode >= OneByteModRmRangeGMin && opcode <= OneByteModRmRangeGMax) ||
            (opcode >= OneByteModRmRangeHMin && opcode <= OneByteModRmRangeHMax) ||
            opcode == OneByteArpl || opcode == OneByteMovsxd ||
            (opcode >= OneByteModRmRangeIMin && opcode <= OneByteModRmRangeIMax) ||
            opcode == OneByteGrp2Rm8Imm8 || opcode == OneByteGrp2RmImm8 ||
            opcode == OneByteVex3 || opcode == OneByteVex2 ||
            opcode == OneByteMovImm8 || opcode == OneByteMovImm32 ||
            opcode == OneByteShiftRm8By1 || opcode == OneByteShiftRmBy1 ||
            opcode == OneByteShiftRm8ByCl || opcode == OneByteShiftRmByCl ||
            opcode == OneByteTestGrp3Rm8 || opcode == OneByteTestGrp3Rm ||
            opcode == OneByteIncDecRm8 || opcode == OneByteGrp5Rm ||
            (opcode >= OneByteModRmRangeJMin && opcode <= OneByteModRmRangeJMax)) {
            hasModRm = true;
        }

        if (opcode == OneByteImm8AluOrGrp3 || opcode == OneByteImm8AluCmp ||
            opcode == OneByteImm8AddAl || opcode == OneByteImm8OrAl ||
            opcode == OneByteImm8AdcAl || opcode == OneByteImm8SbbAl ||
            opcode == OneByteImm8AndAl || opcode == OneByteImm8SubAl ||
            opcode == OneByteImm8XorAl || opcode == OneByteImm8CmpAl ||
            (opcode >= OneByteMovImm8RegMin && opcode <= OneByteMovImm8RegMax) ||
            opcode == OneBytePushImm8 ||
            opcode == OneByteTestAlImm8) {
            immediateSize = ImmSize8;
        } else if (opcode == OneByteImm32AluCmp ||
                   opcode == OneByteImm32AddEax || opcode == OneByteImm32OrEax ||
                   opcode == OneByteImm32AdcEax || opcode == OneByteImm32SbbEax ||
                   opcode == OneByteImm32AndEax || opcode == OneByteImm32SubEax ||
                   opcode == OneByteImm32XorEax || opcode == OneByteImm32CmpEax ||
                   opcode == OneBytePushImm32 ||
                   opcode == OneByteTestEaxImm32 ||
                   (opcode >= OneByteMovImm32RegMin && opcode <= OneByteMovImm32RegMax)) {
            if (opcode >= OneByteMovImm32RegMin && opcode <= OneByteMovImm32RegMax) {
                immediateSize = (rexPresent && (rex & RexWBit) != 0) ? ImmSize64 :
                    (operandSizeOverride ? ImmSize16 : ImmSize32);
            } else {
                immediateSize = operandSizeOverride ? ImmSize16 : ImmSize32;
            }
        } else if (opcode == OneByteImm8Grp1) {
            immediateSize = ImmSize8;
        }

        if (opcode >= OneByteJccRel8Min && opcode <= OneByteJccRel8Max) {
            immediateSize = ImmSize8;
        }

        if (opcode == OneByteCallRel32 || opcode == OneByteJmpRel32) {
            immediateSize = ImmSize32;
        }

        if (opcode == OneByteJmpRel8) {
            immediateSize = ImmSize8;
        }
    } else {
        if (opcode >= TwoByteJccRel32Min && opcode <= TwoByteJccRel32Max) {
            immediateSize = ImmSize32;
        } else if (opcode == TwoByteMovImm8ModRm) {
            hasModRm = true;
            immediateSize = ImmSize8;
        } else if (opcode == TwoByteStringOpMovs || opcode == TwoByteStringOpCmps) {
            immediateSize = ImmSize8;
        } else if ((opcode >= TwoByteCmovRangeMin && opcode <= TwoByteCmovRangeMax) ||
                   (opcode >= TwoByteModRmRangeAMin && opcode <= TwoByteModRmRangeAMax) ||
                   (opcode >= TwoByteModRmRangeBMin && opcode <= TwoByteModRmRangeBMax) ||
                   (opcode >= TwoByteModRmRangeCMin && opcode <= TwoByteModRmRangeCMax) ||
                   (opcode >= TwoByteModRmRangeDMin && opcode <= TwoByteModRmRangeDMax) ||
                   (opcode >= TwoByteModRmRangeEMin && opcode <= TwoByteModRmRangeEMax) ||
                   (opcode >= TwoByteModRmRangeFMin && opcode <= TwoByteModRmRangeFMax) ||
                   (opcode >= TwoByteModRmRangeGMin && opcode <= TwoByteModRmRangeGMax) ||
                   (opcode >= TwoByteModRmRangeHMin && opcode <= TwoByteModRmRangeHMax) ||
                   opcode == TwoByteNopModRm) {
            hasModRm = true;
        }
    }

    if (!hasModRm) {
        if (pos + immediateSize > available) {
            throw CodegenException("Instruction truncated in immediate operand");
        }
        return pos + immediateSize;
    }

    if (pos >= available) {
        throw CodegenException("Instruction truncated before ModRM byte");
    }

    const std::uint8_t modrm = data[pos];
    pos += 1;

    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & ModRmRmMask);

    if (mod != ModRmModRegister && rm == ModRmRmSibPresent) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated before SIB byte");
        }
        const std::uint8_t sib = data[pos];
        const std::uint8_t base = static_cast<std::uint8_t>(sib & SibBaseMask);
        pos += 1;

        if (mod == ModRmModIndirect && base == SibBaseDisp32) {
            pos += Disp32Size;
        }
    }

    if (mod == ModRmModDisp8) {
        pos += Disp8Size;
    } else if (mod == ModRmModDisp32) {
        pos += Disp32Size;
    } else if (mod == ModRmModIndirect && rm == ModRmRmRipRelative) {
        pos += Disp32Size;
    }

    pos += immediateSize;

    if (pos > available) {
        throw CodegenException("Instruction truncated in displacement or immediate operand");
    }

    return pos;
}

}
