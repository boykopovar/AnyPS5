#include "SpirvBackend/SpirvMemory/SpirvModuleSetup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler
{

void EmitModuleHeader(SpirvModule& module, const IrProgram& program, const CompiledBindingLayout& bindings) {
    CheckBindings(program, bindings);
    EmitBaseHeader(module, program);
}

void EmitModuleHeader(SpirvEmitterState& state, const CompiledBindingLayout& bindings) {
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

namespace {

class WindowIndexBounds {
public:
    std::optional<std::uint64_t> Of(const IrValue& input) {
        const IrValue* value = input.Resolve();
        if (value->HasImmediate()) return value->ImmediateU32();
        if (const auto found = memo.find(value); found != memo.end()) return found->second;
        if (!active.insert(value).second || active.size() > 256u) return std::nullopt;
        const auto bound = compute(*value);
        active.erase(value);
        memo.emplace(value, bound);
        return bound;
    }

private:
    using Bound = std::optional<std::uint64_t>;

    Bound maxOf(std::initializer_list<Bound> bounds) {
        std::uint64_t result = 0;
        for (const auto& bound : bounds) {
            if (!bound) return std::nullopt;
            result = std::max(result, *bound);
        }
        return result;
    }

    static std::vector<const IrValue*> reachingStores(const IrValue& load, std::uint32_t slot) {
        std::vector<const IrValue*> values;
        std::unordered_set<const IrBlock*> visited;
        std::vector<std::pair<IrBlock*, const IrValue*>> pending {{load.Parent(), &load}};
        while (!pending.empty()) {
            const auto [block, before] = pending.back();
            pending.pop_back();
            const auto& instructions = block->Instructions();
            auto it = instructions.rbegin();
            if (before != nullptr) it = std::next(std::find(instructions.rbegin(), instructions.rend(), before));
            bool stored = false;
            for (; it != instructions.rend() && !stored; ++it) {
                if ((*it)->Opcode() != IrOpcode::StoreVectorWindow) continue;
                const IrValue* index = (*it)->Argument(0)->Resolve();
                if (!index->HasImmediate() || index->ImmediateU32() != slot) continue;
                values.push_back((*it)->Argument(1));
                stored = true;
            }
            if (stored) continue;
            for (IrBlock* predecessor : block->Predecessors()) {
                if (visited.insert(predecessor).second) pending.emplace_back(predecessor, nullptr);
            }
        }
        return values;
    }

    Bound compute(const IrValue& value) {
        const auto arg = [&](std::size_t index) { return Of(*value.Argument(index)); };
        const auto immediate = [&](std::size_t index) -> std::optional<std::uint32_t> {
            const IrValue* operand = value.Argument(index)->Resolve();
            if (!operand->HasImmediate()) return std::nullopt;
            return operand->ImmediateU32();
        };
        const auto fits = [](std::uint64_t bound) -> Bound { return bound <= 0xffffffffull ? Bound(bound) : std::nullopt; };
        switch (value.Opcode()) {
        case IrOpcode::BitFieldUExtract: {
            const auto count = immediate(2);
            if (count && *count < 32u) return (std::uint64_t {1} << *count) - 1u;
            return arg(0);
        }
        case IrOpcode::BitwiseAnd32: {
            const auto lhs = arg(0);
            const auto rhs = arg(1);
            if (lhs && rhs) return std::min(*lhs, *rhs);
            return lhs ? lhs : rhs;
        }
        case IrOpcode::UMin32: {
            const auto lhs = arg(0);
            const auto rhs = arg(1);
            if (!lhs || !rhs) return std::nullopt;
            return std::min(*lhs, *rhs);
        }
        case IrOpcode::UMax32:
            return maxOf({arg(0), arg(1)});
        case IrOpcode::BitwiseOr32: {
            const auto both = maxOf({arg(0), arg(1)});
            if (!both) return std::nullopt;
            std::uint64_t mask = 0;
            while (mask < *both) mask = mask << 1u | 1u;
            return mask;
        }
        case IrOpcode::IAdd32: {
            const auto lhs = arg(0);
            const auto rhs = arg(1);
            if (!lhs || !rhs) return std::nullopt;
            return fits(*lhs + *rhs);
        }
        case IrOpcode::ShiftRightLogical32: {
            const auto shift = immediate(1);
            const auto bound = arg(0);
            if (!bound) return std::nullopt;
            return shift ? *bound >> (*shift & 31u) : *bound;
        }
        case IrOpcode::ShiftLeftLogical32: {
            const auto shift = immediate(1);
            const auto bound = arg(0);
            if (!bound || !shift) return std::nullopt;
            return fits(*bound << (*shift & 31u));
        }
        case IrOpcode::ConvertU32U16:
            return 0xffffu;
        case IrOpcode::Select:
        case IrOpcode::SelectU32:
        {
            if (value.Argument(0)->Resolve()->Opcode() == IrOpcode::GetExec || value.Argument(2)->Resolve()->Opcode() == IrOpcode::LoadVectorWindow) return arg(1);
            const auto lhs = arg(1);
            const auto rhs = arg(2);
            if (lhs && rhs) return std::max(*lhs, *rhs);
            return lhs ? lhs : rhs;
        }
        case IrOpcode::ReadFirstLane:
            return arg(0);
        case IrOpcode::Identity:
        case IrOpcode::Phi: {
            Bound bound;
            for (std::size_t index = 0; index < value.ArgumentCount(); ++index) {
                if (const auto incoming = arg(index)) bound = std::max(bound.value_or(0u), *incoming);
            }
            return bound;
        }
        case IrOpcode::LoadVectorWindow: {
            const IrValue* index = value.Argument(0)->Resolve();
            if (!index->HasImmediate()) return std::nullopt;
            Bound bound = 0u;
            for (const IrValue* stored : reachingStores(value, index->ImmediateU32())) {
                bound = maxOf({bound, Of(*stored)});
                if (!bound) return std::nullopt;
            }
            return bound;
        }
        default:
            return std::nullopt;
        }
    }

    std::unordered_map<const IrValue*, Bound> memo;
    std::unordered_set<const IrValue*> active;
};

constexpr std::uint64_t MaxNarrowedSwitchCases = 4096;

std::uint32_t IndexedWindowSlots(const IrProgram& program, std::uint32_t laneCount) {
    const auto window = program.Metadata().vectorWindowSize;
    WindowIndexBounds bounds;
    std::uint64_t reached = 0;
    std::uint64_t dynamicAccesses = 0;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::LoadVectorWindow && inst->Opcode() != IrOpcode::StoreVectorWindow) continue;
            const IrValue* index = inst->Argument(0)->Resolve();
            if (index->HasImmediate()) continue;
            ++dynamicAccesses;
            const auto bound = bounds.Of(*index);
            if (bound && *bound < window / 2u) reached = std::max(reached, *bound + 1u);
        }
    }
    if (reached == 0u || dynamicAccesses * (window - reached) * laneCount > MaxNarrowedSwitchCases) return 0u;
    return static_cast<std::uint32_t>(reached);
}

bool WritesGuestMemory(const IrProgram& program) {
    const auto& info = program.Info();
    const auto writtenImage = [](const ImageResource& image) { return !image.targetMerge && (image.written || image.atomic || image.atomic64); };
    const auto writtenModes = [&](const std::vector<ImageResource>& modes) { return std::ranges::any_of(modes, writtenImage); };
    const auto writtenBuffer = [](const BufferResource& buffer) { return buffer.written || buffer.atomic; };
    if (info.bdaWrites || std::ranges::any_of(info.buffers, writtenBuffer) || std::ranges::any_of(info.images, writtenImage) || std::ranges::any_of(info.runtimeImageModes, writtenModes)) return true;
    const auto& memory = program.Resources().memoryInfo;
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            const auto access = SharedAccessOf(inst->Opcode());
            if (access == SharedAccess::None || access == SharedAccess::Read) continue;
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= memory.size()) FailEmit("a shared memory access has no memory metadata");
            if (memory[index].kind == ResourceKind::Gds) return true;
        }
    }
    return false;
}

}

void DefineModule(SpirvEmitterState& state) {
    state.interfaceVariables.reserve(state.program.Info().inputs.size() + state.program.Info().outputs.size());
    DefineInputs(state);
    DefineOutputs(state);
    DefineTessellationInterfaces(state);
    DefineDescriptors(state);
    if (state.requirements.functionLds && !state.pixelFunctionLdsBuffer) {
        state.ldsVariable = state.module.AllocateId();
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.scratchVariable.at(half) = state.module.AllocateId();
        }
    }
    if (const auto window = state.program.Metadata().vectorWindowSize; window != 0u) {
        static const bool narrowWindow = std::getenv("APS5_NO_WINDOW_NARROWING") == nullptr;
        state.vectorWindowIndexed = narrowWindow ? IndexedWindowSlots(state.program, state.laneCount) : 0u;
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.vectorWindowVariable.at(half) = state.module.AllocateId();
            if (state.vectorWindowIndexed == 0u) continue;
            auto& slots = state.vectorWindowSlots.at(half);
            for (std::uint32_t slot = state.vectorWindowIndexed; slot < window; slot++) slots.push_back(state.module.AllocateId());
        }
    }
    state.mainFunc = state.module.AllocateId();
    if (StageOf(state) == IrShaderStage::Mesh) {
        const auto& mesh = VertexInfo(state).mesh;
        state.meshGuestFunc = state.module.AllocateId();
        state.module.EmitCapability(spv::CapabilityMeshShadingEXT);
        state.module.EmitExtension("SPV_EXT_mesh_shader");
        state.module.AddExecutionMode(state.mainFunc, mesh.outputPrimitiveSize == 1u ? spv::ExecutionModeOutputPoints : mesh.outputPrimitiveSize == 2u ? spv::ExecutionModeOutputLinesEXT : spv::ExecutionModeOutputTrianglesEXT);
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
        if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityInt64Atomics)) == state.supportedCapabilities.end()) {
            FailEmit("64-bit buffer atomics need shaderBufferInt64Atomics");
        }
    }
    if (state.requirements.bufferU64View) {
        state.module.EmitCapability(spv::CapabilityInt64);
        state.module.EmitCapability(spv::CapabilityInt64Atomics);
    }
    if (state.requirements.sharedInt64Atomics) {
        state.module.EmitCapability(spv::CapabilityInt64);
    }
    if (state.requirements.ldsBufferInt64Atomics) {
        state.module.EmitCapability(spv::CapabilityInt64);
        state.module.EmitCapability(spv::CapabilityInt64Atomics);
    }
    if (state.requirements.imageInt64Atomics) {
        if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityInt64ImageEXT)) == state.supportedCapabilities.end()) {
            FailEmit("64-bit image atomics need VK_EXT_shader_image_atomic_int64");
        }
        state.module.EmitCapability(spv::CapabilityInt64);
        state.module.EmitCapability(spv::CapabilityInt64Atomics);
        state.module.EmitCapability(spv::CapabilityInt64ImageEXT);
        state.module.EmitExtension("SPV_EXT_shader_image_int64");
    }
    if (state.clipDistanceVariable != 0) {
        state.module.EmitCapability(spv::CapabilityClipDistance);
    }
    if (state.cullDistanceVariable != 0) {
        state.module.EmitCapability(spv::CapabilityCullDistance);
    }
    const bool layerExtension = std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityShaderViewportIndexLayerEXT)) != state.supportedCapabilities.end();
    if (state.layerVariable != 0 && layerExtension) {
        state.module.EmitCapability(spv::CapabilityShaderViewportIndexLayerEXT);
        state.module.EmitExtension("SPV_EXT_shader_viewport_index_layer");
    } else if (state.layerVariable != 0) {
        state.module.RequireVersion(0x00010500u);
        state.module.EmitCapability(spv::CapabilityShaderLayer);
    }
    if (InputVariableForKind(state, StageInputKind::Layer) != 0) {
        const auto supported = [&state](spv::Capability capability) {
            return std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) != state.supportedCapabilities.end();
        };
        if (supported(spv::CapabilityGeometry) || !supported(spv::CapabilityShaderViewportIndexLayerEXT)) {
            state.module.EmitCapability(spv::CapabilityGeometry);
        } else {
            state.module.EmitExtension("SPV_EXT_shader_viewport_index_layer");
            state.module.EmitCapability(spv::CapabilityShaderViewportIndexLayerEXT);
        }
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
        state.module.EmitCapability(spv::CapabilityComputeDerivativeGroupLinearKHR);
        state.module.EmitExtension("SPV_KHR_compute_shader_derivatives");
    }
    if (std::any_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) { return input.kind == StageInputKind::BaseInstance; })) {
        state.module.EmitCapability(spv::CapabilityDrawParameters);
    }
    const bool fragmentBarycentric = StageOf(state) == IrShaderStage::Pixel && std::any_of(state.inputs.begin(), state.inputs.end(), [](const SpirvInputBinding& input) {
        return input.perVertex || input.kind == StageInputKind::BaryCoordSmooth || input.kind == StageInputKind::BaryCoordNoPerspective;
    });
    if (fragmentBarycentric && !state.program.Metadata().barycentricEmulation) {
        state.module.EmitCapability(spv::CapabilityFragmentBarycentricKHR);
        state.module.EmitExtension("SPV_KHR_fragment_shader_barycentric");
    }
    state.module.EmitCapability(spv::CapabilitySignedZeroInfNanPreserve);
    state.module.EmitExtension("SPV_KHR_float_controls");
    state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSignedZeroInfNanPreserve, 32u);
    if (state.float32FlushToZero && std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDenormFlushToZero)) != state.supportedCapabilities.end()) {
        state.module.EmitCapability(spv::CapabilityDenormFlushToZero);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDenormFlushToZero, 32u);
    } else if (state.float32DenormPreserve && !state.float32FlushToZero) {
        state.module.EmitCapability(spv::CapabilityDenormPreserve);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDenormPreserve, 32u);
    }
    const bool deviceFloat64 = std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityFloat64)) != state.supportedCapabilities.end();
    state.integerInterpolation = state.requirements.float64Interpolation && !deviceFloat64;
    if (state.integerInterpolation) state.module.EmitCapability(spv::CapabilityInt64);
    if (state.requirements.float64 || (state.requirements.float64Interpolation && deviceFloat64)) {
        if (!deviceFloat64) {
            throw std::runtime_error("64-bit float instructions need the Float64 capability, which the device lacks");
        }
        state.module.EmitCapability(spv::CapabilityFloat64);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSignedZeroInfNanPreserve, 64u);
        if (state.float64DenormPreserve) {
            state.module.EmitCapability(spv::CapabilityDenormPreserve);
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDenormPreserve, 64u);
        }
    }
    if (const auto* workgroup = ShaderWorkgroupInput(state)) {
        const auto local = WorkgroupLocalSize(state, *workgroup);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeLocalSize, local[0], local[1], local[2]);
    }
    if (StageOf(state) == IrShaderStage::Pixel) {
        const auto& pixel = PixelInfo(state);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOriginUpperLeft);
        if (state.depthVariable != 0) {
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDepthReplacing);
            if (pixel.psConservativeZExport == ConservativeZExport::LessThanZ) {
                state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDepthLess);
            }
            if (pixel.psConservativeZExport == ConservativeZExport::GreaterThanZ) {
                state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDepthGreater);
            }
        }
        if (state.stencilRefVariable != 0 && !state.stencilReplay) {
            if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT)) == state.supportedCapabilities.end()) {
                throw std::runtime_error("a pixel shader that exports the stencil reference needs VK_EXT_shader_stencil_export, which the device lacks");
            }
            state.module.EmitCapability(spv::CapabilityStencilExportEXT);
            state.module.EmitExtension("SPV_EXT_shader_stencil_export");
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeStencilRefReplacingEXT);
        }
        const bool merge = pixel.mergeMask != 0u;
        const std::string mergeName = pixel.mergeBlend.enabled ? "blending into an unclamped normalized color target" : "a partial write of a packed color target";
        if (merge && (state.depthVariable != 0 || state.stencilRefVariable != 0 || pixel.psDepthExportEnable || pixel.psSampleMaskExportEnable || pixel.psStencilRefExportEnable || pixel.psAlphaToCoverage || pixel.QuadMasked() || (pixel.psPixelKillEnable && !pixel.psEarlyZ && pixel.mergeDepthStencilWrites))) {
            throw std::runtime_error(mergeName + " merges inside the pixel shader after early depth and stencil tests, which a shader that kills pixels under late tests of a draw writing depth or stencil, exports depth, stencil or coverage, or runs with a quad sample mask cannot use");
        }
        if (merge && pixel.mergeSamples > 1u && WritesGuestMemory(state.program)) {
            throw std::runtime_error("a partial write of a multisampled packed color target runs the pixel shader once per sample, which a shader that writes guest memory (stores, atomics or GDS) cannot use: each covered sample would repeat its writes");
        }
        if (merge && !pixel.psEarlyZ && pixel.mergeDepthStencilTests && WritesGuestMemory(state.program)) {
            throw std::runtime_error(mergeName + " merges inside the pixel shader after early depth and stencil tests, which a shader that writes guest memory (stores, atomics or GDS) under late tests cannot use: the pixels the tests reject would still write it");
        }
        if ((pixel.psEarlyZ && !pixel.psAlphaToCoverage && !pixel.QuadMasked()) || merge) {
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeEarlyFragmentTests);
        }
        if (pixel.psOrderedPixelShader || merge) {
            if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityFragmentShaderPixelInterlockEXT)) == state.supportedCapabilities.end()) {
                throw std::runtime_error(merge ? mergeName + " needs the fragmentShaderPixelInterlock feature, which the device lacks" : "a primitive-ordered pixel shader needs the fragmentShaderPixelInterlock feature, which the device lacks");
            }
            state.module.EmitCapability(spv::CapabilityFragmentShaderPixelInterlockEXT);
            state.module.EmitExtension("SPV_EXT_fragment_shader_interlock");
            state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModePixelInterlockOrderedEXT);
        }
    }
    if (state.requirements.computeDerivatives && StageOf(state) == IrShaderStage::Compute) {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDerivativeGroupLinearKHR);
    }
    state.module.AddName(state.mainFunc, "main");
    if (state.requirements.functionLds && !state.pixelFunctionLdsBuffer) {
        state.module.AddName(state.ldsVariable, "lds_dwords");
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddName(state.scratchVariable.at(half), "scratch_dwords");
        }
    }
    if (state.program.Metadata().vectorWindowSize != 0u) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddName(state.vectorWindowVariable.at(half), "vector_window");
        }
    }
}

}
