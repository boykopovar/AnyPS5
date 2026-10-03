#include "SpirvBackend/SpirvMemory/SpirvModuleSetup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{
namespace {

[[noreturn]] void FailEmit(const std::string& reason) {
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

}


void EmitModuleHeader(SpirvModule& module, const IrProgram& program, const BindingAllocationResult& bindings) {
    CheckBindings(program, bindings);
    EmitBaseHeader(module, program);
}

void EmitModuleHeader(SpirvEmitterState& state, const BindingAllocationResult& bindings) {
    CheckBindings(state.program, bindings);
    DefineModule(state);
}

std::uint32_t ExecutionModelForStage(IrShaderStage stage) {
    switch (stage) {
    case IrShaderStage::Local:
    case IrShaderStage::Vertex: return spv::ExecutionModelVertex;
    case IrShaderStage::TessellationControl: return spv::ExecutionModelTessellationControl;
    case IrShaderStage::TessellationEvaluation: return spv::ExecutionModelTessellationEvaluation;
    case IrShaderStage::Mesh: return spv::ExecutionModelMeshEXT;
    case IrShaderStage::Pixel: return spv::ExecutionModelFragment;
    case IrShaderStage::Compute: return spv::ExecutionModelGLCompute;
    default: FailEmit("shader stage has no execution model");
    }
}

std::uint32_t DefineInterfaceVariable(SpirvEmitterState& state, std::uint32_t type, std::uint32_t storage, const char* name) {
    const auto variable = state.module.DefineGlobalVariable(TypePointer(state, storage, type), storage);
    state.interfaceVariables.push_back(variable);
    state.module.AddName(variable, name);
    return variable;
}

void DefineModule(SpirvEmitterState& state) {
    state.interfaceVariables.reserve(state.program.Info().inputs.size() + state.program.Info().outputs.size());
    DefineInputs(state);
    DefineOutputs(state);
    DefineTessellationInterfaces(state);
    DefineDescriptors(state);
    if (state.requirements.functionLds) {
        state.ldsVariable = state.module.AllocateId();
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.scratchVariable.at(half) = state.module.AllocateId();
        }
    }
    state.mainFunc = state.module.AllocateId();
    if (StageOf(state) == IrShaderStage::Mesh) {
        const auto& mesh = VertexInfo(state).mesh;
        state.meshGuestFunc = state.module.AllocateId();
        state.module.EmitCapability(spv::CapabilityMeshShadingEXT);
        state.module.EmitExtension("SPV_EXT_mesh_shader");
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOutputTrianglesEXT);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOutputVertices, mesh.maxVertices);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOutputPrimitivesEXT, mesh.maxPrimitives);
    }
    if (StageOf(state) == IrShaderStage::TessellationControl || StageOf(state) == IrShaderStage::TessellationEvaluation) {
        DefineTessellationExecutionModes(state);
    }
    state.entryLabel = state.module.AllocateId();
    EmitBaseHeader(state.module, state.program);
    for (const auto capability : state.requirements.capabilities) {
        state.module.EmitCapability(capability);
    }
    for (const auto& extension : state.requirements.extensions) {
        state.module.EmitExtension(extension);
    }
    if (state.requirements.bufferInt64Atomics) {
        state.module.EmitCapability(spv::CapabilityInt64);
        state.module.EmitCapability(spv::CapabilityInt64Atomics);
    }
    if (state.clipDistanceVariable != 0) {
        state.module.EmitCapability(spv::CapabilityClipDistance);
    }
    if (state.cullDistanceVariable != 0) {
        state.module.EmitCapability(spv::CapabilityCullDistance);
    }
    if (state.layerVariable != 0 || InputVariableForKind(state, StageInputKind::Layer) != 0) {
        state.module.RequireVersion(0x00010500u);
        state.module.EmitCapability(spv::CapabilityShaderLayer);
    }
    if (state.viewportIndexVariable != 0) {
        state.module.RequireVersion(0x00010500u);
        state.module.EmitCapability(spv::CapabilityShaderViewportIndex);
    }
    if (InputVariableForKind(state, StageInputKind::SampleId) != 0) {
        state.module.EmitCapability(spv::CapabilitySampleRateShading);
    }
    if (state.requirements.imageGatherExtended) {
        state.module.EmitCapability(spv::CapabilityImageGatherExtended);
    }
    if (state.laneCount == 2 || state.requirements.subgroupBallot || state.requirements.subgroupShuffle || state.requirements.subgroupLocalInvocationId) {
        state.module.EmitCapability(spv::CapabilityGroupNonUniform);
    }
    if (state.laneCount == 2 || state.requirements.subgroupBallot) {
        state.module.EmitCapability(spv::CapabilityGroupNonUniformBallot);
    }
    if (state.requirements.subgroupShuffle) {
        state.module.EmitCapability(spv::CapabilityGroupNonUniformShuffle);
    }
    if (state.requirements.computeDerivatives && StageOf(state) == IrShaderStage::Compute) {
        state.module.EmitCapability(spv::CapabilityComputeDerivativeGroupQuadsKHR);
        state.module.EmitExtension("SPV_KHR_compute_shader_derivatives");
    }
    const bool fragmentBarycentric = StageOf(state) == IrShaderStage::Pixel && std::any_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) {
        return input.perVertex || input.kind == StageInputKind::BaryCoordSmooth || input.kind == StageInputKind::BaryCoordNoPerspective;
    });
    if (fragmentBarycentric) {
        state.module.EmitCapability(spv::CapabilityFragmentBarycentricKHR);
        state.module.EmitExtension("SPV_KHR_fragment_shader_barycentric");
    }
    state.module.EmitCapability(spv::CapabilitySignedZeroInfNanPreserve);
    state.module.EmitExtension("SPV_KHR_float_controls");
    state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSignedZeroInfNanPreserve, 32u);
    if (state.requirements.float64) {
        if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityFloat64)) == state.supportedCapabilities.end()) {
            throw std::runtime_error("64-bit float instructions need the Float64 capability, which the device lacks");
        }
        state.module.EmitCapability(spv::CapabilityFloat64);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSignedZeroInfNanPreserve, 64u);
    }
    if (const auto* workgroup = ShaderWorkgroupInput(state)) {
        const std::uint32_t derivativeDefault = state.requirements.computeDerivatives ? 2u : 1u;
        std::uint32_t localX = workgroup->threadsNum[0] != 0u ? workgroup->threadsNum[0] : derivativeDefault;
        std::uint32_t localY = workgroup->threadsNum[1] != 0u ? workgroup->threadsNum[1] : derivativeDefault;
        std::uint32_t localZ = workgroup->threadsNum[2] != 0u ? workgroup->threadsNum[2] : 1u;
        if (state.laneCount == 2) {
            localX = ((localX * localY * localZ + 63u) / 64u) * 32u;
            localY = 1u;
            localZ = 1u;
            if (state.requirements.computeDerivatives) {
                localY = localX / 2u;
                localX = 2u;
            }
        }
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeLocalSize, localX, localY, localZ);
    }
    if (StageOf(state) == IrShaderStage::Pixel) {
        const auto& pixel = PixelInfo(state);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOriginUpperLeft);
        if (state.depthVariable != 0) {
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDepthReplacing);
        }
        if (pixel.psEarlyZ && !pixel.psPixelKillEnable && !pixel.psDepthExportEnable && !pixel.psSampleMaskExportEnable) {
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeEarlyFragmentTests);
        }
    }
    if (state.requirements.computeDerivatives && StageOf(state) == IrShaderStage::Compute) {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDerivativeGroupQuadsKHR);
    }
    state.module.AddName(state.mainFunc, "main");
    if (state.requirements.functionLds) {
        state.module.AddName(state.ldsVariable, "lds_dwords");
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddName(state.scratchVariable.at(half), "scratch_dwords");
        }
    }
}

}
