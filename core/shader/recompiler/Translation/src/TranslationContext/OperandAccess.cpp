#include "Translation/TranslationContext.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

DppMoveFlags dppFlags(const RdnaOperand& operand) {
    const auto control = operand.dpp8 ? DppMoveFlags::Lanes8 | operand.dppCtrl : operand.dppCtrl;
    return {control, static_cast<std::uint8_t>(operand.dppRowMask), static_cast<std::uint8_t>(operand.dppBankMask), operand.dppFetchInactive, operand.dppBoundCtrl};
}

}

const RdnaOperand& TranslationContext::sourceAt(const RdnaInstruction& inst, std::uint32_t index) {
    switch (index) {
        case 0u: return inst.source0;
        case 1u: return inst.source1;
        case 2u: return inst.source2;
        case 3u: return inst.source3;
        default: throw std::runtime_error("TranslationContext::sourceAt decoded source operand index is out of range");
    }
}

RdnaOperand TranslationContext::destinationOperand(const RdnaInstruction& inst) {
    RdnaOperand destination = inst.destination;
    if (destination.kind != RdnaOperandKind::VectorRegister) {
        return destination;
    }
    for (std::uint32_t index = 0u; index < std::min(inst.sourceCount, 3u); index++) {
        const RdnaOperand& source = sourceAt(inst, index);
        if (!source.dpp) {
            continue;
        }
        destination.dpp = true;
        destination.dpp8 = source.dpp8;
        destination.dppCtrl = source.dppCtrl;
        destination.dppRowMask = source.dppRowMask;
        destination.dppBankMask = source.dppBankMask;
        destination.dppFetchInactive = source.dppFetchInactive;
        destination.dppBoundCtrl = source.dppBoundCtrl;
        break;
    }
    return destination;
}

RdnaOperand TranslationContext::accumulatorOperand(const RdnaInstruction& inst) {
    RdnaOperand accumulator = inst.destination;
    accumulator.dpp = false;
    return accumulator;
}

RdnaOperand TranslationContext::offsetOperand(const RdnaOperand& operand, std::uint32_t offset) {
    if (offset == 0u) {
        return operand;
    }
    RdnaOperand result = plainOperand(operand);
    switch (result.kind) {
        case RdnaOperandKind::ScalarRegister:
        case RdnaOperandKind::VectorRegister: result.reg += offset; break;
        case RdnaOperandKind::VccLo:
            if (offset != 1u) {
                throw std::runtime_error("TranslationContext::offsetOperand special-register operand offset is out of range");
            }
            result.kind = RdnaOperandKind::VccHi;
            break;
        case RdnaOperandKind::ExecLo:
            if (offset != 1u) {
                throw std::runtime_error("TranslationContext::offsetOperand special-register operand offset is out of range");
            }
            result.kind = RdnaOperandKind::ExecHi;
            break;
        case RdnaOperandKind::VccHi:
        case RdnaOperandKind::ExecHi: throw std::runtime_error("TranslationContext::offsetOperand special-register operand offset is out of range");
        default: return operand;
    }
    return result;
}

RdnaOperand TranslationContext::scalarDestinationOperand(const RdnaOperand& operand, std::uint32_t offset) {
    std::uint32_t code = 0u;
    switch (operand.kind) {
        case RdnaOperandKind::ScalarRegister: code = operand.reg; break;
        case RdnaOperandKind::VccLo: code = 106u; break;
        case RdnaOperandKind::VccHi: code = 107u; break;
        default: throw std::runtime_error("TranslationContext::scalarDestinationOperand invalid scalar-memory destination");
    }
    code += offset;
    RdnaOperand result{};
    if (code < NumScalarRegs) {
        result.kind = RdnaOperandKind::ScalarRegister;
        result.reg = code;
    } else {
        switch (code) {
            case 106u: result.kind = RdnaOperandKind::VccLo; break;
            case 107u: result.kind = RdnaOperandKind::VccHi; break;
            default: throw std::runtime_error("TranslationContext::scalarDestinationOperand scalar-memory destination crosses an invalid register");
        }
    }
    return result;
}

RdnaOperand TranslationContext::plainOperand(const RdnaOperand& operand) {
    RdnaOperand result = operand;
    result.sdwaSel = 6u;
    result.sdwaDstUnused = 2u;
    result.omod = 0u;
    result.sdwaSext = false;
    result.opSel = false;
    result.opSelHi = false;
    result.negate = false;
    result.negateHi = false;
    result.absolute = false;
    result.clamp = false;
    result.dppCtrl = 0u;
    result.dppRowMask = 0xfu;
    result.dppBankMask = 0xfu;
    result.explicitSdwaDst = false;
    result.dppFetchInactive = false;
    result.dppBoundCtrl = false;
    result.dpp = false;
    result.dpp8 = false;
    return result;
}

IrU32 TranslationContext::flushF32Denormal(IrU32 bits) {
    if ((f32DenormalFlush & 1u) == 0u) return bits;
    const IrU1 denormal(ir.IEqual(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7f800000u)), ir.Constant(0u)));
    return IrU32(ir.Select(denormal.Value(), ir.BitwiseAnd(bits.Value(), ir.Constant(0x80000000u)), bits.Value()));
}

IrF32 TranslationContext::flushTinyProduct(IrValue* lhs, IrValue* rhs, IrValue* product, IrValue* addend) {
    if ((f32DenormalFlush & 2u) == 0u) return IrF32(*product);
    IrValue& lhsBits = ir.BitCastU32(*lhs);
    IrValue& rhsBits = ir.BitCastU32(*rhs);
    const auto exponent = [&](IrValue& bits) -> IrValue& { return ir.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&bits, &ir.Constant(23u), &ir.Constant(8u)}); };
    const auto significand = [&](IrValue& bits) -> IrValue& { return ir.BitwiseOr(ir.BitwiseAnd(bits, ir.Constant(0x007fffffu)), ir.Constant(0x00800000u)); };
    IrValue& lhsExponent = exponent(lhsBits);
    IrValue& rhsExponent = exponent(rhsBits);
    const auto finiteNonZero = [&](IrValue& value) -> IrValue& { return ir.LogicalAnd(ir.INotEqual(value, ir.Constant(0u)), ir.INotEqual(value, ir.Constant(0xffu))); };
    IrValue& carry = ir.Select(ir.UGreaterThan(ir.Emit(IrOpcode::UMulHi, IrType::U32, {&significand(lhsBits), &significand(rhsBits)}), ir.Constant(0x7fffu)), ir.Constant(1u), ir.Constant(0u));
    IrValue* tiny = &ir.LogicalAnd(ir.LogicalAnd(finiteNonZero(lhsExponent), finiteNonZero(rhsExponent)), ir.ULessThan(ir.IAdd(ir.IAdd(lhsExponent, rhsExponent), carry), ir.Constant(128u)));
    if (addend != nullptr) tiny = &ir.LogicalAnd(*tiny, ir.IEqual(ir.BitwiseAnd(ir.BitCastU32(*addend), ir.Constant(0x7fffffffu)), ir.Constant(0u)));
    IrValue& sign = ir.BitwiseAnd(ir.BitwiseXor(lhsBits, rhsBits), ir.Constant(0x80000000u));
    return IrF32(ir.Emit(IrOpcode::SelectF32, IrType::F32, {tiny, &ir.BitCastF32(sign), product}));
}

IrValue* TranslationContext::readOperand(const RdnaOperand& operand, IrType type) {
    if (type == IrType::U16) {
        return &ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&applyBitSourceModifiers(operand, readRawU32(operand)).Value()});
    }
    if (type == IrType::F16) {
        const IrU16 bits(ir.Emit(IrOpcode::ConvertU16U32, IrType::U16, {&readF16SourceBits(operand).Value()}));
        return &ir.Emit(IrOpcode::BitCastF16U16, IrType::F16, {&bits.Value()});
    }
    if (type == IrType::U1) {
        switch (operand.kind) {
            case RdnaOperandKind::Scc: return &ir.GetScc();
            case RdnaOperandKind::ExecLo:
            case RdnaOperandKind::ExecHi: return &ir.GetExec();
            case RdnaOperandKind::VccLo:
            case RdnaOperandKind::VccHi: return &ir.GetVcc();
            case RdnaOperandKind::VccZ: return &ir.LogicalNot(ir.GetVcc());
            case RdnaOperandKind::ExecZ: return &ir.LogicalNot(ir.GetExec());
            default: break;
        }
        return &ir.INotEqual(readRawU32(operand).Value(), ir.Constant(0u));
    }
    if (type == IrType::U64) {
        const std::array<IrU32, 2> pair = readU32Pair(operand);
        return &ir.ConstructU64(pair[0].Value(), pair[1].Value());
    }
    IrU32 bits = applyBitSourceModifiers(operand, readRawU32(operand));
    if (operand.absolute) {
        bits = IrU32(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffffffu)));
    }
    if (operand.negate) {
        bits = IrU32(ir.BitwiseXor(bits.Value(), ir.Constant(0x80000000u)));
    }
    if (TypesOverlap(type, IrType::F32) && !TypesOverlap(type, IrType::U32)) {
        return &ir.BitCastF32(flushF32Denormal(bits).Value());
    }
    if (!TypesOverlap(type, IrType::U32)) {
        throw std::runtime_error("TranslationContext::readOperand requested unsupported operand type");
    }
    return &bits.Value();
}

void TranslationContext::writeOperand(const RdnaOperand& operand, IrValue* value) {
    if (operand.kind == RdnaOperandKind::Null) {
        return;
    }
    IrType type = value->Type();
    if (type == IrType::F32) {
        if ((f32DenormalFlush & 2u) != 0u) value = &ir.BitCastF32(flushF32Denormal(IrU32(ir.BitCastU32(*value))).Value());
        value = &applyF32ResultModifiers(operand, IrF32(*value)).Value();
        type = IrType::F32;
    }
    if (type == IrType::Opaque) {
        throw std::runtime_error("TranslationContext::writeOperand opcode produced an untyped value");
    }
    if (type == IrType::U1) {
        switch (operand.kind) {
            case RdnaOperandKind::Scc: ir.SetScc(*value); return;
            case RdnaOperandKind::ExecLo:
            case RdnaOperandKind::ExecHi: {
                const std::array<IrU32, 2> mask = ballotMask(IrU1(*value));
                ir.SetExec(*value);
                ir.SetExecLo(mask[0].Value());
                ir.SetExecHi(mask[1].Value());
                return;
            }
            case RdnaOperandKind::VccLo:
            case RdnaOperandKind::VccHi: {
                const std::array<IrU32, 2> mask = ballotMask(IrU1(*value));
                ir.SetVcc(*value);
                ir.SetVccLo(mask[0].Value());
                ir.SetVccHi(mask[1].Value());
                return;
            }
            default:
                writeRawU32(operand, IrU32(ir.Select(*value, ir.Constant(1u), ir.Constant(0u))));
                return;
        }
    }
    if (type == IrType::U16) {
        write16Bits(operand, IrU32(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {value})));
        return;
    }
    if (type == IrType::F16) {
        const IrU16 bits(ir.Emit(IrOpcode::BitCastU16F16, IrType::U16, {value}));
        write16Bits(operand, IrU32(ir.Emit(IrOpcode::ConvertU32U16, IrType::U32, {&bits.Value()})));
        return;
    }
    if (type == IrType::U64) {
        writeU32Pair(operand, {IrU32(ir.CompositeExtract(*value, 0u)), IrU32(ir.CompositeExtract(*value, 1u))});
        return;
    }
    if (type == IrType::F32) {
        writeRawU32(operand, IrU32(ir.BitCastU32(*value)));
        return;
    }
    if (type != IrType::U32) {
        throw std::runtime_error("TranslationContext::writeOperand unsupported result type");
    }
    writeRawU32(operand, IrU32(*value));
}

IrU32 TranslationContext::applyBitSourceModifiers(const RdnaOperand& operand, IrU32 value) {
    if (operand.dpp) {
        const auto flags = dppFlags(operand);
        value = IrU32(ir.Emit(IrOpcode::DppMoveU32, IrType::U32, {&value.Value(), &ir.GetExec()}, flags));
    }
    if (operand.sdwaSel != 6u) {
        std::uint32_t offset = 0u;
        std::uint32_t width = 0u;
        if (operand.sdwaSel <= 3u) {
            offset = operand.sdwaSel * 8u;
            width = 8u;
        } else if (operand.sdwaSel == 4u || operand.sdwaSel == 5u) {
            offset = operand.sdwaSel == 5u ? 16u : 0u;
            width = 16u;
        } else {
            throw std::runtime_error("TranslationContext::applyBitSourceModifiers invalid SDWA source selector");
        }
        const IrOpcode opcode = operand.sdwaSext ? IrOpcode::BitFieldSExtract : IrOpcode::BitFieldUExtract;
        value = IrU32(ir.Emit(opcode, IrType::U32, {&value.Value(), &ir.Constant(offset), &ir.Constant(width)}));
    }
    return value;
}

bool TranslationContext::outputModifierApplies(std::uint32_t denormalShift) const {
    const std::uint32_t mode = floatMode.has_value() ? floatMode->floatMode : 0xc0u;
    return !ieeeMode && ((mode >> denormalShift) & 2u) == 0u;
}

void TranslationContext::rejectHalfOrDoubleOutputModifier(const RdnaOperand& operand) const {
    if (operand.omod != 0u && outputModifierApplies(6u)) {
        throw std::runtime_error("output modifier on an f16 or f64 result with f16/f64 output denormals flushed is not implemented");
    }
}

IrU32 TranslationContext::quietNan32(IrU32 bits) {
    return ieeeMode ? IrU32(ir.BitwiseOr(bits.Value(), ir.Constant(0x00400000u))) : bits;
}

IrU32 TranslationContext::quietNan16(IrU32 bits) {
    return ieeeMode ? IrU32(ir.BitwiseOr(bits.Value(), ir.Constant(0x0200u))) : bits;
}

IrF32 TranslationContext::applyF32ResultModifiers(const RdnaOperand& operand, IrF32 value) {
    if (operand.omod != 0u && outputModifierApplies(4u)) {
        IrValue& bits = ir.BitCastU32(value.Value());
        IrValue& magnitude = ir.BitwiseAnd(bits, ir.Constant(0x7fffffffu));
        IrValue& sign = ir.BitwiseAnd(bits, ir.Constant(0x80000000u));
        IrValue* result = nullptr;
        if (operand.omod == 3u) {
            result = &ir.Select(ir.ULessThan(magnitude, ir.Constant(0x01000000u)), sign, ir.ISub(bits, ir.Constant(0x00800000u)));
        } else {
            const std::uint32_t exponentStep = operand.omod == 1u ? 0x00800000u : 0x01000000u;
            result = &ir.Select(ir.ULessThan(magnitude, ir.Constant(0x7f800000u - exponentStep)), ir.IAdd(bits, ir.Constant(exponentStep)), ir.BitwiseOr(sign, ir.Constant(0x7f800000u)));
        }
        result = &ir.Select(ir.ULessThan(magnitude, ir.Constant(0x00800000u)), ir.Constant(0u), *result);
        result = &ir.Select(ir.UGreaterThan(magnitude, ir.Constant(0x7f7fffffu)), bits, *result);
        value = IrF32(ir.BitCastF32(*result));
    }
    if (operand.clamp) {
        const IrF32 saturated(ir.Emit(IrOpcode::FPSaturate32, IrType::F32, {&value.Value()}));
        value = dx10Clamp() ? saturated : selectF32(IrU1(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&value.Value()})), value, saturated);
    }
    return value;
}

IrF32 TranslationContext::applyF16ResultModifiers(const RdnaOperand& operand, IrF32 value) {
    rejectHalfOrDoubleOutputModifier(operand);
    if (!operand.clamp) {
        return value;
    }
    const IrF32 zero(ir.ConstantF32(0.0f));
    const IrU1 positive(ir.Emit(IrOpcode::FPOrdGreaterThan32, IrType::U1, {&value.Value(), &zero.Value()}));
    const IrF32 limited(ir.Emit(IrOpcode::FPMin32, IrType::F32, {&value.Value(), &ir.ConstantF32(1.0f)}));
    const IrF32 clamped = selectF32(positive, limited, zero);
    return dx10Clamp() ? clamped : selectF32(IrU1(ir.Emit(IrOpcode::FPIsNan32, IrType::U1, {&value.Value()})), value, clamped);
}

IrU32 TranslationContext::clampF16Bits(const RdnaOperand& operand, IrU32 bits) {
    rejectHalfOrDoubleOutputModifier(operand);
    if (!operand.clamp) {
        return bits;
    }
    const IrU32 magnitude(ir.BitwiseAnd(bits.Value(), ir.Constant(0x7fffu)));
    const IrU1 zero(ir.LogicalOr(ir.UGreaterThan(bits.Value(), ir.Constant(0x7fffu)), ir.UGreaterThan(magnitude.Value(), ir.Constant(0x7c00u))));
    const IrU32 limited(ir.Select(ir.UGreaterThan(magnitude.Value(), ir.Constant(0x3c00u)), ir.Constant(0x3c00u), bits.Value()));
    const IrU32 clamped(ir.Select(zero.Value(), ir.Constant(0u), limited.Value()));
    return dx10Clamp() ? clamped : IrU32(ir.Select(ir.UGreaterThan(magnitude.Value(), ir.Constant(0x7c00u)), bits.Value(), clamped.Value()));
}

IrU32 TranslationContext::readScalarCode(std::uint32_t code) {
    if (code < NumScalarRegs) {
        return IrU32(ir.GetScalarReg(static_cast<ScalarReg>(code)));
    }
    switch (code) {
        case 106u: return IrU32(ir.GetVccLo());
        case 107u: return IrU32(ir.GetVccHi());
        case 124u: return IrU32(ir.GetM0());
        case 126u:
        case 127u: {
            const std::array<IrU32, 2> mask = ballotMask(IrU1(ir.GetExec()));
            return mask[code - 126u];
        }
        default: return IrU32(ir.Constant(0u));
    }
}

IrU32 TranslationContext::readRawU32(const RdnaOperand& operand) {
    switch (operand.kind) {
        case RdnaOperandKind::LiteralConstant:
        case RdnaOperandKind::IntegerInlineConstant:
        case RdnaOperandKind::FloatInlineConstant: return IrU32(ir.Constant(operand.value));
        case RdnaOperandKind::Null:
        case RdnaOperandKind::PopsExitingWaveId: return IrU32(ir.Constant(0u));
        case RdnaOperandKind::ScalarRegister: return IrU32(ir.GetScalarReg(static_cast<ScalarReg>(operand.reg)));
        case RdnaOperandKind::VectorRegister: return IrU32(ir.GetVectorReg(static_cast<VectorReg>(operand.reg)));
        case RdnaOperandKind::VccLo: return IrU32(ir.GetVccLo());
        case RdnaOperandKind::VccHi: return IrU32(ir.GetVccHi());
        case RdnaOperandKind::M0: return IrU32(ir.GetM0());
        case RdnaOperandKind::ExecLo: return hostExecWord(0u);
        case RdnaOperandKind::ExecHi: return hostExecWord(1u);
        case RdnaOperandKind::Scc: return IrU32(ir.Select(ir.GetScc(), ir.Constant(1u), ir.Constant(0u)));
        case RdnaOperandKind::VccZ:
        case RdnaOperandKind::ExecZ: {
            const bool vcc = operand.kind == RdnaOperandKind::VccZ;
            IrU32 mask(vcc ? IrU32(ir.GetVccLo()) : hostExecWord(0u));
            if (program.WaveSize() == 64u) {
                mask = IrU32(ir.BitwiseOr(mask.Value(), vcc ? ir.GetVccHi() : hostExecWord(1u).Value()));
            }
            const IrU32 zero(ir.Constant(0u));
            return IrU32(ir.Select(ir.IEqual(mask.Value(), zero.Value()), ir.Constant(1u), zero.Value()));
        }
        default: throw std::runtime_error("TranslationContext::readRawU32 invalid decoded operand used as a raw U32 source");
    }
}

void TranslationContext::writeRawU32(const RdnaOperand& operand, IrU32 value) {
    if (operand.kind == RdnaOperandKind::Null) {
        return;
    }
    if (operand.sdwaSel != 6u) {
        std::uint32_t offset = 0u;
        std::uint32_t width = 0u;
        if (operand.sdwaSel <= 3u) {
            offset = operand.sdwaSel * 8u;
            width = 8u;
        } else if (operand.sdwaSel == 4u || operand.sdwaSel == 5u) {
            offset = operand.sdwaSel == 5u ? 16u : 0u;
            width = 16u;
        } else {
            throw std::runtime_error("TranslationContext::writeRawU32 invalid SDWA destination selector");
        }
        switch (operand.sdwaDstUnused) {
            case 0u:
                value = IrU32(ir.Emit(IrOpcode::BitFieldInsert, IrType::U32, {&ir.Constant(0u), &value.Value(), &ir.Constant(offset), &ir.Constant(width)}));
                break;
            case 1u: {
                const IrU32 extended(ir.Emit(IrOpcode::BitFieldSExtract, IrType::U32, {&value.Value(), &ir.Constant(0u), &ir.Constant(width)}));
                value = IrU32(ir.ShiftLeftLogical(extended.Value(), ir.Constant(offset)));
                break;
            }
            case 2u: {
                const std::uint32_t fieldMask = width == 32u ? 0xffffffffu : (1u << width) - 1u;
                const IrU32 inserted(ir.ShiftLeftLogical(ir.BitwiseAnd(value.Value(), ir.Constant(fieldMask)), ir.Constant(offset)));
                const IrU32 cleared(ir.BitwiseAnd(readRawU32(plainOperand(operand)).Value(), ir.Constant(~(fieldMask << offset))));
                value = IrU32(ir.BitwiseOr(cleared.Value(), inserted.Value()));
                break;
            }
            default: throw std::runtime_error("TranslationContext::writeRawU32 reserved SDWA DST_U mode");
        }
    }
    switch (operand.kind) {
        case RdnaOperandKind::ScalarRegister: {
            const ScalarReg reg = static_cast<ScalarReg>(operand.reg);
            ir.SetScalarReg(reg, value.Value());
            ir.SetScalarMaskTag(reg, ir.ConstantBool(false));
            if (RegIndex(reg) > 0u) {
                ir.SetScalarMaskTag(static_cast<ScalarReg>(RegIndex(reg) - 1u), ir.ConstantBool(false));
            }
            break;
        }
        case RdnaOperandKind::VectorRegister: {
            const VectorReg reg = static_cast<VectorReg>(operand.reg);
            IrValue& old = ir.GetVectorReg(reg);
            if (operand.dpp) {
                const auto flags = dppFlags(operand);
                value = IrU32(ir.Emit(IrOpcode::DppUpdateU32, IrType::U32, {&value.Value(), &old, &ir.GetExec()}, flags));
            } else {
                value = IrU32(ir.Select(ir.GetExec(), value.Value(), old));
            }
            ir.SetVectorReg(reg, value.Value());
            break;
        }
        case RdnaOperandKind::VccLo:
            ir.SetVccLo(value.Value());
            ir.SetVcc(threadBit({value, IrU32(ir.GetVccHi())}).Value());
            break;
        case RdnaOperandKind::VccHi:
            ir.SetVccHi(value.Value());
            ir.SetVcc(threadBit({IrU32(ir.GetVccLo()), value}).Value());
            break;
        case RdnaOperandKind::M0: ir.SetM0(value.Value()); break;
        case RdnaOperandKind::ExecLo:
            ir.SetExecLo(value.Value());
            ir.SetExec(threadBit({value, IrU32(ir.GetExecHi())}).Value());
            break;
        case RdnaOperandKind::ExecHi:
            ir.SetExecHi(value.Value());
            ir.SetExec(threadBit({IrU32(ir.GetExecLo()), value}).Value());
            break;
        case RdnaOperandKind::Scc:
            ir.SetScc(ir.INotEqual(value.Value(), ir.Constant(0u)));
            break;
        default: throw std::runtime_error("TranslationContext::writeRawU32 invalid decoded operand used as a destination");
    }
}

}
