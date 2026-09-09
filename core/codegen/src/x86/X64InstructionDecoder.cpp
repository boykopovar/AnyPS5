#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <cstring>

namespace Codegen {

using namespace X64OpcodeConstants;

std::size_t X64InstructionDecoder::Decode(const std::uint8_t* data, std::size_t available) const {
    std::size_t pos = 0;
    bool rexPresent = false;
    std::uint8_t rex = 0;
    bool operandSizeOverride = false;
    bool repnePrefix = false;

    while (pos < available) {
        const std::uint8_t b = data[pos];

        if (b == PrefixRepne) {
            repnePrefix = true;
            pos += 1;
            continue;
        }

        if (b == PrefixLock || b == PrefixRep ||
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
    bool vexPresent = false;
    bool evexPresent = false;
    std::uint8_t vexMap = 0;
    std::uint8_t threeByteMap = 0;
    bool threeByteEscape = false;

    if (opcode == OneByteVex2) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated after VEX2 prefix");
        }
        vexPresent = true;
        vexMap = TwoByteOpcodeEscape;
        pos += 1;
    } else if (opcode == OneByteVex3) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated after VEX3 prefix");
        }
        vexPresent = true;
        vexMap = static_cast<std::uint8_t>(data[pos] & Vex3MapMask);
        pos += 1;
        if (pos >= available) {
            throw CodegenException("Instruction truncated after VEX3 prefix");
        }
        pos += 1;
    } else if (opcode == EvexPrefix) {
        if (pos + 2 >= available) {
            throw CodegenException("Instruction truncated after EVEX prefix");
        }
        evexPresent = true;
        vexMap = static_cast<std::uint8_t>(data[pos] & EvexMapMask);
        pos += 3;
    } else if (opcode == TwoByteOpcodeEscape) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated after two-byte opcode escape");
        }
        twoByteOpcode = true;
        opcode = data[pos];
        pos += 1;
        if (opcode == ThreeByteEscape38 || opcode == ThreeByteEscape3A) {
            threeByteEscape = true;
            threeByteMap = opcode;
            if (pos >= available) {
                throw CodegenException("Instruction truncated after three-byte opcode escape");
            }
            opcode = data[pos];
            pos += 1;
        }
    }

    if (vexPresent || evexPresent) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated after VEX prefix");
        }
        opcode = data[pos];
        pos += 1;
    }

    bool hasModRm = false;
    std::size_t immediateSize = ImmSizeNone;

    if (vexPresent) {
        if (vexMap == TwoByteOpcodeEscape && opcode >= VexNoModRmMin && opcode <= VexNoModRmMax) {
            hasModRm = false;
        } else {
            hasModRm = true;
            if (vexMap == Vex3Map0F3A) {
                immediateSize = ImmSize8;
            }
        }
    } else if (evexPresent) {
        hasModRm = true;
        if (vexMap == EvexMap0F3A) {
            immediateSize = ImmSize8;
        }
    } else if (threeByteEscape) {
        hasModRm = true;
        if (threeByteMap == ThreeByteEscape3A) {
            immediateSize = ImmSize8;
        }
    } else if (!twoByteOpcode) {
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
            opcode == OneByteMovImm8 || opcode == OneByteMovImm32 ||
            opcode == OneByteImm32AluCmp || opcode == OneByteImm8Grp1 ||
            opcode == OneByteImm8AluCmp ||
            opcode == OneByteImulRm32Imm32 || opcode == OneByteImulRm32Imm8 ||
            opcode == OneByteShiftRm8By1 || opcode == OneByteShiftRmBy1 ||
            opcode == OneByteShiftRm8ByCl || opcode == OneByteShiftRmByCl ||
            opcode == OneByteTestGrp3Rm8 || opcode == OneByteTestGrp3Rm ||
            opcode == OneByteIncDecRm8 || opcode == OneByteGrp5Rm ||
            (opcode >= OneByteModRmRangeJMin && opcode <= OneByteModRmRangeJMax) ||
            (opcode >= OneByteX87Min && opcode <= OneByteX87Max)) {
            hasModRm = true;
        }

        if (opcode == OneByteImm8AluOrGrp3 || opcode == OneByteImm8AluCmp ||
            opcode == OneByteImm8AddAl || opcode == OneByteImm8OrAl ||
            opcode == OneByteImm8AdcAl || opcode == OneByteImm8SbbAl ||
            opcode == OneByteImm8AndAl || opcode == OneByteImm8SubAl ||
            opcode == OneByteImm8XorAl || opcode == OneByteImm8CmpAl ||
            opcode == OneByteGrp2Rm8Imm8 || opcode == OneByteGrp2RmImm8 ||
            (opcode >= OneByteMovImm8RegMin && opcode <= OneByteMovImm8RegMax) ||
            opcode == OneBytePushImm8 ||
            opcode == OneByteTestAlImm8) {
            immediateSize = ImmSize8;
        } else if (opcode == OneByteImm32AluCmp || opcode == OneByteMovImm32 ||
                   opcode == OneByteImulRm32Imm32 ||
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
        } else if (opcode == OneByteImm8Grp1 || opcode == OneByteImulRm32Imm8) {
            immediateSize = ImmSize8;
        }

        if (opcode >= OneByteJccRel8Min && opcode <= OneByteJccRel8Max) {
            immediateSize = ImmSize8;
        }

        if (opcode == OneByteCallRel32 || opcode == OneByteJmpRel32) {
            immediateSize = ImmSize32;
        }

        if (opcode == OneByteJmpRel8 || opcode == OneByteJrcxz ||
            (opcode >= OneByteLoop && opcode <= OneByteLoopMax) ||
            (opcode >= OneByteInOutImm8Min && opcode <= OneByteInOutImm8Max)) {
            immediateSize = ImmSize8;
        }
    } else {
        if (opcode == TwoByteExtrqInsertqImm8Imm8) {
            if (!operandSizeOverride && !repnePrefix) {
                throw CodegenException("Unsupported 0F 78 opcode without SSE4A prefix");
            }
            hasModRm = true;
            immediateSize = ImmSize16;
        } else if (opcode == TwoByteExtrqInsertqModRm) {
            if (!operandSizeOverride && !repnePrefix) {
                throw CodegenException("Unsupported 0F 79 opcode without SSE4A prefix");
            }
            hasModRm = true;
        } else if (opcode >= TwoByteJccRel32Min && opcode <= TwoByteJccRel32Max) {
            immediateSize = ImmSize32;
        } else if (opcode == TwoByteMovImm8ModRm) {
            hasModRm = true;
            immediateSize = ImmSize8;
        } else if (opcode >= TwoByteShiftImm8Min && opcode <= TwoByteShiftImm8Max) {
            hasModRm = true;
            immediateSize = ImmSize8;
        } else if (opcode == TwoByteShldImm8 || opcode == TwoByteShrdImm8 ||
                   opcode == TwoByteShufpsImm8 || opcode == TwoByteShufpdImm8 ||
                   opcode == TwoBytePextrw) {
            hasModRm = true;
            immediateSize = ImmSize8;
        } else if (opcode == TwoByteShldCl || opcode == TwoByteShrdCl) {
            hasModRm = true;
        } else if ((opcode >= TwoByteCmovRangeMin && opcode <= TwoByteCmovRangeMax) ||
                   (opcode >= TwoByteModRmRangeAMin && opcode <= TwoByteModRmRangeAMax) ||
                   (opcode >= TwoByteModRmRangeBMin && opcode <= TwoByteModRmRangeBMax) ||
                   (opcode >= TwoByteModRmRangeCMin && opcode <= TwoByteModRmRangeCMax) ||
                   (opcode >= TwoByteModRmRangeDMin && opcode <= TwoByteModRmRangeDMax) ||
                   (opcode >= TwoByteModRmRangeEMin && opcode <= TwoByteModRmRangeEMax) ||
                   (opcode >= TwoByteModRmRangeGMin && opcode <= TwoByteModRmRangeGMax) ||
                   (opcode >= TwoByteModRmRangeHMin && opcode <= TwoByteModRmRangeHMax) ||
                   (opcode >= TwoByteModRmRangeIMin && opcode <= TwoByteModRmRangeIMax) ||
                   (opcode >= TwoBytePrefetchGrpMin && opcode <= TwoBytePrefetchGrpMax) ||
                   (opcode >= TwoByteSetccMin && opcode <= TwoByteSetccMax) ||
                   opcode == TwoByteImulRmModRm ||
                   opcode == TwoByteGrp7 ||
                   opcode == TwoByteGrp15 ||
                   opcode == TwoByteXadd ||
                   opcode == TwoByteGrp9 ||
                   opcode == TwoByteNopModRm || opcode == TwoByteEndbr) {
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

    const auto mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    const auto reg = static_cast<std::uint8_t>((modrm >> ModRmRegShift) & ModRmRegMask);
    const auto rm = static_cast<std::uint8_t>(modrm & ModRmRmMask);

    if (!vexPresent && !twoByteOpcode &&
        (opcode == OneByteTestGrp3Rm8 || opcode == OneByteTestGrp3Rm) &&
        reg <= Grp3RegTestMax) {
        immediateSize = (opcode == OneByteTestGrp3Rm8)
            ? ImmSize8
            : (operandSizeOverride ? ImmSize16 : ImmSize32);
    }

    if (mod != ModRmModRegister && rm == ModRmRmSibPresent) {
        if (pos >= available) {
            throw CodegenException("Instruction truncated before SIB byte");
        }
        const std::uint8_t sib = data[pos];
        const auto base = static_cast<std::uint8_t>(sib & SibBaseMask);
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

namespace Codegen {

DecodedInstructionInfo X64InstructionDecoder::DecodeInstruction(
    const std::uint8_t* data,
    std::size_t available
) const {
    using namespace X64OpcodeConstants;

    DecodedInstructionInfo info{};
    info.Length = Decode(data, available);

    std::size_t pos = 0;

    while (pos < info.Length) {
        std::uint8_t b = data[pos];
        if (b == PrefixLock || b == PrefixRepne || b == PrefixRep ||
            b == PrefixSegCs || b == PrefixSegSs || b == PrefixSegDs ||
            b == PrefixSegEs || b == PrefixSegFs || b == PrefixSegGs ||
            b == PrefixOperandSize || b == PrefixAddressSize) {
            ++pos;
            continue;
        }
        break;
    }

    if (pos < info.Length && data[pos] >= RexMin && data[pos] <= RexMax)
        ++pos;

    if (pos >= info.Length)
        return info;

    std::uint8_t op = data[pos++];
    bool twoByteOpcode = false;

    if (op == OneByteVex2 || op == OneByteVex3) {
        info.FlowKind = ControlFlowKind::Sequential;
        return info;
    }
    if (op == EvexPrefix) {
        info.FlowKind = ControlFlowKind::Sequential;
        return info;
    }
    if (op == TwoByteOpcodeEscape && pos < info.Length) {
        twoByteOpcode = true;
        op = data[pos++];
    }

    info.Opcode = op;
    info.IsTwoByteOpcode = twoByteOpcode;

    if (!twoByteOpcode) {
        if (op == OneByteCallRel32) {
            std::int32_t disp = 0;
            std::memcpy(&disp, data + pos, 4);
            info.FlowKind = ControlFlowKind::Call;
            info.HasBranchTarget = true;
            info.BranchDisp = disp;
            return info;
        }
        if (op == OneByteJmpRel32) {
            std::int32_t disp = 0;
            std::memcpy(&disp, data + pos, 4);
            info.FlowKind = ControlFlowKind::UnconditionalJump;
            info.HasBranchTarget = true;
            info.BranchDisp = disp;
            return info;
        }
        if (op == OneByteJmpRel8) {
            info.FlowKind = ControlFlowKind::UnconditionalJump;
            info.HasBranchTarget = true;
            info.BranchDisp = static_cast<std::int8_t>(data[pos]);
            return info;
        }
        if (op >= OneByteJccRel8Min && op <= OneByteJccRel8Max) {
            info.FlowKind = ControlFlowKind::ConditionalBranch;
            info.HasBranchTarget = true;
            info.BranchDisp = static_cast<std::int8_t>(data[pos]);
            return info;
        }
        if (op == OneByteJrcxz || (op >= OneByteLoop && op <= OneByteLoopMax)) {
            info.FlowKind = ControlFlowKind::ConditionalBranch;
            info.HasBranchTarget = true;
            info.BranchDisp = static_cast<std::int8_t>(data[pos]);
            return info;
        }
        if (op == 0xC3 || op == 0xCB || op == 0xC2 || op == 0xCA) {
            info.FlowKind = ControlFlowKind::Return;
            return info;
        }
        if (op == OneByteGrp5Rm && pos < info.Length) {
            std::uint8_t modrm = data[pos];
            info.HasModRm = true;
            info.ModRmByte = modrm;
            std::uint8_t reg = (modrm >> ModRmRegShift) & ModRmRegMask;
            info.ModRmRegField = reg;
            std::uint8_t mod = (modrm >> ModRmModShift) & ModRmModMask;
            std::uint8_t rm = modrm & ModRmRmMask;
            if (reg == 2) {
                if (mod == ModRmModIndirect && rm == ModRmRmRipRelative) {
                    info.FlowKind = ControlFlowKind::Call;
                    info.HasRipRelativeDisp = true;
                    info.RipRelativeDispOffset = pos + 1;
                } else {
                    info.FlowKind = ControlFlowKind::IndirectCall;
                }
                return info;
            }
            if (reg == 4) {
                if (mod == ModRmModIndirect && rm == ModRmRmRipRelative) {
                    std::int32_t disp = 0;
                    std::memcpy(&disp, data + pos + 1, 4);
                    info.FlowKind = ControlFlowKind::UnconditionalJump;
                    info.HasRipRelativeDisp = true;
                    info.RipRelativeDispOffset = pos + 1;
                    info.HasBranchTarget = true;
                    info.BranchDisp = disp;
                } else {
                    info.FlowKind = ControlFlowKind::IndirectJump;
                }
                return info;
            }
            if (reg == 3) {
                info.FlowKind = ControlFlowKind::IndirectCall;
                return info;
            }
            if (reg == 5) {
                info.FlowKind = ControlFlowKind::IndirectJump;
                return info;
            }
        }
    } else {
        if (op == 0x0B) {
            info.FlowKind = ControlFlowKind::Trap;
            return info;
        }
        if (op >= TwoByteJccRel32Min && op <= TwoByteJccRel32Max) {
            std::int32_t disp = 0;
            std::memcpy(&disp, data + pos, 4);
            info.FlowKind = ControlFlowKind::ConditionalBranch;
            info.HasBranchTarget = true;
            info.BranchDisp = disp;
            return info;
        }
    }

    bool hasModRm = false;
    if (!twoByteOpcode) {
        if ((op >= OneByteModRmRangeAMin && op <= OneByteModRmRangeAMax) ||
            (op >= OneByteModRmRangeBMin && op <= OneByteModRmRangeBMax) ||
            (op >= OneByteModRmRangeCMin && op <= OneByteModRmRangeCMax) ||
            (op >= OneByteModRmRangeDMin && op <= OneByteModRmRangeDMax) ||
            (op >= OneByteModRmRangeEMin && op <= OneByteModRmRangeEMax) ||
            (op >= OneByteModRmRangeFMin && op <= OneByteModRmRangeFMax) ||
            (op >= OneByteModRmRangeGMin && op <= OneByteModRmRangeGMax) ||
            (op >= OneByteModRmRangeHMin && op <= OneByteModRmRangeHMax) ||
            (op >= OneByteModRmRangeIMin && op <= OneByteModRmRangeIMax) ||
            (op >= OneByteModRmRangeJMin && op <= OneByteModRmRangeJMax) ||
            op == OneByteArpl || op == OneByteMovsxd ||
            op == OneByteGrp2Rm8Imm8 || op == OneByteGrp2RmImm8 ||
            op == OneByteMovImm8 || op == OneByteMovImm32 ||
            op == OneByteImm32AluCmp || op == OneByteImm8Grp1 ||
            op == OneByteImm8AluCmp ||
            op == OneByteImulRm32Imm32 || op == OneByteImulRm32Imm8 ||
            op == OneByteShiftRm8By1 || op == OneByteShiftRmBy1 ||
            op == OneByteShiftRm8ByCl || op == OneByteShiftRmByCl ||
            op == OneByteTestGrp3Rm8 || op == OneByteTestGrp3Rm ||
            op == OneByteIncDecRm8 || op == OneByteGrp5Rm ||
            (op >= OneByteX87Min && op <= OneByteX87Max))
            hasModRm = true;
    } else {
        if ((op >= TwoByteCmovRangeMin && op <= TwoByteCmovRangeMax) ||
            (op >= TwoByteModRmRangeAMin && op <= TwoByteModRmRangeAMax) ||
            (op >= TwoByteModRmRangeBMin && op <= TwoByteModRmRangeBMax) ||
            (op >= TwoByteModRmRangeCMin && op <= TwoByteModRmRangeCMax) ||
            (op >= TwoByteModRmRangeDMin && op <= TwoByteModRmRangeDMax) ||
            (op >= TwoByteModRmRangeEMin && op <= TwoByteModRmRangeEMax) ||
            (op >= TwoByteModRmRangeGMin && op <= TwoByteModRmRangeGMax) ||
            (op >= TwoByteModRmRangeHMin && op <= TwoByteModRmRangeHMax) ||
            (op >= TwoByteModRmRangeIMin && op <= TwoByteModRmRangeIMax) ||
            (op >= TwoBytePrefetchGrpMin && op <= TwoBytePrefetchGrpMax) ||
            (op >= TwoByteSetccMin && op <= TwoByteSetccMax) ||
            op == TwoByteImulRmModRm || op == TwoByteGrp7 ||
            op == TwoByteGrp15 || op == TwoByteXadd ||
            op == TwoByteGrp9 || op == TwoByteNopModRm ||
            op == TwoByteEndbr || op == TwoByteMovImm8ModRm ||
            (op >= TwoByteShiftImm8Min && op <= TwoByteShiftImm8Max) ||
            op == TwoByteShldImm8 || op == TwoByteShrdImm8 ||
            op == TwoByteShufpsImm8 || op == TwoByteShufpdImm8 ||
            op == TwoBytePextrw || op == TwoByteShldCl || op == TwoByteShrdCl)
            hasModRm = true;
    }

    if (hasModRm && pos < info.Length) {
        std::uint8_t modrm = data[pos];
        info.HasModRm = true;
        info.ModRmByte = modrm;
        info.ModRmRegField = (modrm >> ModRmRegShift) & ModRmRegMask;
        std::uint8_t mod = (modrm >> ModRmModShift) & ModRmModMask;
        std::uint8_t rm = modrm & ModRmRmMask;
        if (mod == ModRmModIndirect && rm == ModRmRmRipRelative) {
            info.HasRipRelativeDisp = true;
            info.RipRelativeDispOffset = pos + 1;
        }
    }

    info.FlowKind = ControlFlowKind::Sequential;
    return info;
}

}
