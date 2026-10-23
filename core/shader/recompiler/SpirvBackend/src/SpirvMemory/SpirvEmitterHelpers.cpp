#include "Optimization/ResourceMaterializer.hpp"
#include "BdaAbi.hpp"
#include "PipelineSpecialization.hpp"
#include "StencilReplay.hpp"
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

std::array<std::uint32_t, 3> WorkgroupLocalSize(const SpirvEmitterState& state, const ShaderWorkgroupInputInfo& workgroup) {
    const std::uint32_t derivativeDefault = state.requirements.computeDerivatives ? 2u : 1u;
    std::array<std::uint32_t, 3> size {
        workgroup.threadsNum[0] != 0u ? workgroup.threadsNum[0] : derivativeDefault,
        workgroup.threadsNum[1] != 0u ? workgroup.threadsNum[1] : derivativeDefault,
        workgroup.threadsNum[2] != 0u ? workgroup.threadsNum[2] : 1u};
    if (state.spannedWave) {
        size = {64u, 1u, 1u};
    }
    if (state.laneCount == 2) {
        size = {((size[0] * size[1] * size[2] + 63u) / 64u) * 32u, 1u, 1u};
    }
    return size;
}

namespace {

constexpr std::uint32_t NoBuiltIn = std::numeric_limits<std::uint32_t>::max();

void RequireTableIndexing(SpirvEmitterState& state) {
    if (state.spirvVersion < 0x00010500u) state.module.EmitExtension("SPV_EXT_descriptor_indexing");
    state.module.EmitCapability(spv::CapabilityRuntimeDescriptorArray);
    state.module.EmitCapability(spv::CapabilitySampledImageArrayDynamicIndexing);
}

std::uint32_t BuiltInForInput(StageInputKind kind) {
    switch (kind) {
    case StageInputKind::VertexIndex: return spv::BuiltInVertexIndex;
    case StageInputKind::InvocationId: return spv::BuiltInInvocationId;
    case StageInputKind::PrimitiveId: return spv::BuiltInPrimitiveId;
    case StageInputKind::TessCoord: return spv::BuiltInTessCoord;
    case StageInputKind::InstanceIndex: return spv::BuiltInInstanceIndex;
    case StageInputKind::BaseInstance: return spv::BuiltInBaseInstance;
    case StageInputKind::FragCoord: return spv::BuiltInFragCoord;
    case StageInputKind::FrontFacing: return spv::BuiltInFrontFacing;
    case StageInputKind::HelperInvocation: return spv::BuiltInHelperInvocation;
    case StageInputKind::Layer: return spv::BuiltInLayer;
    case StageInputKind::SampleId: return spv::BuiltInSampleId;
    case StageInputKind::SampleMask: return spv::BuiltInSampleMask;
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
         {spv::OpMemberDecorate, {0u, spv::DecorationInvariant}},
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
        {{spv::OpMemberDecorate, {0u, spv::DecorationOffset, state.program.Metadata().bindings.PushSlotDword() * 4u}},
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
    if (state.laneCount == 2u || state.spannedWave) {
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
    if (state.requirements.ldsLock && std::none_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) {
        return input.kind == StageInputKind::LocalInvocationIndex;
    })) {
        state.inputs.push_back(SpirvInputBinding {{StageInputKind::LocalInvocationIndex, 0u, 1u, "gl_LocalInvocationIndex", false}});
    }
    if (LdsInDeviceMemory(state)) {
        if (std::none_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) {
            return input.kind == StageInputKind::WorkgroupId;
        })) {
            state.inputs.push_back(SpirvInputBinding {{StageInputKind::WorkgroupId, 0u, 3u, "gl_WorkGroupID", false}});
        }
        state.numWorkgroupsVariable = DefineInterfaceVariable(state, TypeU32Vector(state, 3u), spv::StorageClassInput, "gl_NumWorkGroups");
        state.module.AddAnnotation(spv::OpDecorate, state.numWorkgroupsVariable, spv::DecorationBuiltIn, spv::BuiltInNumWorkgroups);
    }
    const bool pixelStage = state.program.Resources().stage == IrShaderStage::Pixel;
    const bool emulated = pixelStage && state.program.Metadata().barycentricEmulation;
    BarycentricEmulationLayout emulation;
    if (emulated) {
        const auto parameters = DescribeFragmentParameters(state.program, state.inputInfo);
        const auto reads = [&](StageInputKind kind) {
            return std::any_of(state.inputs.begin(), state.inputs.end(), [&](const SpirvInputBinding& input) { return input.kind == kind; });
        };
        emulation = LayoutBarycentricEmulation(parameters, {true, reads(StageInputKind::BaryCoordSmooth), reads(StageInputKind::BaryCoordNoPerspective), state.program.Info().smoothOffsetBarycentrics}, state.meshBarycentricPixel);
        if (state.meshBarycentricPixel && emulation.positionsLocation != BarycentricEmulationLayout::NoLocation) {
            state.pixelFlatPositions = DefineInterfaceVariable(state, state.module.Type(spv::OpTypeArray, TypeF32Vector(state, 4u), ConstantU32(state, 3u)), spv::StorageClassInput, "meshBarycentricPositions");
            state.module.AddAnnotation(spv::OpDecorate, state.pixelFlatPositions, spv::DecorationLocation, emulation.positionsLocation);
            state.module.AddAnnotation(spv::OpDecorate, state.pixelFlatPositions, spv::DecorationPerPrimitiveEXT);
            state.module.EmitCapability(spv::CapabilityMeshShadingEXT);
            state.module.EmitExtension("SPV_EXT_mesh_shader");
        }
        if (emulation.reciprocalWLocation != BarycentricEmulationLayout::NoLocation) {
            state.barycentricReciprocalWVariable = DefineInterfaceVariable(state, TypeF32Vector(state, 3u), spv::StorageClassInput, "gl_BarycentricReciprocalW");
            state.module.AddAnnotation(spv::OpDecorate, state.barycentricReciprocalWVariable, spv::DecorationNoPerspective);
            state.module.AddAnnotation(spv::OpDecorate, state.barycentricReciprocalWVariable, spv::DecorationLocation, emulation.reciprocalWLocation);
        }
    }
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
        case StageInputKind::BaseInstance:
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
        case StageInputKind::SampleMask:
            type = SampleMaskArrayType(state);
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
        if (input.kind == StageInputKind::Layer || input.kind == StageInputKind::SampleId || input.kind == StageInputKind::SampleMask) {
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationFlat);
        }
        if (input.kind == StageInputKind::Parameter) {
            const auto flat = PixelParameterIsFlat(state, input.location);
            auto location = PixelParameterLocation(state, input.location);
            if (input.perVertex && emulated) {
                const auto relocated = std::find_if(emulation.perVertexLocations.begin(), emulation.perVertexLocations.end(), [&](const auto& entry) { return entry.first == location; });
                if (relocated == emulation.perVertexLocations.end()) {
                    throw std::runtime_error("SPIR-V module emission failed: per-vertex parameter " + std::to_string(location) + " has no emulated location");
                }
                location = relocated->second;
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, state.meshBarycentricPixel ? spv::DecorationPerPrimitiveEXT : spv::DecorationFlat);
            } else if (input.perVertex) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationPerVertexKHR);
            } else if (flat) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationFlat);
            }
            if (!flat && !input.perVertex && PixelParameterIsLinear(state, input.location)) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationNoPerspective);
            }
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationLocation, location);
        } else if (emulated && (input.kind == StageInputKind::BaryCoordSmooth || input.kind == StageInputKind::BaryCoordNoPerspective)) {
            const bool linear = input.kind == StageInputKind::BaryCoordNoPerspective;
            if (linear) {
                state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationNoPerspective);
            }
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationLocation, linear ? emulation.linearLocation : emulation.smoothLocation);
        } else if (const auto builtin = BuiltInForInput(input.kind); builtin != NoBuiltIn) {
            state.module.AddAnnotation(spv::OpDecorate, input.variableId, spv::DecorationBuiltIn, builtin);
        }
    }
    if (state.requirements.subgroupLocalInvocationId) {
        const auto variable = DefineInterfaceVariable(state, TypeU32(state), spv::StorageClassInput, "gl_SubgroupInvocationID");
        state.subgroupLocalInvocationIdVariable = variable;
        if (state.spannedWave) {
            state.subgroupIdVariable = DefineInterfaceVariable(state, TypeU32(state), spv::StorageClassInput, "gl_SubgroupID");
            state.module.AddAnnotation(spv::OpDecorate, state.subgroupIdVariable, spv::DecorationBuiltIn, spv::BuiltInSubgroupId);
        }
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, spv::BuiltInSubgroupLocalInvocationId);
        if (state.program.Resources().stage == IrShaderStage::Pixel) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationFlat);
        }
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->QuadSampleMaskPartial()) {
        const auto read = std::find_if(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) { return input.kind == StageInputKind::SampleMask && input.variableId != 0u; });
        if (read != state.inputs.end()) {
            state.sampleMaskInputVariable = read->variableId;
        } else {
            state.sampleMaskInputVariable = DefineInterfaceVariable(state, SampleMaskArrayType(state), spv::StorageClassInput, "gl_SampleMaskIn");
            state.module.AddAnnotation(spv::OpDecorate, state.sampleMaskInputVariable, spv::DecorationBuiltIn, spv::BuiltInSampleMask);
            state.module.AddAnnotation(spv::OpDecorate, state.sampleMaskInputVariable, spv::DecorationFlat);
        }
    }
}

constexpr std::uint32_t MaxClipDistances = 8u;

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
    const bool depthRemap = state.depthRangeRemap && state.program.Resources().stage == IrShaderStage::Vertex;
    if (depthRemap) {
        state.depthRemapClipIndex = clipDistanceCount;
        clipDistanceCount += 2u;
    }
    const auto stage = state.program.Resources().stage;
    const bool exportsPosition = std::ranges::any_of(state.outputs, [](const SpirvOutputBinding& output) { return output.kind == StageOutputKind::Position; });
    if (exportsPosition && (stage == IrShaderStage::Vertex || stage == IrShaderStage::TessellationEvaluation) && clipDistanceCount + cullDistanceCount < MaxClipDistances) {
        if (std::ranges::find(state.supportedCapabilities, spv::CapabilityClipDistance) == state.supportedCapabilities.end()) {
            FailEmit("clipping a primitive with an all-zero clip position needs shaderClipDistance on the target device");
        }
        state.zeroPositionClipIndex = clipDistanceCount;
        clipDistanceCount += 1u;
    }
    const auto BuiltIn = [&](std::uint32_t& variable, std::uint32_t type, const char* name, std::uint32_t builtin) {
        if (variable == 0u) {
            variable = DefineInterfaceVariable(state, type, spv::StorageClassOutput, name);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, builtin);
        }
        return variable;
    };
    if (depthRemap) BuiltIn(state.clipDistanceVariable, F32ArrayType(state, clipDistanceCount), "gl_ClipDistance", spv::BuiltInClipDistance);
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
        case StageOutputKind::StencilRef:
            if (std::ranges::find(state.supportedCapabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT)) == state.supportedCapabilities.end()) {
                const auto values = StencilReferenceValues(state.program, PixelInfo(state).psStencilOperationValueExport ? 8u : 0u);
                if (values.empty()) throw std::runtime_error("stencil reference export emulation found no stencil export");
                if (!StencilReferenceDefinedForKeptInvocations(state.program)) throw std::runtime_error("stencil reference export emulation requires a stencil export on every kept path");
                if (PixelInfo(state).psEarlyZ || PixelInfo(state).psOrderedPixelShader || !StencilReplaySafe(state.program)) throw std::runtime_error("stencil reference export emulation requires a replay-safe pixel shader with late tests");
                state.stencilReplay = true;
                state.stencilRefVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, TypeI32(state)), spv::StorageClassPrivate);
                if (state.spirvVersion >= 0x00010400u) state.interfaceVariables.push_back(state.stencilRefVariable);
                binding.variableId = state.stencilRefVariable;
            } else {
                binding.variableId = BuiltIn(state.stencilRefVariable, TypeI32(state), "gl_FragStencilRefARB", spv::BuiltInFragStencilRefEXT);
            }
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
    if (state.zeroPositionClipIndex) {
        BuiltIn(state.clipDistanceVariable, F32ArrayType(state, clipDistanceCount), "gl_ClipDistance", spv::BuiltInClipDistance);
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->psAlphaToCoverage) {
        if (state.inputInfo.pixel->psSampleMaskExportEnable) throw std::runtime_error("alpha-to-coverage with a sample-mask export is unsupported");
        BuiltIn(state.sampleMaskVariable, SampleMaskArrayType(state), "gl_SampleMask", spv::BuiltInSampleMask);
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->QuadSampleMaskPartial()) {
        BuiltIn(state.sampleMaskVariable, SampleMaskArrayType(state), "gl_SampleMask", spv::BuiltInSampleMask);
    }
}

void DefineDescriptors(SpirvEmitterState& state) {
    const IrBindingLayout& layout = state.program.Metadata().bindings;
    const IrShaderStage stage = state.program.Resources().stage;
    if (layout.UsesPushData() || state.stencilReplay || stage == IrShaderStage::Mesh || stage == IrShaderStage::TessellationControl || (state.depthRangeRemap && stage == IrShaderStage::Vertex)) {
        const auto type = PushConstantBlockType(state);
        state.pushConstantVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPushConstant, type), spv::StorageClassPushConstant);
        state.module.AddName(type, "BufferResource");
        state.module.AddName(state.pushConstantVariable, "vsharp");
    }
    for (const IrDescriptorBinding& binding : layout.descriptors) {
        const auto Define = [&](std::uint32_t type, const char* name, std::uint32_t storage = spv::StorageClassStorageBuffer) {
            const auto variable = state.module.DefineGlobalVariable(TypePointer(state, storage, type), storage);
            state.module.AddName(variable, name);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, state.fragmentDescriptorSet && stage == IrShaderStage::Pixel ? RuntimeAbi::FragmentDescriptorSet : RuntimeAbi::DescriptorSet);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, NativeBinding(stage, binding.kind));
            return variable;
        };
        const auto ArrayType = [&](std::uint32_t type) {
            const bool heap = binding.kind == DescriptorBindingKind::Samplers || ImageBindingResourceClass(binding.kind) != ImageResourceClass::None;
            const auto count = static_cast<std::uint32_t>(binding.resources.size());
            const auto length = heap && !state.registrationModules ? state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::HeapCountBase + static_cast<std::uint32_t>(binding.kind), count) : ConstantU32(state, count);
            return state.module.Type(spv::OpTypeArray, type, length);
        };
        switch (binding.kind) {
        case DescriptorBindingKind::Buffers:
            state.storageBufferVariable = Define(ArrayType(StorageBufferBlockType(state)), "buffers");
            if (state.requirements.coherentBufferAlias) state.storageBufferCoherentVariable = Define(ArrayType(StorageBufferBlockType(state)), "buffers_coherent");
            if (state.requirements.bufferU64View) state.storageBufferU64Variable = Define(ArrayType(StorageBufferU64BlockType(state)), "buffers_u64");
            if (state.storageBufferU64Variable != 0u || state.storageBufferCoherentVariable != 0u) {
                for (const auto variable : {state.storageBufferVariable, state.storageBufferCoherentVariable, state.storageBufferU64Variable}) {
                    if (variable != 0u) state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationAliased);
                }
            }
            if (state.requirements.coherentBuffers) {
                state.module.AddAnnotation(spv::OpDecorate, state.storageBufferCoherentVariable != 0u ? state.storageBufferCoherentVariable : state.storageBufferVariable, spv::DecorationCoherent);
                if (state.storageBufferU64Variable != 0u) state.module.AddAnnotation(spv::OpDecorate, state.storageBufferU64Variable, spv::DecorationCoherent);
            }
            break;
        case DescriptorBindingKind::BdaPagetable:
            state.bdaPagetableVariable = Define(StorageBufferBlockType(state), "bda_pagetable");
            state.module.AddAnnotation(spv::OpDecorate, state.bdaPagetableVariable, spv::DecorationNonWritable);
            break;
        case DescriptorBindingKind::FaultBuffer:
            state.faultBufferVariable = Define(StorageBufferBlockType(state), "fault_buffer");
            break;
        case DescriptorBindingKind::ShaderData:
            state.shaderDataStorageVariable = Define(StorageBufferBlockType(state), "shader_data");
            break;
        case DescriptorBindingKind::FlattenedSrt:
            state.flattenedSrtVariable = Define(StorageBufferBlockType(state), "flattened_srt");
            state.module.AddAnnotation(spv::OpDecorate, state.flattenedSrtVariable, spv::DecorationNonWritable);
            break;
        case DescriptorBindingKind::Samplers:
            state.samplerVariable = Define(ArrayType(state.module.Type(spv::OpTypeSampler)), "samplers", spv::StorageClassUniformConstant);
            break;
        case DescriptorBindingKind::Gds:
            state.gdsVariable = Define(StorageBufferBlockType(state), "gds");
            state.module.AddAnnotation(spv::OpDecorate, state.gdsVariable, spv::DecorationCoherent);
            break;
        case DescriptorBindingKind::ImageTable:
        case DescriptorBindingKind::ImageStorageTable:
            RequireTableIndexing(state);
            if (binding.kind == DescriptorBindingKind::ImageStorageTable) state.module.EmitCapability(spv::CapabilityStorageImageArrayDynamicIndexing);
            for (std::uint32_t resource = 0; resource < state.program.Info().images.size(); ++resource) {
                const auto& image = state.program.Info().images[resource];
                if (image.table == NoTable) continue;
                for (const auto& mode : state.program.Info().runtimeImageModes.at(resource)) {
                    if (!TableModeEmitted(mode)) continue;
                    const auto kind = DescriptorBindingForImage(mode);
                    if ((ImageBindingResourceClass(kind) == ImageResourceClass::Storage) != (binding.kind == DescriptorBindingKind::ImageStorageTable)) continue;
                    if (std::any_of(state.tableImageVariables.begin(), state.tableImageVariables.end(), [&](const SpirvTableImageVariable& variable) { return variable.kind == kind; })) continue;
                    const auto name = "table_images_" + std::to_string(static_cast<std::uint32_t>(kind));
                    state.tableImageVariables.push_back({kind, Define(state.module.Type(spv::OpTypeRuntimeArray, ImageType(state, mode)), name.c_str(), spv::StorageClassUniformConstant)});
                    if (mode.dimension == RdnaImageDimension::Dim1D || mode.dimension == RdnaImageDimension::Dim1DArray) state.module.EmitCapability(binding.kind == DescriptorBindingKind::ImageStorageTable ? spv::CapabilityImage1D : spv::CapabilitySampled1D);
                }
            }
            break;
        case DescriptorBindingKind::SamplerTable:
            RequireTableIndexing(state);
            state.tableSamplerVariable = Define(state.module.Type(spv::OpTypeRuntimeArray, state.module.Type(spv::OpTypeSampler)), "table_samplers", spv::StorageClassUniformConstant);
            break;
        case DescriptorBindingKind::ImageTableMap:
            state.tableMapVariable = Define(StorageBufferBlockType(state), "table_map");
            state.module.AddAnnotation(spv::OpDecorate, state.tableMapVariable, spv::DecorationNonWritable);
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
    if (LdsInDeviceMemory(state)) {
        state.ldsBufferVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassStorageBuffer, StorageBufferBlockType(state)), spv::StorageClassStorageBuffer);
        state.module.AddName(state.ldsBufferVariable, "workgroup_memory");
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationDescriptorSet, WorkgroupMemoryDescriptorSet);
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationBinding, 0u);
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationCoherent);
        if (state.requirements.ldsBufferInt64Atomics) {
            state.ldsBufferU64Variable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassStorageBuffer, StorageBufferU64BlockType(state)), spv::StorageClassStorageBuffer);
            state.module.AddName(state.ldsBufferU64Variable, "workgroup_memory_u64");
            state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferU64Variable, spv::DecorationDescriptorSet, WorkgroupMemoryDescriptorSet);
            state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferU64Variable, spv::DecorationBinding, 0u);
            state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferU64Variable, spv::DecorationCoherent);
            for (const auto variable : {state.ldsBufferVariable, state.ldsBufferU64Variable}) state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationAliased);
        }
    }
    if (state.pixelFunctionLdsBuffer) {
        state.ldsBufferVariable = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassStorageBuffer, StorageBufferBlockType(state)), spv::StorageClassStorageBuffer);
        state.module.AddName(state.ldsBufferVariable, "pixel_function_lds");
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationDescriptorSet, PixelFunctionLdsDescriptorSet);
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationBinding, 0u);
        state.module.AddAnnotation(spv::OpDecorate, state.ldsBufferVariable, spv::DecorationCoherent);
        if (state.spirvVersion >= 0x00010400u) state.interfaceVariables.push_back(state.ldsBufferVariable);
    }
}

}
