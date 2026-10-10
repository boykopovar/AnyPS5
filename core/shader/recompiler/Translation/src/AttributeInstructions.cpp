#include "Translation/AttributeInstructions.hpp"
#include "Translation/TranslationContext.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

ExportTargetKind exportTargetKindFromTarget(std::uint32_t target, std::uint32_t& index) {
    index = 0u;
    switch (target) {
        case 0x08u: return ExportTargetKind::MrtZ;
        case 0x09u: return ExportTargetKind::Null;
        case 0x14u: return ExportTargetKind::Primitive;
        default: break;
    }
    if (target <= 0x07u) {
        index = target;
        return ExportTargetKind::Mrt;
    }
    if (target >= 0x0cu && target <= 0x0fu) {
        index = target - 0x0cu;
        return ExportTargetKind::Position;
    }
    if (target >= 0x20u && target <= 0x3fu) {
        index = target - 0x20u;
        return ExportTargetKind::Parameter;
    }
    return ExportTargetKind::Unknown;
}

}

void TranslateAttributeInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const TranslateOptions& options) {
    throw std::runtime_error("TranslateAttributeInstruction not implemented");
}

ExportFlags TranslationContext::addExportInfo(const RdnaInstruction& inst) {
    ExportInfo info;
    info.kind = exportTargetKindFromTarget(inst.exportTarget, info.index);
    info.target = inst.exportTarget;
    info.en = inst.exportEnableMask;
    info.done = inst.exportIsLast;
    info.compr = inst.exportIsCompressed;
    info.vm = inst.exportValidMask;
    const std::uint32_t index = static_cast<std::uint32_t>(program.Metadata().exportInfo.size());
    program.Metadata().exportInfo.push_back(info);
    return ExportFlags{index, inst.programCounter};
}

void TranslationContext::vInterpP1F32(const RdnaInstruction& inst) {
    if (!fragmentShaderBarycentricEnabled) {
        writeOperand(inst.destination, &ir.Emit(IrOpcode::InterpolateHostP1, IrType::F32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), readOperand(inst.source0, IrType::F32)}));
        return;
    }
    auto& delta = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(0u)});
    auto& origin = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(2u)});
    auto& product = ir.Emit(IrOpcode::FPMul32, IrType::F32, {&ir.BitCastF32(delta), readOperand(inst.source0, IrType::F32)});
    auto& result = ir.Emit(IrOpcode::FPAdd32, IrType::F32, {&product, &ir.BitCastF32(origin)});
    writeOperand(inst.destination, &result);
}

void TranslationContext::vInterpP2F32(const RdnaInstruction& inst) {
    if (fragmentShaderBarycentricEnabled) {
        auto& delta = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(1u)});
        auto& product = ir.Emit(IrOpcode::FPMul32, IrType::F32, {&ir.BitCastF32(delta), readOperand(inst.source0, IrType::F32)});
        auto& result = ir.Emit(IrOpcode::FPAdd32, IrType::F32, {&product, readOperand(inst.destination, IrType::F32)});
        writeOperand(inst.destination, &result);
        return;
    }
    if (pixelInput != nullptr && inst.source0.kind == RdnaOperandKind::VectorRegister && inst.source1.value < 32u) {
        const auto readsPair = [&](PixelInput input) {
            const auto base = pixelInput->psInputVgpr[static_cast<std::size_t>(input)];
            return base != ShaderPixelInputInfo::NoPixelInputVgpr && inst.source0.reg == base + 1u;
        };
        const auto bit = 1u << inst.source1.value;
        if (readsPair(PixelInput::LinearCenter) || readsPair(PixelInput::LinearCentroid)) {
            program.Metadata().pixelLinearInputs |= bit;
        } else if (readsPair(PixelInput::PerspectiveCenter) || readsPair(PixelInput::PerspectiveCentroid)) {
            program.Metadata().pixelPerspectiveInputs |= bit;
        }
    }
    ir.Emit(IrOpcode::InterpolateHostP2, IrType::Void, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), readOperand(inst.source0, IrType::F32), readOperand(inst.destination, IrType::F32), &ir.GetExec()});
    IrValue& value = ir.Emit(IrOpcode::GetAttribute, IrType::U32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value)});
    writeOperand(inst.destination, &value);
}

void TranslationContext::vInterpMovF32(const RdnaInstruction& inst) {
    if (inst.source0.value >= 3u) {
        throw std::runtime_error("v_interp_mov_f32 mode is reserved");
    }
    IrValue& value = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(inst.source0.value)});
    writeOperand(inst.destination, &value);
}

IrF32 TranslationContext::interpolationParameterF16(const RdnaInstruction& inst, std::uint32_t mode) {
    if (!fragmentShaderBarycentricEnabled) {
        throw std::runtime_error("16-bit interpolation of pixel input " + std::to_string(inst.source1.value) + " requires fragmentShaderBarycentric");
    }
    if (pixelInput == nullptr || !pixelInput->InputIsFp16(inst.source1.value)) {
        throw std::runtime_error("pixel input " + std::to_string(inst.source1.value) + " is read with 16-bit interpolation without FP16_INTERP_MODE");
    }
    return IrF32(ir.Emit(IrOpcode::GetInterpolationParameterF16, IrType::F32, {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(mode), &ir.Constant(inst.source1.opSel ? 1u : 0u)}));
}

std::uint32_t TranslationContext::interpolationModeF16(const RdnaInstruction& inst) const {
    if (!floatMode.has_value()) return 0u;
    const auto mode = floatMode->floatMode;
    const auto denorm32 = (mode >> 4u) & 3u;
    const auto denorm16 = (mode >> 6u) & 3u;
    if ((mode & 0xfu) != 0u || (denorm32 != 0u && denorm32 != 3u) || (denorm16 != 0u && denorm16 != 3u)) {
        throw std::runtime_error("16-bit interpolation at pc " + std::to_string(inst.programCounter) + " in FLOAT_MODE " + std::to_string(mode) + " is not measured");
    }
    return (floatMode->ieeeMode ? InterpolationQuiet : 0u) | (denorm32 == 0u ? InterpolationFlush32 : 0u) | (denorm16 == 0u ? InterpolationFlush16 : 0u);
}

void TranslationContext::vInterpP1F16(const RdnaInstruction& inst) {
    const auto mode = interpolationModeF16(inst);
    const IrF32 delta = interpolationParameterF16(inst, 0u);
    const IrF32 origin = inst.op == RdnaOpcode::VInterpP1lvF16 ? readF16AsF32(inst.source3) : interpolationParameterF16(inst, 2u);
    writeOperand(inst.destination, &ir.Emit(IrOpcode::FPInterpolateF32, IrType::F32, {&delta.Value(), readOperand(inst.source0, IrType::F32), &origin.Value(), &ir.Constant(mode)}));
}

void TranslationContext::vInterpP2F16(const RdnaInstruction& inst) {
    const auto mode = interpolationModeF16(inst);
    const IrF32 delta = interpolationParameterF16(inst, 1u);
    IrValue* coordinate = readOperand(inst.source0, IrType::F32);
    IrValue* partial = readOperand(inst.source3, IrType::F32);
    writeF16(inst.destination, IrF32(ir.Emit(IrOpcode::FPInterpolateF16, IrType::F32, {&delta.Value(), coordinate, partial, &ir.Constant(mode)})), {&delta.Value(), coordinate, partial});
}

void TranslationContext::eXP(const RdnaInstruction& inst) {
    std::uint32_t index = 0u;
    if (exportTargetKindFromTarget(inst.exportTarget, index) == ExportTargetKind::Unknown) {
        throw std::runtime_error("unsupported EXP target");
    }
    std::array<IrValue*, 4> components{&ir.Constant(0u), &ir.Constant(0u), &ir.Constant(0u), &ir.Constant(0u)};
    const std::uint32_t sourceCount = std::min(inst.sourceCount, 4u);
    for (std::uint32_t source = 0u; source < sourceCount; ++source) {
        components[source] = &readRawU32(plainOperand(sourceAt(inst, source))).Value();
    }
    IrValue& data = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4, {components[0], components[1], components[2], components[3]});
    IrValue& exec = ir.GetExec();
    (void)ir.Emit(IrOpcode::SetAttribute, IrType::Void, {&data, &exec}, addExportInfo(inst));
}

bool TranslationContext::emitInterpolation(const RdnaInstruction& inst) {
    const bool interpolates = inst.op == RdnaOpcode::VInterpP1F32 || inst.op == RdnaOpcode::VInterpP2F32 || inst.op == RdnaOpcode::VInterpP1llF16 || inst.op == RdnaOpcode::VInterpP1lvF16 || inst.op == RdnaOpcode::VInterpP2F16;
    if (interpolates && pixelInput != nullptr && pixelInput->InputIsCustom(inst.source1.value)) {
        throw std::runtime_error("pixel input " + std::to_string(inst.source1.value) + " passes its vertices through unchanged but is read with v_interp_p1/p2");
    }
    switch (inst.op) {
        case RdnaOpcode::VInterpP1F32:
            vInterpP1F32(inst);
            return true;
        case RdnaOpcode::VInterpP2F32:
            vInterpP2F32(inst);
            return true;
        case RdnaOpcode::VInterpMovF32:
            vInterpMovF32(inst);
            return true;
        case RdnaOpcode::VInterpP1llF16:
        case RdnaOpcode::VInterpP1lvF16:
            vInterpP1F16(inst);
            return true;
        case RdnaOpcode::VInterpP2F16:
            vInterpP2F16(inst);
            return true;
        default:
            return false;
    }
}

void TranslationContext::TranslateEmbeddedFetch(const RdnaInstruction& instruction, std::uint32_t attribute, std::uint32_t components) {
    if (!instruction.formatted || instruction.typed || components == 0u || components > 4u) throw std::runtime_error("invalid prepared vertex fetch");
    for (std::uint32_t component = 0; component < components; ++component) {
        auto& value = ir.Emit(IrOpcode::GetAttribute, IrType::U32, {&ir.Constant(attribute), &ir.Constant(component)});
        value.SetFlags<std::uint32_t>(1u);
        writeOperand(offsetOperand(instruction.destination, component), &value);
    }
}

void TranslateAttributeInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateAttributeInstruction not implemented");
}

}
