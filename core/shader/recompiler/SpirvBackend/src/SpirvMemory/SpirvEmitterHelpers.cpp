#include "Optimization/ResourceMaterializer.hpp"
#include "BdaAbi.hpp"
#include "PipelineSpecialization.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvMemory/SpirvDescriptors.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{
void FailEmit(const std::string& reason) {
    throw std::runtime_error("SPIR-V module emission failed: " + reason);
}

IrShaderStage StageOf(const SpirvEmitterState& state) {
    return state.program.Resources().stage;
}

const ShaderVertexInputInfo& VertexInfo(const SpirvEmitterState& state) {
    if (state.inputInfo.vertex == nullptr) {
        FailEmit("vertex input info is missing");
    }
    return *state.inputInfo.vertex;
}

const ShaderPixelInputInfo& PixelInfo(const SpirvEmitterState& state) {
    if (state.inputInfo.pixel == nullptr) {
        FailEmit("pixel input info is missing");
    }
    return *state.inputInfo.pixel;
}

const ShaderWorkgroupInputInfo* ShaderWorkgroupInput(const SpirvEmitterState& state) {
    switch (state.program.Resources().stage) {
    case IrShaderStage::Compute:
        if (state.inputInfo.compute == nullptr) {
            FailEmit("compute input info is missing");
        }
        return state.inputInfo.compute;
    case IrShaderStage::Mesh:
        if (state.inputInfo.vertex == nullptr) {
            FailEmit("vertex input info is missing");
        }
        return &state.inputInfo.vertex->mesh;
    default:
        return nullptr;
    }
}

namespace {

constexpr std::uint32_t NoBuiltIn = std::numeric_limits<std::uint32_t>::max();

std::uint32_t BuiltInForInput(StageInputKind kind) {
    switch (kind) {
    case StageInputKind::VertexIndex: return spv::BuiltInVertexIndex;
    case StageInputKind::InvocationId: return spv::BuiltInInvocationId;
    case StageInputKind::PrimitiveId: return spv::BuiltInPrimitiveId;
    case StageInputKind::TessCoord: return spv::BuiltInTessCoord;
    case StageInputKind::InstanceIndex: return spv::BuiltInInstanceIndex;
    case StageInputKind::FragCoord: return spv::BuiltInFragCoord;
    case StageInputKind::FrontFacing: return spv::BuiltInFrontFacing;
    case StageInputKind::HelperInvocation: return spv::BuiltInHelperInvocation;
    case StageInputKind::Layer: return spv::BuiltInLayer;
    case StageInputKind::SampleId: return spv::BuiltInSampleId;
    case StageInputKind::BaryCoordSmooth: return spv::BuiltInBaryCoordKHR;
    case StageInputKind::BaryCoordNoPerspective: return spv::BuiltInBaryCoordNoPerspKHR;
    case StageInputKind::WorkgroupId: return spv::BuiltInWorkgroupId;
    case StageInputKind::LocalInvocationId: return spv::BuiltInLocalInvocationId;
    case StageInputKind::LocalInvocationIndex: return spv::BuiltInLocalInvocationIndex;
    case StageInputKind::GlobalInvocationId: return spv::BuiltInGlobalInvocationId;
    default: return NoBuiltIn;
    }
}

std::uint32_t PerVertexType(SpirvEmitterState& state) {
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}},
         {spv::OpDecorate, {spv::DecorationBlock}}},
        TypeF32Vector(state, 4u));
}

std::uint32_t SampleMaskArrayType(SpirvEmitterState& state) {
    return state.module.Type(spv::OpTypeArray, TypeI32(state), ConstantU32(state, 1u));
}

std::uint32_t F32ArrayType(SpirvEmitterState& state, std::uint32_t count) {
    return state.module.Type(spv::OpTypeArray, TypeF32(state), ConstantU32(state, count));
}

std::uint32_t StorageBufferBlockType(SpirvEmitterState& state) {
    const auto array = state.module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, TypeU32(state));
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpDecorate, {spv::DecorationBlock}},
         {spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}}},
        array);
}

std::uint32_t StorageBufferU64BlockType(SpirvEmitterState& state) {
    const auto array = state.module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 8u}}}, TypeScalarU64(state));
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpDecorate, {spv::DecorationBlock}},
         {spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}}},
        array);
}

std::uint32_t PushConstantArrayType(SpirvEmitterState& state) {
    const auto count = ConstantU32(state, PushData::DwordCount);
    return state.module.DecoratedType(spv::OpTypeArray,
        {{spv::OpDecorate, {spv::DecorationArrayStride, static_cast<std::uint32_t>(sizeof(std::uint32_t))}}},
        TypeU32(state), count);
}

std::uint32_t PushConstantBlockType(SpirvEmitterState& state) {
    return state.module.DecoratedType(spv::OpTypeStruct,
        {{spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}},
         {spv::OpDecorate, {spv::DecorationBlock}}},
        PushConstantArrayType(state));
}

}

void CheckBindings(const IrProgram& program, const CompiledBindingLayout& bindings) {
    const IrProgramMetadata& metadata = program.Metadata();
    if (!metadata.bindingLayoutComplete) {
        FailEmit("shader binding layout has not been allocated for this program");
    }
    if (!(bindings.layout == metadata.bindings)) {
        FailEmit("binding allocation result does not match the program's committed binding layout");
    }
}

void EmitBaseHeader(SpirvModule& module, const IrProgram& program) {
    module.EmitCapability(spv::CapabilityShader);
    const bool physical = program.Info().usesDma || program.Resources().stage == IrShaderStage::Mesh;
    if (physical) {
        module.EmitCapability(spv::CapabilityInt64);
        module.EmitCapability(spv::CapabilityPhysicalStorageBufferAddresses);
        module.EmitExtension("SPV_KHR_physical_storage_buffer");
    }
    if (program.Info().usesDma) {
        module.EmitCapability(spv::CapabilityStorageBuffer8BitAccess);
        module.EmitExtension("SPV_KHR_8bit_storage");
    }
    module.AddMemoryModel(physical ? spv::AddressingModelPhysicalStorageBuffer64 : spv::AddressingModelLogical, spv::MemoryModelGLSL450);
}

void DefineInputs(SpirvEmitterState& state) {
    state.inputs.reserve(state.program.Info().inputs.size());
    for (const auto& input : state.program.Info().inputs) {
        state.inputs.push_back(SpirvInputBinding {input});
    }
    if (state.laneCount == 2u) {
        const auto addBuiltin = [&](StageInputKind kind, std::uint32_t components, const char* name) {
            if (std::none_of(state.inputs.begin(), state.inputs.end(), [kind](const SpirvInputBinding& input) {
                return input.kind == kind;
            })) {
                state.inputs.push_back(SpirvInputBinding {{kind, 0u, components, name, false}});
            }
        };
        addBuiltin(StageInputKind::LocalInvocationIndex, 1u, "gl_LocalInvocationIndex");
        if (std::any_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) {
            return input.kind == StageInputKind::GlobalInvocationId;
        })) {
            addBuiltin(StageInputKind::WorkgroupId, 3u, "gl_WorkGroupID");
        }
    }
    const bool pixelStage = state.program.Resources().stage == IrShaderStage::Pixel;
    for (auto& input : state.inputs) {
        if (pixelStage && input.kind == StageInputKind::Parameter) {
            const auto location = PixelParameterLocation(state, input.location);
            const auto shared = std::find_if(state.inputs.begin(), state.inputs.end(), [&](const SpirvInputBinding& other) {
                return &other != &input && other.kind == StageInputKind::Parameter && other.variableId != 0u && PixelParameterLocation(state, other.location) == location;
            });
            if (shared != state.inputs.end()) {
                if (shared->perVertex != input.perVertex) {
                    throw std::runtime_error("SPIR-V module emission failed: pixel inputs sharing parameter " + std::to_string(location) + " disagree on per-vertex access");
                }
                input.variableId = shared->variableId;
                continue;
            }
        }
        std::uint32_t type = TypeU32(state);
        switch (input.kind) {
        case StageInputKind::VertexIndex:
        case StageInputKind::InvocationId:
        case StageInputKind::PrimitiveId:
        case StageInputKind::InstanceIndex:
        case StageInputKind::Layer:
        case StageInputKind::SampleId:
            type = TypeI32(state);
            break;
        case StageInputKind::WorkgroupId:
        case StageInputKind::LocalInvocationId:
        case StageInputKind::GlobalInvocationId:
            type = TypeU32Vector(state, 3u);
            break;
        case StageInputKind::FragCoord:
            type = TypeF32Vector(state, 4u);
            break;
        case StageInputKind::TessCoord:
        case StageInputKind::BaryCoordSmooth:
        case StageInputKind::BaryCoordNoPerspective:
            type = TypeF32Vector(state, 3u);
            break;
        case StageInputKind::FrontFacing:
        case StageInputKind::HelperInvocation:
            type = TypeBool(state);
            break;
        case StageInputKind::Parameter:
            if (state.program.Resources().stage == IrShaderStage::Vertex || state.program.Resources().stage == IrShaderStage::Local) {
                type = VertexParameterScalarType(state, VertexParameterScalarKind(state, input.location));
                const auto components = VertexParameterComponentCount(input);
                if (components > 1u) {
                    type = state.module.Type(spv::OpTypeVector, type, components);
                }
            } else if (input.perVertex) {
                type = state.module.Type(spv::OpTypeArray, TypeF32Vector(state, 4u), ConstantU32(state, 3u));
            } else {
                type = TypeF32Vector(state, 4u);
            }
            break;
        default:
            break;
        }
        input.variableId = DefineInterfaceVariable(state, type, spv::StorageClassInput, input.debugName.c_str());
        if (input.kind == StageInputKind::Layer || input.kind == StageInputKind::SampleId) {
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationFlat);
        }
        if (input.kind == StageInputKind::Parameter) {
            const auto flat = PixelParameterIsFlat(state, input.location);
            if (input.perVertex) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationPerVertexKHR);
            } else if (flat) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationFlat);
            }
            if (!flat && !input.perVertex && PixelParameterIsLinear(state, input.location)) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationNoPerspective);
            }
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationLocation, PixelParameterLocation(state, input.location));
        } else if (const auto builtin = BuiltInForInput(input.kind); builtin != NoBuiltIn) {
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationBuiltIn, builtin);
        }
    }
    if (state.requirements.subgroupLocalInvocationId) {
        const auto variable = DefineInterfaceVariable(state, TypeU32(state), spv::StorageClassInput, "gl_SubgroupInvocationID");
        state.subgroupLocalInvocationIdVariable = variable;
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, spv::BuiltInSubgroupLocalInvocationId);
        if (state.program.Resources().stage == IrShaderStage::Pixel) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationFlat);
        }
    }
}

void DefineOutputs(SpirvEmitterState& state) {
    state.outputs.reserve(state.program.Info().outputs.size());
    std::uint32_t clipDistanceCount = 0u;
    std::uint32_t cullDistanceCount = 0u;
    for (const auto& output : state.program.Info().outputs) {
        state.outputs.push_back(SpirvOutputBinding {output});
        if (output.kind == StageOutputKind::ClipDistance) {
            clipDistanceCount = std::max(clipDistanceCount, output.index + 1u);
        } else if (output.kind == StageOutputKind::CullDistance) {
            cullDistanceCount = std::max(cullDistanceCount, output.index + 1u);
        }
    }
    if (state.program.Resources().stage == IrShaderStage::Mesh) {
        DefineMeshOutputs(state);
        return;
    }
    const auto BuiltIn = [&](std::uint32_t& variable, std::uint32_t type, const char* name, std::uint32_t builtin) {
        if (variable == 0u) {
            variable = DefineInterfaceVariable(state, type, spv::StorageClassOutput, name);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, builtin);
        }
        return variable;
    };
    for (auto& binding : state.outputs) {
        switch (binding.kind) {
        case StageOutputKind::Position:
            if (state.perVertexVariable == 0u) {
                const auto type = PerVertexType(state);
                state.module.AddName(type, "gl_PerVertex");
                state.perVertexVariable = DefineInterfaceVariable(state, type, spv::StorageClassOutput, "outPerVertex");
            }
            binding.variableId = state.perVertexVariable;
            break;
        case StageOutputKind::PointSize:
            binding.variableId = BuiltIn(state.pointSizeVariable, TypeF32(state), "gl_PointSize", spv::BuiltInPointSize);
            break;
        case StageOutputKind::ClipDistance:
            binding.variableId = BuiltIn(state.clipDistanceVariable, F32ArrayType(state, clipDistanceCount), "gl_ClipDistance", spv::BuiltInClipDistance);
            break;
        case StageOutputKind::CullDistance:
            binding.variableId = BuiltIn(state.cullDistanceVariable, F32ArrayType(state, cullDistanceCount), "gl_CullDistance", spv::BuiltInCullDistance);
            break;
        case StageOutputKind::Layer:
            binding.variableId = BuiltIn(state.layerVariable, TypeU32(state), "gl_Layer", spv::BuiltInLayer);
            break;
        case StageOutputKind::ViewportIndex:
            binding.variableId = BuiltIn(state.viewportIndexVariable, TypeU32(state), "gl_ViewportIndex", spv::BuiltInViewportIndex);
            break;
        case StageOutputKind::Depth:
            binding.variableId = BuiltIn(state.depthVariable, TypeF32(state), "gl_FragDepth", spv::BuiltInFragDepth);
            break;
        case StageOutputKind::SampleMask:
            binding.variableId = BuiltIn(state.sampleMaskVariable, SampleMaskArrayType(state), "gl_SampleMask", spv::BuiltInSampleMask);
            break;
        case StageOutputKind::Parameter:
        case StageOutputKind::Mrt: {
            const bool uintOutput = binding.kind == StageOutputKind::Mrt && state.program.Resources().stage == IrShaderStage::Pixel && binding.index < std::size(PixelInfo(state).targetOutputMode) && PixelInfo(state).targetOutputMode[binding.index] == 7u;
            const auto type = uintOutput ? TypeU32Vector(state, 4u) : TypeF32Vector(state, 4u);
            binding.variableId = DefineInterfaceVariable(state, type, spv::StorageClassOutput, binding.debugName.c_str());
            state.module.AddAnnotation(spv::OpDecorate, binding.variableId, spv::DecorationLocation, binding.location);
            break;
        }
        }
    }
}

void DefineDescriptors(SpirvEmitterState& state) {
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    const IrShaderStage stage = state.program.Resources().stage;
    if (layout.UsesPushData() || stage == IrShaderStage::Mesh) {
        const auto type = PushConstantBlockType(state);
        state.pushConstantVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPushConstant, type), spv::StorageClassPushConstant);
        state.module.AddName(type, "BufferResource");
        state.module.AddName(state.pushConstantVariable, "vsharp");
    }
    for (const IrDescriptorBinding& binding : layout.descriptors) {
        const auto Define = [&](std::uint32_t type, const char* name, std::uint32_t storage = spv::StorageClassStorageBuffer) {
            const auto variable = state.module.DefineGlobalVariable(TypePointer(state, storage, type), storage);
            state.module.AddName(variable, name);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, NativeBinding(stage, binding.kind));
            return variable;
        };
        const auto ArrayType = [&](std::uint32_t type) {
            const bool heap = binding.kind == DescriptorBindingKind::Samplers || ImageBindingResourceClass(binding.kind) != ImageResourceClass::None;
            const auto count = static_cast<std::uint32_t>(binding.resources.size());
            const auto length = heap ? state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::HeapCountBase + static_cast<std::uint32_t>(binding.kind), count) : ConstantU32(state, count);
            return state.module.Type(spv::OpTypeArray, type, length);
        };
        switch (binding.kind) {
        case DescriptorBindingKind::Buffers:
            state.storageBufferVariable = Define(ArrayType(StorageBufferBlockType(state)), "buffers");
            if (state.requirements.bufferInt64Atomics) {
                state.storageBufferU64Variable = Define(ArrayType(StorageBufferU64BlockType(state)), "buffers_u64");
                state.module.AddAnnotation(spv::OpDecorate, state.storageBufferVariable, spv::DecorationAliased);
                state.module.AddAnnotation(spv::OpDecorate, state.storageBufferU64Variable, spv::DecorationAliased);
            }
            if (state.requirements.coherentBuffers) {
                state.module.AddAnnotation(spv::OpDecorate, state.storageBufferVariable, spv::DecorationCoherent);
                if (state.storageBufferU64Variable != 0u) state.module.AddAnnotation(spv::OpDecorate, state.storageBufferU64Variable, spv::DecorationCoherent);
            }
            break;
        case DescriptorBindingKind::BdaPagetable:
            state.bdaPagetableVariable = Define(StorageBufferBlockType(state), "bda_pagetable");
            break;
        case DescriptorBindingKind::FaultBuffer:
            state.faultBufferVariable = Define(StorageBufferBlockType(state), "fault_buffer");
            break;
        case DescriptorBindingKind::ShaderData:
            state.shaderDataStorageVariable = Define(StorageBufferBlockType(state), "shader_data");
            break;
        case DescriptorBindingKind::FlattenedSrt:
            state.flattenedSrtVariable = Define(StorageBufferBlockType(state), "flattened_srt");
            break;
        case DescriptorBindingKind::Samplers:
            state.samplerVariable = Define(ArrayType(state.module.Type(spv::OpTypeSampler)), "samplers", spv::StorageClassUniformConstant);
            break;
        case DescriptorBindingKind::Gds:
            state.gdsVariable = Define(StorageBufferBlockType(state), "gds");
            break;
        default: {
            if (ImageBindingResourceClass(binding.kind) == ImageResourceClass::None) {
                FailEmit("descriptor binding has an unmapped image resource class");
            }
            const auto modes = ResourceMaterializer::RuntimeImageModes(state.program.Info().images.at(binding.resources.front()));
            const auto selected = std::ranges::find_if(modes, [&](const ImageResource& mode) { return DescriptorBindingForImage(mode) == binding.kind; });
            if (selected == modes.end()) FailEmit("static image heap has no runtime mode");
            const auto& image = *selected;
            const auto name = "image_" + std::to_string(static_cast<std::uint32_t>(binding.kind));
            state.imageVariables.at(ImageBindingIndex(binding.kind)) = Define(ArrayType(ImageType(state, image)), name.c_str(), spv::StorageClassUniformConstant);
            if (image.dimension == RdnaImageDimension::Dim1D || image.dimension == RdnaImageDimension::Dim1DArray) {
                state.module.EmitCapability(image.resourceClass == ImageResourceClass::Sampled ? spv::CapabilitySampled1D : spv::CapabilityImage1D);
            }
            break;
        }
        }
    }
}

}
