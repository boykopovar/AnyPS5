#include "Optimization/ShaderInfoCollector.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace ShaderRecompiler {
namespace {

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error("shader info collection failed: " + message);
}

void AddInput(ShaderInfo& info, StageInputKind kind, std::uint32_t location, std::uint32_t components, std::string name, bool perVertex = false) {
    const auto input = std::ranges::find_if(info.inputs, [kind, location](const StageInput& value) {
        return value.kind == kind && value.location == location;
    });
    if (input == info.inputs.end()) {
        info.inputs.push_back(StageInput {kind, location, components, std::move(name), perVertex});
    } else {
        input->componentCount = std::max(input->componentCount, components);
        input->perVertex = input->perVertex || perVertex;
    }
}

bool HasOutput(const ShaderInfo& info, StageOutputKind kind, std::uint32_t index) {
    return std::ranges::any_of(info.outputs, [kind, index](const StageOutput& output) {
        return output.kind == kind && output.index == index;
    });
}

void AddOutput(ShaderInfo& info, StageOutputKind kind, std::uint32_t index, std::uint32_t location, std::string name) {
    if (!HasOutput(info, kind, index)) {
        info.outputs.push_back(StageOutput {kind, index, location, std::move(name)});
    }
}

void ValidateOptions(const IrProgram& program, ShaderStageInputInfo inputInfo) {
    switch (program.Resources().stage) {
        case IrShaderStage::Vertex:
        case IrShaderStage::Local:
        case IrShaderStage::TessellationControl:
        case IrShaderStage::TessellationEvaluation:
        case IrShaderStage::Mesh:
            if (inputInfo.vertex == nullptr) {
                return Fail("vertex shader has no input metadata");
            }
            if (inputInfo.vertex->resourcesNum < 0 || inputInfo.vertex->resourcesNum > ShaderVertexInputInfo::MaxResources) {
                return Fail("vertex resource count is out of range");
            }
            return;
        case IrShaderStage::Pixel:
            if (inputInfo.pixel == nullptr) {
                return Fail("pixel shader has no input metadata");
            }
            if (inputInfo.pixel->inputNum > std::size(inputInfo.pixel->interpolatorSettings)) {
                return Fail("pixel input count is out of range");
            }
            return;
        case IrShaderStage::Compute:
            if (inputInfo.compute == nullptr) {
                return Fail("compute shader has no input metadata");
            }
            if (inputInfo.compute->threadIdsNum < 0 || inputInfo.compute->threadIdsNum > 3) {
                return Fail("compute thread ID count is out of range");
            }
            return;
        default:
            return Fail("unsupported shader stage for info collection");
    }
}

void ValidateValueReferences(const IrProgram& program, ShaderStageInputInfo inputInfo) {
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            switch (inst->Opcode()) {
                case IrOpcode::GetAttribute: {
                    const IrValue* attribute = inst->Argument(0)->Resolve();
                    const IrValue* channel = inst->Argument(1)->Resolve();
                    if (!attribute->HasImmediate() || attribute->Type() != IrType::U32 || !channel->HasImmediate() || channel->Type() != IrType::U32) {
                        return Fail("typed attribute reference is not constant");
                    }
                    const auto stage = program.Resources().stage;
                    if ((stage == IrShaderStage::Vertex || stage == IrShaderStage::Local) &&
                        (channel->ImmediateU32() >= 4u || attribute->ImmediateU32() >= static_cast<std::uint32_t>(inputInfo.vertex->resourcesNum))) {
                        return Fail("vertex input reference is out of range");
                    }
                    if (stage == IrShaderStage::Pixel && (channel->ImmediateU32() >= 4u || attribute->ImmediateU32() >= inputInfo.pixel->inputNum)) {
                        return Fail("pixel input reference is out of range");
                    }
                    break;
                }
                case IrOpcode::GetInterpolationParameter: {
                    const IrValue* input = inst->Argument(0)->Resolve();
                    const IrValue* component = inst->Argument(1)->Resolve();
                    const IrValue* mode = inst->Argument(2)->Resolve();
                    if (program.Resources().stage != IrShaderStage::Pixel || !input->HasImmediate() || input->Type() != IrType::U32 ||
                        !component->HasImmediate() || component->Type() != IrType::U32 || !mode->HasImmediate() || mode->Type() != IrType::U32) {
                        return Fail("interpolation parameter reference is invalid");
                    }
                    if (input->ImmediateU32() >= inputInfo.pixel->inputNum || component->ImmediateU32() >= 4u || mode->ImmediateU32() >= 3u) {
                        return Fail("interpolation parameter reference is out of range");
                    }
                    break;
                }
                case IrOpcode::GetBuiltin: {
                    const IrValue* kindValue = inst->Argument(0)->Resolve();
                    const IrValue* componentValue = inst->Argument(1)->Resolve();
                    if (!kindValue->HasImmediate() || kindValue->Type() != IrType::U32 || !componentValue->HasImmediate() || componentValue->Type() != IrType::U32) {
                        return Fail("typed builtin reference is not constant");
                    }
                    const auto kind = static_cast<StageInputKind>(kindValue->ImmediateU32());
                    const auto component = componentValue->ImmediateU32();
                    switch (kind) {
                        case StageInputKind::PackedAncillary:
                            return Fail("packed pixel ancillary input has an unsupported live use");
                        case StageInputKind::Layer:
                        case StageInputKind::SampleId:
                            if (program.Resources().stage != IrShaderStage::Pixel || component != 0u) {
                                return Fail("typed pixel scalar input is invalid");
                            }
                            break;
                        case StageInputKind::VertexIndex:
                        case StageInputKind::InstanceIndex:
                        case StageInputKind::InvocationId:
                        case StageInputKind::PrimitiveId:
                        case StageInputKind::FrontFacing:
                        case StageInputKind::HelperInvocation:
                        case StageInputKind::LocalInvocationIndex:
                            if (component != 0u) {
                                return Fail("typed scalar builtin component is out of range");
                            }
                            break;
                        case StageInputKind::FragCoord:
                            if (component >= 4u) {
                                return Fail("typed fragment-coordinate component is out of range");
                            }
                            break;
                        case StageInputKind::BaryCoordSmooth:
                        case StageInputKind::BaryCoordNoPerspective:
                            if (component >= 2u) {
                                return Fail("typed barycentric component is out of range");
                            }
                            break;
                        case StageInputKind::TessCoord:
                        case StageInputKind::WorkgroupId:
                        case StageInputKind::LocalInvocationId:
                        case StageInputKind::GlobalInvocationId:
                        case StageInputKind::DispatchThreadLimit:
                            if (component >= 3u) {
                                return Fail("typed invocation builtin component is out of range");
                            }
                            break;
                        case StageInputKind::Parameter:
                        default:
                            return Fail("typed builtin kind is invalid");
                    }
                    break;
                }
                case IrOpcode::SetAttribute: {
                    const auto index = inst->Flags<ExportFlags>().index;
                    if (index >= program.Metadata().exportInfo.size()) {
                        return Fail("typed export metadata index is out of range");
                    }
                    const auto& exportInfo = program.Metadata().exportInfo[index];
                    const auto stage = program.Resources().stage;
                    if (exportInfo.kind == ExportTargetKind::Position && exportInfo.index != 0u && exportInfo.en != 0u &&
                        stage != IrShaderStage::Vertex && stage != IrShaderStage::Mesh && stage != IrShaderStage::TessellationEvaluation) {
                        return Fail("auxiliary position export requires a vertex, mesh, or tessellation evaluation shader");
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CollectVertexInputs(const IrProgram& program, const ShaderVertexInputInfo* vertex, ShaderInfo& info) {
    AddInput(info, StageInputKind::VertexIndex, 0, 1, "gl_VertexIndex");
    AddInput(info, StageInputKind::InstanceIndex, 0, 1, "gl_InstanceIndex");
    std::array<std::uint32_t, ShaderVertexInputInfo::MaxResources> usedComponents {};
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetAttribute) {
                const auto attribute = inst->Argument(0)->Resolve()->ImmediateU32();
                const auto channel = inst->Argument(1)->Resolve()->ImmediateU32();
                usedComponents[attribute] = std::max(usedComponents[attribute], channel + 1u);
            }
        }
    }
    for (std::uint32_t attribute = 0; attribute < static_cast<std::uint32_t>(vertex->resourcesNum); attribute++) {
        if (usedComponents[attribute] != 0u) {
            AddInput(info, StageInputKind::Parameter, attribute, usedComponents[attribute], "in_attr_" + std::to_string(attribute));
        }
    }
}

bool IsPixelParameterCustom(const ShaderPixelInputInfo& pixel, std::uint32_t input) {
    return pixel.InputIsCustom(input);
}

bool IsPixelParameterFlat(const ShaderPixelInputInfo& pixel, std::uint32_t input) {
    constexpr std::uint32_t flatShadeBit = 0x00000400u;
    return input < pixel.inputNum && (pixel.interpolatorSettings[input] & flatShadeBit) != 0u && !IsPixelParameterCustom(pixel, input);
}

void CollectPixelInputs(const IrProgram& program, const ShaderPixelInputInfo* pixel, ShaderInfo& info) {
    if (pixel->HasPositionInput() || pixel->quadPixelMask != 0xf) {
        AddInput(info, StageInputKind::FragCoord, 0, 4, "gl_FragCoord");
    }
    if (pixel->psFrontFace) {
        AddInput(info, StageInputKind::FrontFacing, 0, 1, "gl_FrontFacing");
    }
    std::array<bool, 32> read {};
    std::array<bool, 32> perVertex {};
    std::array<bool, 32> interpolated {};
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetAttribute) {
                const auto input = inst->Argument(0)->Resolve()->ImmediateU32();
                if (IsPixelParameterCustom(*pixel, input)) {
                    throw std::runtime_error("pixel input " + std::to_string(input) + " passes its vertices through unchanged but is read with v_interp_p1/p2");
                }
                read[input] = true;
                interpolated[input] = true;
            } else if (inst->Opcode() == IrOpcode::GetInterpolationParameter) {
                const auto input = inst->Argument(0)->Resolve()->ImmediateU32();
                const auto mode = inst->Argument(2)->Resolve()->ImmediateU32();
                read[input] = true;
                perVertex[input] = perVertex[input] || mode < 2u || !IsPixelParameterFlat(*pixel, input);
            }
        }
    }
    const auto& metadata = program.Metadata();
    const auto linear = [&](std::uint32_t input) {
        return pixel->InputIsLinear(input, metadata.pixelLinearInputs, metadata.pixelPerspectiveInputs);
    };
    const auto interpolation = [&](std::uint32_t input) {
        return IsPixelParameterFlat(*pixel, input) ? 2u : linear(input) ? 1u : 0u;
    };
    constexpr std::uint32_t unassigned = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, 32> slotInterpolation {};
    slotInterpolation.fill(unassigned);
    std::array<bool, 32> slotPerVertex {};
    for (std::uint32_t input = 0; input < pixel->inputNum; input++) {
        if (!read[input] || pixel->InputIsDefault(input)) {
            continue;
        }
        const auto slot = pixel->InputSlot(input);
        if (slotInterpolation[slot] == unassigned) {
            slotInterpolation[slot] = interpolation(input);
        } else if (slotInterpolation[slot] != interpolation(input)) {
            slotPerVertex[slot] = true;
        }
        slotPerVertex[slot] = slotPerVertex[slot] || perVertex[input];
    }
    bool smooth = false;
    bool noPerspective = false;
    for (std::uint32_t input = 0; input < pixel->inputNum; input++) {
        if (!read[input] || pixel->InputIsDefault(input)) {
            continue;
        }
        const bool vertexInput = slotPerVertex[pixel->InputSlot(input)];
        AddInput(info, StageInputKind::Parameter, input, 4, "in_param_" + std::to_string(input), vertexInput);
        if (vertexInput && interpolated[input] && !IsPixelParameterFlat(*pixel, input)) {
            (linear(input) ? noPerspective : smooth) = true;
        }
    }
    if (smooth) {
        AddInput(info, StageInputKind::BaryCoordSmooth, 0, 3, "gl_BaryCoordKHR");
    }
    if (noPerspective) {
        AddInput(info, StageInputKind::BaryCoordNoPerspective, 0, 3, "gl_BaryCoordNoPerspKHR");
    }
}

void CollectComputeInputs(const ShaderComputeInputInfo* compute, ShaderInfo& info) {
    if (compute->groupId[0] || compute->groupId[1] || compute->groupId[2]) {
        AddInput(info, StageInputKind::WorkgroupId, 0, 3, "gl_WorkGroupID");
    }
    if (compute->threadIdsNum > 0) {
        AddInput(info, StageInputKind::LocalInvocationId, 0, 3, "gl_LocalInvocationID");
    }
    if (compute->threadIdsNum > 0 || compute->tgSizeEn) {
        AddInput(info, StageInputKind::LocalInvocationIndex, 0, 1, "gl_LocalInvocationIndex");
    }
}

void CollectBuiltinInputs(const IrProgram& program, ShaderInfo& info) {
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (program.Resources().stage == IrShaderStage::TessellationControl && inst->Opcode() == IrOpcode::LaneId) {
                AddInput(info, StageInputKind::InvocationId, 0, 1, "gl_InvocationID");
            }
            if (inst->Opcode() != IrOpcode::GetBuiltin) {
                continue;
            }
            const auto kind = static_cast<StageInputKind>(inst->Argument(0)->Resolve()->ImmediateU32());
            switch (kind) {
                case StageInputKind::VertexIndex:
                    AddInput(info, kind, 0, 1, "gl_VertexIndex");
                    break;
                case StageInputKind::InstanceIndex:
                    AddInput(info, kind, 0, 1, "gl_InstanceIndex");
                    break;
                case StageInputKind::InvocationId:
                    AddInput(info, kind, 0, 1, "gl_InvocationID");
                    break;
                case StageInputKind::PrimitiveId:
                    AddInput(info, kind, 0, 1, "gl_PrimitiveID");
                    break;
                case StageInputKind::TessCoord:
                    AddInput(info, kind, 0, 3, "gl_TessCoord");
                    break;
                case StageInputKind::FragCoord:
                    AddInput(info, kind, 0, 4, "gl_FragCoord");
                    break;
                case StageInputKind::FrontFacing:
                    AddInput(info, kind, 0, 1, "gl_FrontFacing");
                    break;
                case StageInputKind::HelperInvocation:
                    AddInput(info, kind, 0, 1, "gl_HelperInvocation");
                    break;
                case StageInputKind::Layer:
                    AddInput(info, kind, 0, 1, "gl_Layer");
                    break;
                case StageInputKind::SampleId:
                    AddInput(info, kind, 0, 1, "gl_SampleID");
                    break;
                case StageInputKind::BaryCoordSmooth:
                    AddInput(info, kind, 0, 3, "gl_BaryCoordKHR");
                    break;
                case StageInputKind::BaryCoordNoPerspective:
                    AddInput(info, kind, 0, 3, "gl_BaryCoordNoPerspKHR");
                    break;
                case StageInputKind::WorkgroupId:
                    AddInput(info, kind, 0, 3, "gl_WorkGroupID");
                    break;
                case StageInputKind::LocalInvocationId:
                    AddInput(info, kind, 0, 3, "gl_LocalInvocationID");
                    break;
                case StageInputKind::LocalInvocationIndex:
                    AddInput(info, kind, 0, 1, "gl_LocalInvocationIndex");
                    break;
                case StageInputKind::GlobalInvocationId:
                    AddInput(info, kind, 0, 3, "gl_GlobalInvocationID");
                    break;
                case StageInputKind::DispatchThreadLimit:
                    info.dispatchThreadLimit = true;
                    break;
                case StageInputKind::PackedAncillary:
                case StageInputKind::Parameter:
                    break;
            }
        }
    }
}

void CollectOutputs(const IrProgram& program, ShaderStageInputInfo inputInfo, ShaderInfo& info) {
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::SetAttribute) {
                continue;
            }
            const auto& exportInfo = program.Metadata().exportInfo[inst->Flags<ExportFlags>().index];
            if (exportInfo.kind == ExportTargetKind::MrtZ) {
                if (program.Resources().stage == IrShaderStage::Pixel && (exportInfo.en & 0x1u) != 0u && inputInfo.pixel->psDepthExportEnable) {
                    AddOutput(info, StageOutputKind::Depth, 0, 0, "gl_FragDepth");
                }
                if (program.Resources().stage == IrShaderStage::Pixel && (exportInfo.en & 0x4u) != 0u && inputInfo.pixel->psSampleMaskExportEnable) {
                    AddOutput(info, StageOutputKind::SampleMask, 0, 0, "gl_SampleMask");
                }
                continue;
            }
            if (exportInfo.en == 0u) {
                continue;
            }
            switch (exportInfo.kind) {
                case ExportTargetKind::Position:
                    if (exportInfo.index == 0u) {
                        AddOutput(info, StageOutputKind::Position, 0, 0, "out_position");
                        break;
                    }
                    if (exportInfo.compr) {
                        return Fail("compressed auxiliary position export is unsupported");
                    }
                    for (std::uint32_t component = 0; component < 4u; component++) {
                        if ((exportInfo.en & (1u << component)) == 0u) {
                            continue;
                        }
                        const auto output = DecodePositionExportComponent(inputInfo.vertex->paClVsOutCntl, exportInfo.index, component);
                        if (output.viewport) {
                            AddOutput(info, StageOutputKind::ViewportIndex, 0, 0, "gl_ViewportIndex");
                        }
                        if (output.pointSize) {
                            AddOutput(info, StageOutputKind::PointSize, 0, 0, "gl_PointSize");
                        }
                        if (output.layer) {
                            AddOutput(info, StageOutputKind::Layer, 0, 0, "gl_Layer");
                        }
                        if (output.clipDistance != std::numeric_limits<std::uint32_t>::max()) {
                            AddOutput(info, StageOutputKind::ClipDistance, output.clipDistance, 0, "gl_ClipDistance");
                        }
                        if (output.cullDistance != std::numeric_limits<std::uint32_t>::max()) {
                            AddOutput(info, StageOutputKind::CullDistance, output.cullDistance, 0, "gl_CullDistance");
                        }
                    }
                    break;
                case ExportTargetKind::Parameter:
                    AddOutput(info, StageOutputKind::Parameter, exportInfo.index, exportInfo.index, "out_param_" + std::to_string(exportInfo.index));
                    break;
                case ExportTargetKind::Mrt:
                    AddOutput(info, StageOutputKind::Mrt, exportInfo.index, exportInfo.index, "out_mrt_" + std::to_string(exportInfo.index));
                    break;
                default:
                    break;
            }
        }
    }
}

std::uint8_t SamplerUseOf(IrOpcode opcode, std::uint32_t flags, IrShaderStage stage) {
    if (opcode == IrOpcode::ImageGatherRaw) {
        return SamplerUseGather;
    }
    if (opcode == IrOpcode::ImageQueryLod) {
        return SamplerUseQueryLod;
    }
    std::uint32_t use = (flags & RdnaImageSampleFlagDerivative) != 0u ? SamplerUseGradient : ImageSampleExplicitLod(flags, stage) ? SamplerUseExplicitLod : SamplerUseImplicitLod;
    if ((flags & RdnaImageSampleFlagOffset) != 0u) {
        use |= SamplerUseOffset;
    }
    if ((flags & RdnaImageSampleFlagCompare) != 0u) {
        use |= SamplerUseCompare;
    }
    if ((flags & RdnaImageSampleFlagAdjust) != 0u) {
        use |= SamplerUseAdjust;
    }
    return static_cast<std::uint8_t>(use);
}

void CollectSamplerUses(const IrProgram& program, ShaderInfo& info) {
    for (auto& sampler : info.samplers) {
        sampler.uses = 0;
    }
    const auto& memoryInfo = program.Resources().memoryInfo;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (!ImageOpcodeInfoOf(inst->Opcode()).needsSampler) {
                continue;
            }
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= memoryInfo.size()) {
                return Fail("image instruction has no memory metadata");
            }
            const auto& memory = memoryInfo[index];
            if (memory.sampler >= info.samplers.size()) {
                return Fail("image instruction references an unknown sampler");
            }
            auto& sampler = info.samplers[memory.sampler];
            sampler.uses = static_cast<std::uint8_t>(sampler.uses | SamplerUseOf(inst->Opcode(), memory.imageSampleFlags, program.Resources().stage));
        }
    }
}

}

void ShaderInfoCollector::Collect(IrProgram& program, const ShaderStageInputInfo& inputInfo) const {
    if (!program.Resources().resourceTrackingComplete || program.Metadata().shaderInfoComplete) {
        return Fail(!program.Resources().resourceTrackingComplete ? "shader resources were not tracked" : "shader info already collected");
    }
    ValidateOptions(program, inputInfo);
    ValidateValueReferences(program, inputInfo);
    auto next = program.Info();
    next.inputs.clear();
    next.outputs.clear();
    next.hasBitwiseXor = std::ranges::any_of(program.Blocks(), [](const auto& block) {
        return std::ranges::any_of(block->Instructions(), [](const IrValue* inst) {
            return inst->Opcode() == IrOpcode::BitwiseXor32;
        });
    });
    switch (program.Resources().stage) {
        case IrShaderStage::Vertex:
        case IrShaderStage::Local:
            CollectVertexInputs(program, inputInfo.vertex, next);
            break;
        case IrShaderStage::TessellationControl:
        case IrShaderStage::TessellationEvaluation:
        case IrShaderStage::Mesh:
            break;
        case IrShaderStage::Pixel:
            CollectPixelInputs(program, inputInfo.pixel, next);
            break;
        case IrShaderStage::Compute:
            CollectComputeInputs(inputInfo.compute, next);
            break;
        default:
            return Fail("unsupported shader stage for info collection");
    }
    CollectBuiltinInputs(program, next);
    CollectOutputs(program, inputInfo, next);
    CollectSamplerUses(program, next);
    program.Info() = std::move(next);
    program.Metadata().shaderInfoComplete = true;
}

void ShaderInfoCollector::Collect(IrProgram& program) const {
    Collect(program, ShaderStageInputInfo {});
}

}
