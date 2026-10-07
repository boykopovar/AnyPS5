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
    case 0x08u:
        return ExportTargetKind::MrtZ;
    case 0x09u:
        return ExportTargetKind::Null;
    case 0x14u:
        return ExportTargetKind::Primitive;
    default:
        break;
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

std::uint32_t getDstSel(std::uint32_t dstSelXYZW, std::uint32_t component) {
    return (dstSelXYZW >> (component * 3u)) & 0x7u;
}

bool isIntegerBufferFormat(IrBufferFormat format) {
    switch (format) {
    case IrBufferFormat::Format8UInt:
    case IrBufferFormat::Format8SInt:
    case IrBufferFormat::Format16UInt:
    case IrBufferFormat::Format16SInt:
    case IrBufferFormat::Format8_8UInt:
    case IrBufferFormat::Format8_8SInt:
    case IrBufferFormat::Format32UInt:
    case IrBufferFormat::Format32SInt:
    case IrBufferFormat::Format16_16UInt:
    case IrBufferFormat::Format16_16SInt:
    case IrBufferFormat::Format11_11_10UInt:
    case IrBufferFormat::Format11_11_10SInt:
    case IrBufferFormat::Format10_11_11UInt:
    case IrBufferFormat::Format10_11_11SInt:
    case IrBufferFormat::Format2_10_10_10UInt:
    case IrBufferFormat::Format2_10_10_10SInt:
    case IrBufferFormat::Format10_10_10_2UInt:
    case IrBufferFormat::Format10_10_10_2SInt:
    case IrBufferFormat::Format8_8_8_8UInt:
    case IrBufferFormat::Format8_8_8_8SInt:
    case IrBufferFormat::Format32_32UInt:
    case IrBufferFormat::Format32_32SInt:
    case IrBufferFormat::Format16_16_16_16UInt:
    case IrBufferFormat::Format16_16_16_16SInt:
    case IrBufferFormat::Format32_32_32UInt:
    case IrBufferFormat::Format32_32_32SInt:
    case IrBufferFormat::Format32_32_32_32UInt:
    case IrBufferFormat::Format32_32_32_32SInt:
        return true;
    default:
        return false;
    }
}

std::uint32_t formattedConstantBits(IrBufferFormat format, std::uint32_t selector) {
    if (selector == 0u) {
        return 0u;
    }
    if (selector == 1u) {
        return isIntegerBufferFormat(format) ? 1u : std::bit_cast<std::uint32_t>(1.0f);
    }
    throw std::runtime_error("reserved buffer destination selector");
}

}

void TranslateAttributeInstruction(IrBuilder& builder, const RdnaInstruction& instruction,
                                   const TranslateOptions& options) {
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
    if (!fragmentShaderBarycentricEnabled)
        return;
    auto& delta = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32,
                          {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(0u)});
    auto& origin = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32,
                           {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(2u)});
    auto& product =
        ir.Emit(IrOpcode::FPMul32, IrType::F32, {&ir.BitCastF32(delta), readOperand(inst.source0, IrType::F32)});
    auto& result = ir.Emit(IrOpcode::FPAdd32, IrType::F32, {&product, &ir.BitCastF32(origin)});
    writeOperand(inst.destination, &result);
}

void TranslationContext::vInterpP2F32(const RdnaInstruction& inst) {
    if (fragmentShaderBarycentricEnabled) {
        auto& delta = ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32,
                              {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(1u)});
        auto& product =
            ir.Emit(IrOpcode::FPMul32, IrType::F32, {&ir.BitCastF32(delta), readOperand(inst.source0, IrType::F32)});
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
    IrValue& value = ir.Emit(IrOpcode::GetAttribute, IrType::U32,
                             {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value)});
    writeOperand(inst.destination, &value);
}

void TranslationContext::vInterpMovF32(const RdnaInstruction& inst) {
    if (inst.source0.value >= 3u) {
        throw std::runtime_error("v_interp_mov_f32 mode is reserved");
    }
    IrValue& value =
        ir.Emit(IrOpcode::GetInterpolationParameter, IrType::U32,
                {&ir.Constant(inst.source1.value), &ir.Constant(inst.source2.value), &ir.Constant(inst.source0.value)});
    writeOperand(inst.destination, &value);
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
    IrValue& data = ir.Emit(IrOpcode::CompositeConstructU32x4, IrType::U32x4,
                            {components[0], components[1], components[2], components[3]});
    IrValue& exec = ir.GetExec();
    (void)ir.Emit(IrOpcode::SetAttribute, IrType::Void, {&data, &exec}, addExportInfo(inst));
}

bool TranslationContext::emitInterpolation(const RdnaInstruction& inst) {
    if ((inst.op == RdnaOpcode::VInterpP1F32 || inst.op == RdnaOpcode::VInterpP2F32) && pixelInput != nullptr &&
        pixelInput->InputIsCustom(inst.source1.value)) {
        throw std::runtime_error("pixel input " + std::to_string(inst.source1.value) +
                                 " passes its vertices through unchanged but is read with v_interp_p1/p2");
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
    default:
        return false;
    }
}

void TranslationContext::TranslateEmbeddedFetch(const RdnaInstruction& instruction, std::uint32_t attribute,
                                                std::uint32_t componentCount, const ShaderBufferResource& resource) {
    const auto format = static_cast<IrBufferFormat>((resource.fields[3] >> 12u) & 0x7Fu);
    const std::uint32_t dstSel = resource.fields[3] & 0xFFFu;
    for (std::uint32_t component = 0u; component < componentCount; ++component) {
        const std::uint32_t selector =
            instruction.formatted && !instruction.typed ? getDstSel(dstSel, component) : component + 4u;
        IrValue* value = nullptr;
        if (selector <= 1u) {
            value = &ir.Constant(formattedConstantBits(format, selector));
        } else if (selector >= 4u && selector <= 7u) {
            const std::uint32_t memoryComponent = selector - 4u;
            value =
                &ir.Emit(IrOpcode::GetAttribute, IrType::U32, {&ir.Constant(attribute), &ir.Constant(memoryComponent)});
            std::uint8_t& required = program.Info().vertexFetchComponents[attribute];
            required = static_cast<std::uint8_t>(std::max<std::uint32_t>(required, memoryComponent + 1u));
        } else {
            throw std::runtime_error("invalid embedded fetch component selector");
        }
        writeOperand(offsetOperand(instruction.destination, component), value);
    }
}

void TranslateAttributeInstruction(TranslationContext& context, const RdnaInstruction& instruction) {
    throw std::runtime_error("TranslateAttributeInstruction not implemented");
}

}
