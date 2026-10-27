#include "SpirvBackend/SpirvModuleEmitter.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvFlowEmitter.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include "PipelineSpecialization.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

bool UserDataDwordIndex(const SpirvEmitterState& state, ScalarReg reg, std::uint32_t& dwordIndex) {
    const auto registerIndex = RegIndex(reg);
    const auto& registers = state.program.Metadata().bindings.userDataRegisters;
    const auto found = std::lower_bound(registers.begin(), registers.end(), registerIndex);
    if (found == registers.end() || *found != registerIndex) {
        return false;
    }
    dwordIndex = static_cast<std::uint32_t>(found - registers.begin());
    return true;
}

bool MeshBarycentricPixel(const SpirvEmitterState& state) {
    return state.meshBarycentricPixel && state.program.Metadata().barycentricEmulation && state.pixelFlatPositions != 0u;
}

std::uint32_t EmitSolvedBarycentric(SpirvEmitterState& state, StageInputKind kind) {
    const auto cached = state.solvedBarycentrics.find({state.currentLabel, static_cast<std::uint32_t>(kind)});
    if (cached != state.solvedBarycentrics.end()) return cached->second;
    const auto vec3 = TypeF32Vector(state, 3u);
    const auto varying = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, vec3, varying, InputVariableForKind(state, kind));
    std::array<std::uint32_t, 3> corner {};
    for (std::uint32_t index = 0; index < 3u; index++) {
        const auto pointer = state.module.AllocateId();
        const auto position = state.module.AllocateId();
        corner.at(index) = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32Vector(state, 4u)), pointer, state.pixelFlatPositions, ConstantU32(state, index));
        state.module.AddFunction(spv::OpLoad, TypeF32Vector(state, 4u), position, pointer);
        state.module.AddFunction(spv::OpVectorShuffle, vec3, corner.at(index), position, position, 0u, 1u, 3u);
    }
    const auto cross = [&](std::uint32_t left, std::uint32_t right) {
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpExtInst, vec3, result, GlslStd450(state), GLSLstd450Cross, left, right);
        return result;
    };
    const auto dot = [&](std::uint32_t left, std::uint32_t right) {
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpDot, TypeF32(state), result, left, right);
        return result;
    };
    const auto determinant = dot(corner[0], cross(corner[1], corner[2]));
    std::array<std::uint32_t, 3> weight {};
    const std::array<std::array<std::uint32_t, 2>, 3> rows{{{1u, 2u}, {2u, 0u}, {0u, 1u}}};
    for (std::uint32_t index = 0; index < 3u; index++) {
        weight.at(index) = state.module.AllocateId();
        state.module.AddFunction(spv::OpFDiv, TypeF32(state), weight.at(index), dot(varying, cross(corner[rows[index][0]], corner[rows[index][1]])), determinant);
    }
    const auto solved = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, vec3, solved, weight[0], weight[1], weight[2]);
    state.solvedBarycentrics[{state.currentLabel, static_cast<std::uint32_t>(kind)}] = solved;
    return solved;
}

const ShaderWorkgroupInputInfo* ShaderWorkgroupInputFor(const SpirvEmitterState& state) {
    switch (state.program.Resources().stage) {
    case IrShaderStage::Compute:
        return state.inputInfo.compute;
    case IrShaderStage::Mesh:
        return state.inputInfo.vertex != nullptr ? &state.inputInfo.vertex->mesh : nullptr;
    default:
        return nullptr;
    }
}

namespace {

std::uint32_t GuestSampleIndex(SpirvEmitterState& state, std::uint32_t hostIndex) {
    if (state.sampleOrderWord == 0u) return hostIndex;
    const auto shift = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitBinaryU32(state, spv::OpBitwiseAnd, hostIndex, ConstantU32(state, 7u)), ConstantU32(state, 2u));
    return EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, state.sampleOrderWord, shift), ConstantU32(state, 7u));
}

std::uint32_t ReorderedSampleBits(SpirvEmitterState& state, std::uint32_t bits, bool toHost) {
    auto reordered = EmitBinaryU32(state, spv::OpBitwiseAnd, bits, ConstantU32(state, ~0xffu));
    for (std::uint32_t sample = 0; sample < 8u; ++sample) {
        const auto guest = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, state.sampleOrderWord, ConstantU32(state, sample * 4u)), ConstantU32(state, 7u));
        if (toHost) {
            const auto bit = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, bits, guest), ConstantU32(state, 1u));
            reordered = EmitBinaryU32(state, spv::OpBitwiseOr, reordered, EmitBinaryU32(state, spv::OpShiftLeftLogical, bit, ConstantU32(state, sample)));
        } else {
            const auto bit = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, bits, ConstantU32(state, sample)), ConstantU32(state, 1u));
            reordered = EmitBinaryU32(state, spv::OpBitwiseOr, reordered, EmitBinaryU32(state, spv::OpShiftLeftLogical, bit, guest));
        }
    }
    return reordered;
}

}

std::uint32_t EmitBuiltinU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component);

std::uint32_t EmitQuadPixel(SpirvEmitterState& state) {
    const auto coordinate = [&](std::uint32_t component) {
        const auto value = Unary(state, spv::OpBitcast, TypeF32(state), EmitBuiltinU32(state, StageInputKind::FragCoord, component));
        return EmitBinaryU32(state, spv::OpBitwiseAnd, Unary(state, spv::OpConvertFToU, TypeU32(state), value), ConstantU32(state, 1u));
    };
    return EmitBinaryU32(state, spv::OpBitwiseOr, coordinate(0u), EmitBinaryU32(state, spv::OpShiftLeftLogical, coordinate(1u), ConstantU32(state, 1u)));
}

std::uint32_t InterpolationOffset(SpirvEmitterState& state, std::uint8_t location) {
    const std::array<std::uint32_t, 2> components{ConstantF32Value(state, SampleLocationOffset(location, 0u)), ConstantF32Value(state, SampleLocationOffset(location, 1u))};
    return state.module.Constant(spv::OpConstantComposite, TypeF32Vector(state, 2u), std::span<const std::uint32_t>(components));
}

std::uint32_t EmitSampleOffset(SpirvEmitterState& state) {
    const auto& locations = state.inputInfo.pixel->sampleLocations;
    const bool vary = state.inputInfo.pixel->SampleLocationsVary();
    const bool iterated = state.program.Metadata().pixelIteratesSamples;
    if (!iterated && !vary) return InterpolationOffset(state, locations[0]);
    const auto count = vary ? static_cast<std::uint32_t>(locations.size()) : 8u;
    std::array<std::uint32_t, 32> offsets{};
    for (std::uint32_t index = 0; index < count; ++index) offsets[index] = InterpolationOffset(state, locations[index]);
    const auto array = state.module.Type(spv::OpTypeArray, TypeF32Vector(state, 2u), ConstantU32(state, count));
    const auto table = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, array), spv::StorageClassPrivate);
    state.module.AddFunction(spv::OpStore, table, state.module.Constant(spv::OpConstantComposite, array, std::span<const std::uint32_t>(offsets.data(), count)));
    auto index = iterated ? EmitBuiltinU32(state, StageInputKind::SampleId, 0u) : ConstantU32(state, 0u);
    if (vary) index = EmitBinaryU32(state, spv::OpBitwiseOr, index, EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitQuadPixel(state), ConstantU32(state, 3u)));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassPrivate, TypeF32Vector(state, 2u)), pointer, table, index);
    const auto offset = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeF32Vector(state, 2u), offset, pointer);
    return offset;
}

std::uint32_t EmitOffsetBarycentricU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component) {
    const bool smoothOffset = kind == StageInputKind::BaryCoordSmoothCenter || kind == StageInputKind::BaryCoordSmoothSample;
    const auto variable = smoothOffset && state.barycentricReciprocalWVariable != 0u
        ? state.barycentricReciprocalWVariable
        : InputVariableForKind(state, BarycentricInputOf(kind));
    if (variable == 0u) {
        throw std::runtime_error("an offset barycentric read lacks its barycentric input");
    }
    if (std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityInterpolationFunction)) == state.supportedCapabilities.end()) {
        throw std::runtime_error("barycentrics away from the fragment's default position need the device's sampleRateShading");
    }
    state.module.EmitCapability(spv::CapabilityInterpolationFunction);
    const bool center = kind == StageInputKind::BaryCoordSmoothCenter || kind == StageInputKind::BaryCoordNoPerspectiveCenter;
    const auto offset = center ? InterpolationOffset(state, 0u) : EmitSampleOffset(state);
    const auto weights = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32Vector(state, 3u), weights, GlslStd450(state), GLSLstd450InterpolateAtOffset, variable, offset);
    const auto weight = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), weight, weights, component + 1u);
    if (smoothOffset && state.barycentricReciprocalWVariable != 0u) {
        std::array<std::uint32_t, 3> components{};
        for (std::uint32_t index = 0; index < components.size(); ++index) {
            components[index] = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), components[index], weights, index);
        }
        const auto firstSum = state.module.AllocateId();
        state.module.AddFunction(spv::OpFAdd, TypeF32(state), firstSum, components[0], components[1]);
        const auto sum = state.module.AllocateId();
        state.module.AddFunction(spv::OpFAdd, TypeF32(state), sum, firstSum, components[2]);
        const auto normalized = state.module.AllocateId();
        state.module.AddFunction(spv::OpFDiv, TypeF32(state), normalized, weight, sum);
        return Unary(state, spv::OpBitcast, TypeU32(state), normalized);
    }
    return Unary(state, spv::OpBitcast, TypeU32(state), weight);
}

std::uint32_t EmitBuiltinU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component) {
    if (BarycentricInputOf(kind) != kind) {
        return EmitOffsetBarycentricU32(state, kind, component);
    }
    if (kind == StageInputKind::LocalInvocationIndex) {
        return EmitLocalInvocationIndex(state);
    }
    if (kind == StageInputKind::DispatchThreadLimit) {
        return EmitShaderDataDwordLoad(state, state.program.Metadata().bindings.DispatchThreadLimitDword() + component);
    }
    if (kind == StageInputKind::RasterPrimitive) {
        return state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::RasterPrimitive, static_cast<std::uint32_t>(RasterPrimitive::Triangle));
    }
    if (kind == StageInputKind::LayerExported) {
        return state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::LayerExported, 0u);
    }
    if ((state.laneCount == 2 || state.spannedWave) && (kind == StageInputKind::LocalInvocationId || kind == StageInputKind::GlobalInvocationId)) {
        const auto* workgroup = ShaderWorkgroupInputFor(state);
        if (workgroup == nullptr) {
            throw std::runtime_error("workgroup input info is missing for a local/global invocation builtin");
        }
        std::uint32_t divisor = 1u;
        for (std::uint32_t axis = 0; axis < component; axis++) {
            divisor *= std::max(workgroup->threadsNum[axis], 1u);
        }
        const auto size = std::max(workgroup->threadsNum[component], 1u);
        const auto divided = EmitBinaryU32(state, spv::OpUDiv, EmitLocalInvocationIndex(state), ConstantU32(state, divisor));
        const auto local = EmitBinaryU32(state, spv::OpUMod, divided, ConstantU32(state, size));
        if (kind == StageInputKind::LocalInvocationId) {
            return local;
        }
        const auto group = EmitBuiltinU32(state, StageInputKind::WorkgroupId, component);
        return EmitBinaryU32(state, spv::OpIAdd, local, EmitBinaryU32(state, spv::OpIMul, group, ConstantU32(state, size)));
    }
    if (StageOf(state) == IrShaderStage::Compute && (kind == StageInputKind::WorkgroupId || kind == StageInputKind::GlobalInvocationId)) {
        const auto offset = state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::WorkgroupOffsetBase + component, 0u);
        const auto value = EmitInputComponentU32(state, kind, component);
        if (kind == StageInputKind::WorkgroupId) {
            return EmitBinaryU32(state, spv::OpIAdd, value, offset);
        }
        const auto local = WorkgroupLocalSize(state, *ShaderWorkgroupInput(state));
        return EmitBinaryU32(state, spv::OpIAdd, value, EmitBinaryU32(state, spv::OpIMul, offset, ConstantU32(state, local[component])));
    }
    const auto variable = InputVariableForKind(state, kind);
    if (variable == 0) {
        return ConstantU32(state, 0u);
    }
    if (kind == StageInputKind::FrontFacing) {
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeBool(state), value, variable);
        state.module.AddFunction(spv::OpSelect, TypeU32(state), bits, value, ConstantU32(state, 0x3f800000u), ConstantU32(state, 0xbf800000u));
        return bits;
    }
    if (kind == StageInputKind::HelperInvocation) {
        auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeBool(state), value, variable);
        if (state.quadPixelUncovered != 0u) {
            const auto helper = state.module.AllocateId();
            state.module.AddFunction(spv::OpLogicalOr, TypeBool(state), helper, value, state.quadPixelUncovered);
            value = helper;
        }
        state.module.AddFunction(spv::OpSelect, TypeU32(state), bits, value, ConstantU32(state, 1u), ConstantU32(state, 0u));
        return bits;
    }
    if (kind == StageInputKind::VertexIndex || kind == StageInputKind::InstanceIndex || kind == StageInputKind::BaseInstance || kind == StageInputKind::InvocationId || kind == StageInputKind::PrimitiveId || kind == StageInputKind::Layer || kind == StageInputKind::SampleId) {
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeI32(state), value, variable);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        return kind == StageInputKind::SampleId ? GuestSampleIndex(state, bits) : bits;
    }
    if (kind == StageInputKind::SampleMask) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeI32(state)), pointer, variable, ConstantU32(state, 0u));
        state.module.AddFunction(spv::OpLoad, TypeI32(state), value, pointer);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        return bits;
    }
    if (kind == StageInputKind::FragCoord || kind == StageInputKind::TessCoord) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, variable, ConstantU32(state, component));
        state.module.AddFunction(spv::OpLoad, TypeF32(state), value, pointer);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        return bits;
    }
    if ((kind == StageInputKind::BaryCoordSmooth || kind == StageInputKind::BaryCoordNoPerspective) && MeshBarycentricPixel(state)) {
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), value, EmitSolvedBarycentric(state, kind), component + 1u);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        return bits;
    }
    if (kind == StageInputKind::BaryCoordSmooth || kind == StageInputKind::BaryCoordNoPerspective) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, variable, ConstantU32(state, component + 1u));
        state.module.AddFunction(spv::OpLoad, TypeF32(state), value, pointer);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        return bits;
    }
    return EmitInputComponentU32(state, kind, component);
}

std::uint32_t EmitAttributeValue(SpirvEmitterState& state, std::uint32_t attr, std::uint32_t chan) {
    if (PixelParameterIsDefault(state, attr)) {
        return ConstantU32(state, state.inputInfo.pixel->InputDefaultBits(attr, chan & 3u));
    }
    const auto* input = SpirvInputBindingForParameter(state, attr);
    if (input == nullptr || input->variableId == 0) {
        return ConstantU32(state, 0u);
    }
    const auto stage = state.program.Resources().stage;
    if (stage == IrShaderStage::Vertex || stage == IrShaderStage::Local) {
        return EmitVertexParameterComponentU32(state, *input, chan & 3u);
    }
    const auto loadPerVertex = [&](std::uint32_t vertex) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, input->variableId, ConstantU32(state, vertex), ConstantU32(state, chan & 3u));
        state.module.AddFunction(spv::OpLoad, TypeF32(state), value, pointer);
        return value;
    };
    if (input->perVertex) {
        if (state.inputInfo.pixel == nullptr) {
            throw std::runtime_error("pixel input info is missing for a per-vertex attribute");
        }
        if (PixelParameterIsFlat(state, attr)) {
            const auto bits = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, loadPerVertex(0u));
            return bits;
        }
        const auto barycentricKind = PixelParameterIsLinear(state, attr) ? StageInputKind::BaryCoordNoPerspective : StageInputKind::BaryCoordSmooth;
        const auto barycentric = InputVariableForKind(state, barycentricKind);
        if (barycentric == 0u) {
            throw std::runtime_error("per-vertex attribute interpolation lacks its barycentric input");
        }
        std::uint32_t sum = 0;
        const auto solved = MeshBarycentricPixel(state) ? EmitSolvedBarycentric(state, barycentricKind) : 0u;
        for (std::uint32_t vertex = 0; vertex < 3u; vertex++) {
            const auto pointer = state.module.AllocateId();
            const auto weight = state.module.AllocateId();
            const auto product = state.module.AllocateId();
            if (solved != 0u) {
                state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), weight, solved, vertex);
            } else {
                state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, barycentric, ConstantU32(state, vertex));
                state.module.AddFunction(spv::OpLoad, TypeF32(state), weight, pointer);
            }
            state.module.AddFunction(spv::OpFMul, TypeF32(state), product, loadPerVertex(vertex), weight);
            if (vertex == 0u) {
                sum = product;
            } else {
                const auto next = state.module.AllocateId();
                state.module.AddFunction(spv::OpFAdd, TypeF32(state), next, sum, product);
                sum = next;
            }
        }
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, sum);
        return bits;
    }
    const auto vector = state.module.AllocateId();
    const auto component = state.module.AllocateId();
    const auto bits = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeF32Vector(state, 4u), vector, input->variableId);
    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), component, vector, chan & 3u);
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, component);
    return bits;
}

std::uint32_t EmitInterpolationParameterValue(SpirvEmitterState& state, std::uint32_t attr, std::uint32_t chan, std::uint32_t mode) {
    if (state.inputInfo.pixel == nullptr) {
        throw std::runtime_error("interpolation parameter refers to an undefined attribute");
    }
    if (attr >= state.inputInfo.pixel->inputNum || PixelParameterIsDefault(state, attr)) {
        return mode == 2u ? ConstantU32(state, state.inputInfo.pixel->InputDefaultBits(attr, chan & 3u)) : ConstantU32(state, 0u);
    }
    const auto* input = SpirvInputBindingForParameter(state, attr);
    if (input == nullptr) {
        return mode == 2u ? ConstantU32(state, state.inputInfo.pixel->InputDefaultBits(attr, chan & 3u)) : ConstantU32(state, 0u);
    }
    if (!input->perVertex) {
        return EmitAttributeValue(state, attr, chan);
    }
    const auto loadVertex = [&](std::uint32_t vertex) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, input->variableId, ConstantU32(state, vertex), ConstantU32(state, chan & 3u));
        state.module.AddFunction(spv::OpLoad, TypeF32(state), value, pointer);
        return value;
    };
    const auto selectedVertex = (mode + 1u) % 3u;
    std::uint32_t value = loadVertex(selectedVertex);
    if (!PixelParameterIsCustom(state, attr) && mode < 2u) {
        const auto delta = state.module.AllocateId();
        state.module.AddFunction(spv::OpFSub, TypeF32(state), delta, value, loadVertex(0u));
        value = delta;
    }
    const auto bits = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
    return bits;
}

std::uint32_t EmitInterpolationParameterF16Value(SpirvEmitterState& state, std::uint32_t attr, std::uint32_t chan, std::uint32_t mode, bool high) {
    const auto& pixel = *state.inputInfo.pixel;
    if (pixel.InputHalfIsDefault(attr, high)) {
        return ConstantF32(state, mode == 2u ? pixel.InputHalfDefaultBits(attr, chan & 3u, high) : 0u);
    }
    const auto* input = SpirvInputBindingForParameter(state, attr);
    if (input == nullptr || !input->perVertex) {
        throw std::runtime_error("16-bit interpolation parameter is not read per vertex");
    }
    const auto loadVertex = [&](std::uint32_t vertex) {
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, input->variableId, ConstantU32(state, vertex), ConstantU32(state, chan & 3u));
        state.module.AddFunction(spv::OpLoad, TypeF32(state), value, pointer);
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        if (high) {
            const auto shifted = state.module.AllocateId();
            state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), shifted, bits, ConstantU32(state, 16u));
            bits = shifted;
        }
        return EmitF16BitsToF32(state, bits);
    };
    const auto value = loadVertex((mode + 1u) % 3u);
    if (mode == 2u) {
        return value;
    }
    const auto delta = state.module.AllocateId();
    state.module.AddFunction(spv::OpFSub, TypeF32(state), delta, value, loadVertex(0u));
    return EmitF16BitsToF32(state, EmitConvertF16F32(state, delta));
}

std::uint32_t MrtOutputMode(const SpirvEmitterState& state, const ExportInfo& exp) {
    if (state.program.Resources().stage != IrShaderStage::Pixel || exp.kind != ExportTargetKind::Mrt) {
        return 0u;
    }
    if (state.inputInfo.pixel == nullptr || exp.index >= std::size(state.inputInfo.pixel->targetOutputMode)) {
        return 0u;
    }
    return state.inputInfo.pixel->targetOutputMode[exp.index];
}

std::uint32_t PackExport(SpirvEmitterState& state, IrBufferFormat format, std::uint32_t color) {
    const auto info = GetFormatInfo(format);
    auto packed = ConstantU32(state, 0u);
    for (std::uint32_t component = 0; component < info.componentCount; component++) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), value, color, component);
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
        const auto field = EmitFormatStoreComponent(state, info, component, bits);
        packed = EmitOrU32(state, packed, info.componentBitOffset[component] == 0u ? field : EmitBinaryU32(state, spv::OpShiftLeftLogical, field, ConstantU32(state, info.componentBitOffset[component])));
    }
    return packed;
}

std::uint32_t SelectExportPacking(SpirvEmitterState& state, std::uint32_t target, std::uint32_t color) {
    const auto packing = state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::ExportPackingBase + target, static_cast<std::uint32_t>(ColorExportPacking::None));
    const auto unorm = EmitCompareU32Constant(state, spv::OpIEqual, packing, static_cast<std::uint32_t>(ColorExportPacking::Unorm10_11_11));
    const auto floating = EmitCompareU32Constant(state, spv::OpIEqual, packing, static_cast<std::uint32_t>(ColorExportPacking::Float11_11_10));
    const auto small = EmitCompareU32Constant(state, spv::OpIEqual, packing, static_cast<std::uint32_t>(ColorExportPacking::Unorm4_4_4_4));
    const auto wide = Select(state, TypeU32(state), floating, PackExport(state, IrBufferFormat::Format10_11_11Float, color), PackExport(state, IrBufferFormat::Format11_11_10UNorm, color));
    const auto word = Select(state, TypeU32(state), small, PackExport(state, IrBufferFormat::Format4_4_4_4UNorm, color), wide);
    const auto packed = Binary(state, spv::OpLogicalOr, TypeBool(state), Binary(state, spv::OpLogicalOr, TypeBool(state), unorm, floating), small);
    std::array<std::uint32_t, 4> components{};
    for (std::uint32_t component = 0; component < components.size(); ++component) {
        const auto unpacked = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), unpacked, color, component);
        components[component] = Select(state, TypeF32(state), packed, component == 0u ? Unary(state, spv::OpBitcast, TypeF32(state), word) : ConstantF32(state, 0u), unpacked);
    }
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4u), result, components[0], components[1], components[2], components[3]);
    return result;
}

struct MergeTexel {
    std::uint32_t resource;
    std::uint32_t coord;
    std::uint32_t sample;
};

MergeTexel MergeTargetTexel(SpirvEmitterState& state) {
    const auto& pixel = PixelInfo(state);
    const auto& images = state.program.Info().images;
    const auto image = std::ranges::find_if(images, [](const ImageResource& candidate) { return candidate.targetMerge; });
    if (image == images.end()) FailEmit("the color target merge image was not collected");
    if (InputVariableForKind(state, StageInputKind::FragCoord) == 0u || (pixel.mergeSamples > 1u && InputVariableForKind(state, StageInputKind::SampleId) == 0u)) FailEmit("the color target merge inputs were not declared");
    if (state.mergeWordVariable == 0u || state.mergePendingVariable == 0u) FailEmit("the color target merge variables were not declared");
    const auto position = [&](std::uint32_t component) { return Unary(state, spv::OpConvertFToU, TypeU32(state), Unary(state, spv::OpBitcast, TypeF32(state), EmitBuiltinU32(state, StageInputKind::FragCoord, component))); };
    const auto x = position(0u);
    const auto y = position(1u);
    const auto coord = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 2u), coord, x, y);
    return {static_cast<std::uint32_t>(image - images.begin()), coord, pixel.mergeSamples > 1u ? EmitBuiltinU32(state, StageInputKind::SampleId, 0u) : 0u};
}

void MergeColorTarget(SpirvEmitterState& state, std::uint32_t color) {
    const auto& pixel = PixelInfo(state);
    const auto [resource, coord, sample] = MergeTargetTexel(state);
    const auto stored = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), stored, EmitStorageImageRead(state, resource, coord, sample), 0u);
    const auto word = PackExport(state, IrBufferFormat::Format4_4_4_4UNorm, color);
    const auto kept = EmitBinaryU32(state, spv::OpBitwiseAnd, stored, ConstantU32(state, ~static_cast<std::uint32_t>(pixel.mergeMask)));
    const auto merged = EmitOrU32(state, kept, EmitBinaryU32(state, spv::OpBitwiseAnd, word, ConstantU32(state, pixel.mergeMask)));
    state.module.AddFunction(spv::OpStore, state.mergeWordVariable, merged);
    state.module.AddFunction(spv::OpStore, state.mergePendingVariable, ConstantU32(state, 1u));
}

bool AlphaToCoverage(const SpirvEmitterState& state) {
    return state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->psAlphaToCoverage;
}

void StoreCoverage(SpirvEmitterState& state, std::uint32_t mask) {
    if (state.quadSampleCoverage != 0u) mask = Binary(state, spv::OpBitwiseAnd, TypeI32(state), mask, state.quadSampleCoverage);
    if (state.sampleOrderWord != 0u) mask = Unary(state, spv::OpBitcast, TypeI32(state), ReorderedSampleBits(state, Unary(state, spv::OpBitcast, TypeU32(state), mask), true));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeI32(state)), pointer, state.sampleMaskVariable, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpStore, pointer, mask);
}

void StoreDitheredAlphaCoverage(SpirvEmitterState& state, std::uint32_t color) {
    constexpr std::array<std::uint32_t, 8> twoSampleMasks{{
    0x00000000u, 0x02000002u, 0x01000201u, 0x01020201u, 0x01020101u, 0x03020103u, 0x03010303u, 0x03030303u,
    }};
    constexpr std::array<std::uint32_t, 64> eightSampleMasks{{
    0x00000000u, 0x00000010u, 0x00000004u, 0x10000004u, 0x10000002u, 0x10100002u, 0x04100002u, 0x04101002u,
    0x04101001u, 0x04101011u, 0x04041011u, 0x14041011u, 0x12041011u, 0x12141011u, 0x12140411u, 0x12141411u,
    0x12141411u, 0x12141431u, 0x12141415u, 0x32141415u, 0x32121415u, 0x32321415u, 0x16321415u, 0x16323415u,
    0x15323415u, 0x15323435u, 0x15163435u, 0x35163435u, 0x35163235u, 0x35363235u, 0x35361635u, 0x35363635u,
    0x35363635u, 0x35363675u, 0x3536363du, 0x7536363du, 0x75363637u, 0x75763637u, 0x3d763637u, 0x3d767637u,
    0x3d757637u, 0x3d757677u, 0x3d3d7677u, 0x7d3d7677u, 0x773d7677u, 0x777d7677u, 0x777d3e77u, 0x777d7e77u,
    0x777d7e77u, 0x777d7ef7u, 0x777d7e7fu, 0xf77d7e7fu, 0xf7777e7fu, 0xf7f77e7fu, 0x7ff77e7fu, 0x7ff7fe7fu,
    0x7ff7fd7fu, 0x7ff7fdffu, 0x7f7ffdffu, 0xff7ffdffu, 0xff7ff7ffu, 0xfffff7ffu, 0xffff7fffu, 0xffffffffu,
    }};
    const std::span<const std::uint32_t> masks = state.inputInfo.pixel->psDitheredAlphaSamples == 2u ? std::span<const std::uint32_t>(twoSampleMasks) : std::span<const std::uint32_t>(eightSampleMasks);
    const auto levels = static_cast<float>(masks.size());
    const auto alpha = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), alpha, color, 3u);
    const auto positive = Select(state, TypeF32(state), Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), alpha, ConstantF32Value(state, 0.0f)), alpha, ConstantF32Value(state, 0.0f));
    const auto bounded = Select(state, TypeF32(state), Binary(state, spv::OpFOrdGreaterThan, TypeBool(state), positive, ConstantF32Value(state, (levels - 1.0f) / levels)), ConstantF32Value(state, (levels - 1.0f) / levels), positive);
    const auto index = Unary(state, spv::OpConvertFToU, TypeU32(state), Binary(state, spv::OpFMul, TypeF32(state), bounded, ConstantF32Value(state, levels)));
    std::vector<std::uint32_t> words(masks.size());
    for (std::size_t i = 0; i < words.size(); ++i) words[i] = ConstantU32(state, masks[i]);
    const auto array = state.module.Type(spv::OpTypeArray, TypeU32(state), ConstantU32(state, static_cast<std::uint32_t>(words.size())));
    const auto table = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, array), spv::StorageClassPrivate);
    state.module.AddFunction(spv::OpStore, table, state.module.Constant(spv::OpConstantComposite, array, std::span<const std::uint32_t>(words)));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassPrivate, TypeU32(state)), pointer, table, index);
    const auto packed = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), packed, pointer);
    const auto coord = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeF32Vector(state, 4u), coord, InputVariableForKind(state, StageInputKind::FragCoord));
    std::array<std::uint32_t, 2> parity{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto component = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), component, coord, i);
        parity[i] = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Unary(state, spv::OpConvertFToU, TypeU32(state), component), ConstantU32(state, 1u));
    }
    const auto pixel = Binary(state, spv::OpBitwiseOr, TypeU32(state), Binary(state, spv::OpShiftLeftLogical, TypeU32(state), parity[0], ConstantU32(state, 1u)), parity[1]);
    const auto shift = Binary(state, spv::OpShiftLeftLogical, TypeU32(state), pixel, ConstantU32(state, 3u));
    const auto mask = Binary(state, spv::OpBitwiseAnd, TypeU32(state), Binary(state, spv::OpShiftRightLogical, TypeU32(state), packed, shift), ConstantU32(state, 255u));
    StoreCoverage(state, Unary(state, spv::OpBitcast, TypeI32(state), mask));
}

void StoreAlphaCoverage(SpirvEmitterState& state, std::uint32_t color) {
    if (state.inputInfo.pixel->psDitheredAlphaToCoverage) {
        StoreDitheredAlphaCoverage(state, color);
        return;
    }
    const auto alpha = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), alpha, color, 3u);
    const auto below = state.module.AllocateId();
    state.module.AddFunction(spv::OpFUnordLessThan, TypeBool(state), below, alpha, ConstantF32(state, 0x3f000000u));
    const auto mask = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, TypeI32(state), mask, below, ConstantI32(state, 0), ConstantI32(state, -1));
    StoreCoverage(state, mask);
}

std::uint32_t ExportRawComponent(SpirvValueEmitContext& ctx, std::uint32_t vector, std::uint32_t component) {
    const auto value = ctx.state.module.AllocateId();
    ctx.state.module.AddFunction(spv::OpCompositeExtract, TypeU32(ctx.state), value, vector, component);
    return value;
}

std::uint32_t ExportVector(SpirvValueEmitContext& ctx, std::uint32_t data, const ExportInfo& exp, bool uintOutput) {
    auto& state = ctx.state;
    if (exp.compr && !uintOutput) {
        const auto mode = MrtOutputMode(state, exp);
        const auto unpack = mode == 5u ? GLSLstd450UnpackUnorm2x16 : mode == 6u ? GLSLstd450UnpackSnorm2x16 : GLSLstd450UnpackHalf2x16;
        std::array<std::uint32_t, 4> f32 {ConstantF32(state, 0u), ConstantF32(state, 0u), ConstantF32(state, 0u), ConstantF32(state, 0x3f800000u)};
        for (std::uint32_t pair = 0; pair < 2u; pair++) {
            if ((exp.en & (3u << (pair * 2u))) == 0u) {
                continue;
            }
            const auto packed = state.module.AllocateId();
            const auto unpacked = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), packed, data, pair);
            state.module.AddFunction(spv::OpExtInst, TypeF32Vector(state, 2u), unpacked, GlslStd450(state), unpack, packed);
            for (std::uint32_t lane = 0; lane < 2u; lane++) {
                const auto component = pair * 2u + lane;
                if (((exp.en >> component) & 1u) != 0u) {
                    f32.at(component) = state.module.AllocateId();
                    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), f32.at(component), unpacked, lane);
                }
            }
        }
        const auto vector = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4u), vector, f32[0], f32[1], f32[2], f32[3]);
        return vector;
    }
    std::array<std::uint32_t, 4> raw {
        ConstantU32(state, 0u),
        ConstantU32(state, 0u),
        ConstantU32(state, 0u),
        ConstantU32(state, uintOutput ? 1u : 0x3f800000u),
    };
    const auto mode = MrtOutputMode(state, exp);
    const auto exported = exp.en & (mode == 1u ? 0x1u : mode == 2u || mode == 3u ? 0x3u : 0xfu);
    if (exp.compr) {
        for (std::uint32_t pair = 0; pair < 2u; pair++) {
            if ((exp.en & (3u << (pair * 2u))) == 0u) {
                continue;
            }
            const auto packed = ExportRawComponent(ctx, data, pair);
            for (std::uint32_t lane = 0; lane < 2u; lane++) {
                const auto component = pair * 2u + lane;
                if (((exp.en >> component) & 1u) == 0u) {
                    continue;
                }
                raw.at(component) = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), raw.at(component), packed, ConstantU32(state, lane * 16u), ConstantU32(state, 16u));
            }
        }
    } else {
        for (std::uint32_t component = 0; component < 4u; component++) {
            if (((exported >> component) & 1u) != 0u) {
                raw.at(mode == 3u && component == 1u ? 3u : component) = ExportRawComponent(ctx, data, component);
            }
        }
    }
    if (uintOutput) {
        const auto vector = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4u), vector, raw[0], raw[1], raw[2], raw[3]);
        return vector;
    }
    std::array<std::uint32_t, 4> f32 {};
    for (std::uint32_t component = 0; component < 4u; component++) {
        f32.at(component) = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeF32(state), f32.at(component), raw.at(component));
    }
    const auto vector = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4u), vector, f32[0], f32[1], f32[2], f32[3]);
    return vector;
}

float PointSizeFloor(const ShaderVertexInputInfo& vertex) {
    return std::max(vertex.pointSizeMin, std::numeric_limits<float>::min());
}

void EmitAuxPositionExport(SpirvValueEmitContext& ctx, std::uint32_t data, const ExportInfo& exp) {
    auto& state = ctx.state;
    if (state.inputInfo.vertex == nullptr) {
        throw std::runtime_error("vertex input info is missing for an auxiliary position export");
    }
    for (std::uint32_t component = 0; component < 4u; component++) {
        if ((exp.en & (1u << component)) == 0u) {
            continue;
        }
        const auto output = DecodePositionExportComponent(state.inputInfo.vertex->paClVsOutCntl, exp.index, component);
        if (output.layer || output.viewport) {
            const auto raw = ExportRawComponent(ctx, data, component);
            if (output.layer) {
                const auto layer = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), layer, raw, ConstantU32(state, 0x7ffu));
                const auto pointer = state.program.Resources().stage == IrShaderStage::Mesh ? MeshOutputPointer(state, StageOutputKind::Layer) : state.layerVariable;
                state.module.AddFunction(spv::OpStore, pointer, layer);
            }
            if (output.viewport) {
                const auto viewport = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), viewport, raw, ConstantU32(state, 16u), ConstantU32(state, 4u));
                state.module.AddFunction(spv::OpStore, state.viewportIndexVariable, viewport);
            }
            continue;
        }
        if (!output.pointSize && output.clipDistance == std::numeric_limits<std::uint32_t>::max() && output.cullDistance == std::numeric_limits<std::uint32_t>::max()) {
            continue;
        }
        const auto raw = ExportRawComponent(ctx, data, component);
        const auto f32 = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeF32(state), f32, raw);
        if (output.pointSize) {
            auto size = f32;
            if (state.inputInfo.vertex->pointList) {
                size = state.module.AllocateId();
                state.module.AddFunction(spv::OpExtInst, TypeF32(state), size, GlslStd450(state), GLSLstd450FClamp, f32, ConstantF32(state, std::bit_cast<std::uint32_t>(PointSizeFloor(*state.inputInfo.vertex))), ConstantF32(state, std::bit_cast<std::uint32_t>(state.inputInfo.vertex->pointSizeMax)));
            }
            const auto pointer = state.program.Resources().stage == IrShaderStage::Mesh ? MeshOutputPointer(state, StageOutputKind::PointSize) : state.pointSizeVariable;
            state.module.AddFunction(spv::OpStore, pointer, size);
            continue;
        }
        const auto storeDistance = [&](StageOutputKind kind, std::uint32_t variable, std::uint32_t index) {
            if (index == std::numeric_limits<std::uint32_t>::max()) {
                return;
            }
            if (state.program.Resources().stage == IrShaderStage::Mesh) {
                state.module.AddFunction(spv::OpStore, MeshOutputPointer(state, kind, index), f32);
                return;
            }
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeF32(state)), pointer, variable, ConstantU32(state, index));
            state.module.AddFunction(spv::OpStore, pointer, f32);
        };
        storeDistance(StageOutputKind::ClipDistance, state.clipDistanceVariable, output.clipDistance);
        storeDistance(StageOutputKind::CullDistance, state.cullDistanceVariable, output.cullDistance);
    }
}

void StoreZeroPositionClipDistance(SpirvEmitterState& state, std::uint32_t position) {
    const auto bits = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeU32Vector(state, 4u), bits, position);
    std::uint32_t combined = 0u;
    for (std::uint32_t component = 0; component < 4u; component++) {
        const auto raw = state.module.AllocateId();
        const auto magnitude = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), raw, bits, component);
        state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), magnitude, raw, ConstantU32(state, 0x7fffffffu));
        if (component == 0u) {
            combined = magnitude;
            continue;
        }
        const auto merged = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), merged, combined, magnitude);
        combined = merged;
    }
    const auto allZero = state.module.AllocateId();
    const auto distance = state.module.AllocateId();
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), allZero, combined, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelect, TypeF32(state), distance, allZero, ConstantF32Value(state, -1.0f), ConstantF32Value(state, 0.0f));
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeF32(state)), pointer, state.clipDistanceVariable, ConstantU32(state, *state.zeroPositionClipIndex));
    state.module.AddFunction(spv::OpStore, pointer, distance);
}

std::uint32_t ConvertClipCoordinate(SpirvEmitterState& state, std::uint32_t coordinate, float scale, float offset, float halfExtent) {
    const auto window = state.module.AllocateId();
    const auto biased = state.module.AllocateId();
    const auto divided = state.module.AllocateId();
    const auto ndc = state.module.AllocateId();
    state.module.AddFunction(spv::OpFMul, TypeF32(state), window, coordinate, ConstantF32Value(state, scale));
    state.module.AddFunction(spv::OpFAdd, TypeF32(state), biased, window, ConstantF32Value(state, offset));
    state.module.AddFunction(spv::OpFDiv, TypeF32(state), divided, biased, ConstantF32Value(state, halfExtent));
    state.module.AddFunction(spv::OpFSub, TypeF32(state), ndc, divided, ConstantF32Value(state, 1.0f));
    return ndc;
}

std::uint32_t RemapPositionDepth(SpirvEmitterState& state, std::uint32_t position) {
    const auto& remap = state.inputInfo.vertex->depthRemap;
    const auto f32 = TypeF32(state);
    const auto u32 = TypeU32(state);
    const auto boolean = TypeBool(state);
    std::array<std::uint32_t, 4> components{};
    for (std::uint32_t i = 0; i < 4u; i++) {
        components.at(i) = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, f32, components.at(i), position, i);
    }
    const auto mode = state.module.SpecializationConstant(u32, PipelineSpecialization::DepthRemapBase, remap.mode);
    if (state.pushConstantVariable == 0u) throw std::runtime_error("depth remap push constants were not declared");
    const auto pushFloat = [&](std::uint32_t dword) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, DepthRemapPushOffsetBytes / 4u + dword));
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, bits, pointer);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, f32, value, bits);
        return value;
    };
    const auto scale = pushFloat(0u);
    const auto offset = pushFloat(1u);
    const auto scaled = state.module.AllocateId();
    state.module.AddFunction(spv::OpFMul, f32, scaled, components[2], scale);
    const auto shifted = state.module.AllocateId();
    state.module.AddFunction(spv::OpFMul, f32, shifted, components[3], offset);
    const auto remapped = state.module.AllocateId();
    state.module.AddFunction(spv::OpFAdd, f32, remapped, scaled, shifted);
    const auto active = state.module.AllocateId();
    state.module.AddFunction(spv::OpINotEqual, boolean, active, mode, ConstantU32(state, 0u));
    const auto z = Select(state, f32, active, remapped, components[2]);
    const auto symmetric = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, boolean, symmetric, mode, ConstantU32(state, 2u));
    const auto nearSymmetric = state.module.AllocateId();
    state.module.AddFunction(spv::OpFAdd, f32, nearSymmetric, components[3], components[2]);
    const auto near = Select(state, f32, symmetric, nearSymmetric, components[2]);
    const auto far = state.module.AllocateId();
    state.module.AddFunction(spv::OpFSub, f32, far, components[3], components[2]);
    const auto inside = ConstantF32Value(state, 1.0f);
    const auto storeDistance = [&](std::uint32_t index, std::uint32_t distance) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, f32), pointer, state.clipDistanceVariable, ConstantU32(state, index));
        state.module.AddFunction(spv::OpStore, pointer, Select(state, f32, active, distance, inside));
    };
    if (state.clipDistanceVariable == 0u) throw std::runtime_error("depth remap clip distances were not declared");
    storeDistance(state.depthRemapClipIndex, near);
    storeDistance(state.depthRemapClipIndex + 1u, far);
    const auto converted = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4u), converted, components[0], components[1], z, components[3]);
    return converted;
}

std::uint32_t ConvertPositionToClipSpace(SpirvEmitterState& state, std::uint32_t position) {
    if (state.inputInfo.vertex == nullptr) {
        throw std::runtime_error("vertex input info is missing for clip-space conversion");
    }
    const auto& transform = state.inputInfo.vertex->clipSpace;
    std::array<std::uint32_t, 4> components {};
    for (std::uint32_t i = 0; i < 4u; i++) {
        components.at(i) = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), components.at(i), position, i);
    }
    components[0] = ConvertClipCoordinate(state, components[0], transform.scale[0], transform.offset[0], transform.halfExtent[0]);
    components[1] = ConvertClipCoordinate(state, components[1], transform.scale[1], transform.offset[1], transform.halfExtent[1]);
    const auto converted = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, TypeF32Vector(state, 4u), converted, components[0], components[1], components[2], components[3]);
    return converted;
}

}

void FlushColorTargetMerge(SpirvEmitterState& state) {
    if (state.mergePendingVariable == 0u) return;
    const auto pending = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), pending, state.mergePendingVariable);
    auto write = Binary(state, spv::OpINotEqual, TypeBool(state), pending, ConstantU32(state, 0u));
    if (state.pixelValidMaskVariable != 0u) {
        const auto valid = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), valid, state.pixelValidMaskVariable);
        write = Binary(state, spv::OpLogicalAnd, TypeBool(state), write, Binary(state, spv::OpINotEqual, TypeBool(state), valid, ConstantU32(state, 0u)));
    }
    EmitIfCondition(state, write, [&] {
        const auto [resource, coord, sample] = MergeTargetTexel(state);
        const auto merged = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), merged, state.mergeWordVariable);
        const auto texel = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 4u), texel, merged, ConstantU32(state, 0u), ConstantU32(state, 0u), ConstantU32(state, 0u));
        EmitStorageImageWrite(state, resource, 0u, coord, texel, sample);
    });
}

namespace {

std::uint32_t MeshArray(SpirvEmitterState& state, std::uint32_t storage, std::uint32_t type, std::uint32_t count) {
    const auto array = state.module.Type(spv::OpTypeArray, type, ConstantU32(state, count));
    return state.module.DefineGlobalVariable(TypePointer(state, storage, array), storage);
}

std::uint32_t MeshElement(SpirvEmitterState& state, std::uint32_t variable, std::uint32_t storage, std::uint32_t type, std::uint32_t index) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, storage, type), pointer, variable, index);
    return pointer;
}

std::uint32_t MeshLoad(SpirvEmitterState& state, std::uint32_t variable, std::uint32_t storage, std::uint32_t type, std::uint32_t index) {
    const auto pointer = MeshElement(state, variable, storage, type, index);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, type, value, pointer);
    return value;
}

bool IsMeshDistance(StageOutputKind kind) {
    return kind == StageOutputKind::ClipDistance || kind == StageOutputKind::CullDistance;
}

std::uint32_t MeshOutputType(SpirvEmitterState& state, StageOutputKind kind) {
    if (kind == StageOutputKind::Layer) {
        return TypeU32(state);
    }
    return IsMeshDistance(kind) || kind == StageOutputKind::PointSize ? TypeF32(state) : TypeF32Vector(state, 4u);
}

std::uint32_t MeshDistancePointer(SpirvEmitterState& state, const SpirvOutputBinding& output, std::uint32_t vertex) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeF32(state)), pointer, output.variableId, vertex, ConstantU32(state, output.index));
    return pointer;
}

}

namespace {

std::uint32_t TessellationOuterFactors(const SpirvEmitterState& state) {
    return state.inputInfo.vertex->tess.domain == 2u ? 4u : 3u;
}

std::uint32_t TessellationPointer(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto kind = static_cast<TessellationAttribute>(inst.Argument(0)->ImmediateU32());
    const auto variable = state.tessVariables.at(static_cast<std::uint32_t>(kind));
    if (variable == 0u) {
        throw std::runtime_error("tessellation attribute has no interface variable");
    }
    const bool input = kind == TessellationAttribute::ControlInput || kind == TessellationAttribute::EvaluationInput || kind == TessellationAttribute::PatchInput || kind == TessellationAttribute::InstanceInput;
    const auto storage = input ? spv::StorageClassInput : spv::StorageClassOutput;
    if (kind == TessellationAttribute::InstanceOutput) {
        return variable;
    }
    const auto pointer = state.module.AllocateId();
    if (kind == TessellationAttribute::InstanceInput) {
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, storage, TypeU32(state)), pointer, variable, ConstantU32(state, 0u));
        return pointer;
    }
    if (kind == TessellationAttribute::Factor) {
        if (!inst.Argument(1)->HasImmediate()) {
            throw std::runtime_error("tessellation factor index must be immediate");
        }
        const auto index = inst.Argument(1)->ImmediateU32() / 4u;
        const auto outerFactors = TessellationOuterFactors(state);
        const bool outer = index < outerFactors;
        if (index >= outerFactors + (outerFactors == 4u ? 2u : 1u)) {
            throw std::runtime_error("tessellation factor index out of range");
        }
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, storage, TypeF32(state)), pointer, outer ? variable : state.tessInnerVariable, ConstantU32(state, outer ? index : index - outerFactors));
        return pointer;
    }
    const auto address = ctx.Arg(inst, 1);
    const bool patch = kind == TessellationAttribute::PatchOutput || kind == TessellationAttribute::PatchInput || kind == TessellationAttribute::GeometryOutput;
    const bool local = kind == TessellationAttribute::LocalOutput || kind == TessellationAttribute::ControlInput;
    const auto stride = local ? state.program.Metadata().tessellationLocalStride : kind == TessellationAttribute::ControlShared ? state.program.Metadata().tessellationSharedStride : state.program.Metadata().tessellationControlStride;
    const auto type = TypePointer(state, storage, TypeU32(state));
    const bool controlRead = kind == TessellationAttribute::ControlOutput && inst.Opcode() == IrOpcode::GetTessellationAttribute;
    const auto inPatch = [&](std::uint32_t vertex) {
        if (kind != TessellationAttribute::ControlInput && kind != TessellationAttribute::EvaluationInput && !controlRead) return vertex;
        const auto& tess = state.inputInfo.vertex->tess;
        const auto clamped = state.module.AllocateId();
        state.module.AddFunction(spv::OpExtInst, TypeU32(state), clamped, GlslStd450(state), GLSLstd450UMin, vertex, ConstantU32(state, (kind == TessellationAttribute::ControlInput ? tess.inputControlPoints : tess.outputControlPoints) - 1u));
        return clamped;
    };
    const bool sharedRead = kind == TessellationAttribute::ControlShared && inst.Opcode() == IrOpcode::GetTessellationAttribute;
    const auto* sum = inst.Argument(1);
    const auto* scaled = sum->Opcode() == IrOpcode::IAdd32 ? sum->Argument(0) : nullptr;
    if ((kind == TessellationAttribute::ControlInput || sharedRead || controlRead) && scaled != nullptr && sum->Argument(1)->HasImmediate() && scaled->Opcode() == IrOpcode::IMul32 && scaled->Argument(1)->HasImmediate() && scaled->Argument(1)->ImmediateU32() == stride) {
        const auto constant = sum->Argument(1)->ImmediateU32();
        const auto vertex = inPatch(EmitBinaryU32(state, spv::OpIAdd, ctx.Arg(*scaled, 0), ConstantU32(state, constant / stride)));
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, vertex, ConstantU32(state, constant % stride / 16u), ConstantU32(state, constant % stride % 16u / 4u));
        return pointer;
    }
    if (inst.Argument(1)->HasImmediate()) {
        const auto value = inst.Argument(1)->ImmediateU32();
        const auto offset = patch ? value : value % stride;
        const auto attribute = ConstantU32(state, offset >> 4u);
        const auto component = ConstantU32(state, (offset >> 2u) & 3u);
        if (kind == TessellationAttribute::LocalOutput || patch) {
            state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, attribute, component);
            return pointer;
        }
        const bool own = (kind == TessellationAttribute::ControlOutput && !controlRead) || (kind == TessellationAttribute::ControlShared && !sharedRead);
        const auto controlPoint = kind == TessellationAttribute::ControlInput ? std::min(value / stride, state.inputInfo.vertex->tess.inputControlPoints - 1u) : kind == TessellationAttribute::EvaluationInput || controlRead ? std::min(value / stride, state.inputInfo.vertex->tess.outputControlPoints - 1u) : value / stride;
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, own ? EmitLaneId(state) : ConstantU32(state, controlPoint), attribute, component);
        return pointer;
    }
    const auto offset = patch ? address : EmitBinaryU32(state, spv::OpUMod, address, ConstantU32(state, stride));
    const auto attribute = EmitBinaryU32(state, spv::OpShiftRightLogical, offset, ConstantU32(state, 4u));
    const auto component = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, offset, ConstantU32(state, 2u)), ConstantU32(state, 3u));
    if (kind == TessellationAttribute::LocalOutput || patch) {
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, attribute, component);
    } else {
        const bool own = (kind == TessellationAttribute::ControlOutput && !controlRead) || (kind == TessellationAttribute::ControlShared && !sharedRead);
        const auto vertex = own ? EmitLaneId(state) : inPatch(EmitBinaryU32(state, spv::OpUDiv, address, ConstantU32(state, stride)));
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, vertex, attribute, component);
    }
    return pointer;
}

}

void DefineTessellationInterfaces(SpirvEmitterState& state) {
    std::array<bool, 11> used {};
    for (const IrBlock* block : state.program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::GetTessellationAttribute && inst->Opcode() != IrOpcode::SetTessellationAttribute) {
                continue;
            }
            used.at(inst->Argument(0)->ImmediateU32()) = true;
        }
    }
    if (std::none_of(used.begin(), used.end(), [](bool value) { return value; })) {
        return;
    }
    const auto& tess = state.inputInfo.vertex->tess;
    const auto array = [&](std::uint32_t type, std::uint32_t count) {
        return state.module.Type(spv::OpTypeArray, type, ConstantU32(state, count));
    };
    for (std::uint32_t index = 0; index < used.size(); index++) {
        if (!used[index]) {
            continue;
        }
        const auto kind = static_cast<TessellationAttribute>(index);
        const bool input = kind == TessellationAttribute::ControlInput || kind == TessellationAttribute::EvaluationInput || kind == TessellationAttribute::PatchInput || kind == TessellationAttribute::InstanceInput;
        const bool patch = kind == TessellationAttribute::PatchOutput || kind == TessellationAttribute::PatchInput;
        const auto storage = input ? spv::StorageClassInput : spv::StorageClassOutput;
        const bool instance = kind == TessellationAttribute::InstanceOutput || kind == TessellationAttribute::InstanceInput;
        std::uint32_t type;
        if (kind == TessellationAttribute::Factor) {
            type = array(TypeF32(state), 4u);
        } else if (instance) {
            type = kind == TessellationAttribute::InstanceInput ? array(TypeU32(state), tess.inputControlPoints) : TypeU32(state);
        } else if (kind == TessellationAttribute::GeometryOutput) {
            type = array(TypeU32Vector(state, 4u), (tess.geometryItemDwords + 3u) / 4u);
        } else if (patch) {
            type = array(TypeU32Vector(state, 4u), (state.program.Metadata().tessellationPatchStride + 15u) / 16u);
        } else {
            const bool local = kind == TessellationAttribute::LocalOutput || kind == TessellationAttribute::ControlInput;
            const auto stride = local ? state.program.Metadata().tessellationLocalStride : kind == TessellationAttribute::ControlShared ? state.program.Metadata().tessellationSharedStride : state.program.Metadata().tessellationControlStride;
            type = array(TypeU32Vector(state, 4u), (stride + 15u) / 16u);
            if (kind != TessellationAttribute::LocalOutput) {
                type = array(type, local ? tess.inputControlPoints : tess.outputControlPoints);
            }
        }
        auto& variable = state.tessVariables.at(index);
        variable = DefineInterfaceVariable(state, type, storage, instance ? "tess_instance" : "tess_attributes");
        if (instance) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationLocation, (state.program.Metadata().tessellationLocalStride + 15u) / 16u);
        } else if (kind == TessellationAttribute::Factor) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, spv::BuiltInTessLevelOuter);
            state.tessInnerVariable = DefineInterfaceVariable(state, array(TypeF32(state), 2u), storage, "tess_inner");
            state.module.AddAnnotation(spv::OpDecorate, state.tessInnerVariable, spv::DecorationBuiltIn, spv::BuiltInTessLevelInner);
            state.module.AddAnnotation(spv::OpDecorate, state.tessInnerVariable, spv::DecorationPatch);
        } else {
            const auto controlLocations = (state.program.Metadata().tessellationControlStride + 15u) / 16u;
            const bool patchUsed = used[static_cast<std::uint32_t>(TessellationAttribute::PatchOutput)] || used[static_cast<std::uint32_t>(TessellationAttribute::PatchInput)];
            const auto patchLocations = patchUsed ? (state.program.Metadata().tessellationPatchStride + 15u) / 16u : 0u;
            const auto location = patch ? controlLocations : kind == TessellationAttribute::ControlShared ? controlLocations + patchLocations : 0u;
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationLocation, location);
        }
        if (kind == TessellationAttribute::Factor || patch) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationPatch);
        }
    }
}

void DefineTessellationExecutionModes(SpirvEmitterState& state) {
    const auto& tess = state.inputInfo.vertex->tess;
    if ((tess.domain != 1u && tess.domain != 2u) || (tess.partitioning != 0u && tess.partitioning != 2u && tess.partitioning != 3u) || (tess.outputTopology != 2u && tess.outputTopology != 3u)) {
        throw std::runtime_error("unsupported tessellation domain, partitioning, or output topology");
    }
    state.module.EmitCapability(spv::CapabilityTessellation);
    if (state.program.Resources().stage == IrShaderStage::TessellationControl) {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOutputVertices, tess.outputControlPoints);
    } else {
        state.module.AddExecutionMode(state.mainFunc, tess.domain == 2u ? spv::ExecutionModeQuads : spv::ExecutionModeTriangles);
        state.module.AddExecutionMode(state.mainFunc, tess.partitioning == 0u ? spv::ExecutionModeSpacingEqual : tess.partitioning == 3u ? spv::ExecutionModeSpacingFractionalEven : spv::ExecutionModeSpacingFractionalOdd);
        state.module.AddExecutionMode(state.mainFunc, tess.outputTopology == 3u ? spv::ExecutionModeVertexOrderCcw : spv::ExecutionModeVertexOrderCw);
    }
}

void DefineMeshOutputs(SpirvEmitterState& state) {
    if (state.inputInfo.vertex == nullptr) {
        throw std::runtime_error("vertex input info is missing for mesh output definition");
    }
    const auto& mesh = state.inputInfo.vertex->mesh;
    std::uint32_t clipDistanceCount = 0u;
    std::uint32_t cullDistanceCount = 0u;
    for (const auto& output : state.outputs) {
        if (output.kind == StageOutputKind::ClipDistance) {
            clipDistanceCount = std::max(clipDistanceCount, output.index + 1u);
        } else if (output.kind == StageOutputKind::CullDistance) {
            cullDistanceCount = std::max(cullDistanceCount, output.index + 1u);
        }
    }
    for (auto& output : state.outputs) {
        if (IsMeshDistance(output.kind)) {
            const bool clip = output.kind == StageOutputKind::ClipDistance;
            auto& variable = clip ? state.clipDistanceVariable : state.cullDistanceVariable;
            if (variable == 0u) {
                const auto distances = state.module.Type(spv::OpTypeArray, TypeF32(state), ConstantU32(state, clip ? clipDistanceCount : cullDistanceCount));
                variable = MeshArray(state, spv::StorageClassOutput, distances, mesh.maxVertices);
                state.interfaceVariables.push_back(variable);
                state.module.AddName(variable, clip ? "gl_ClipDistance" : "gl_CullDistance");
                state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, clip ? spv::BuiltInClipDistance : spv::BuiltInCullDistance);
            }
            output.variableId = variable;
            output.meshDataVariable = MeshArray(state, spv::StorageClassPrivate, TypeF32(state), state.laneCount);
            continue;
        }
        if (output.kind != StageOutputKind::Position && output.kind != StageOutputKind::Parameter && output.kind != StageOutputKind::Layer && output.kind != StageOutputKind::PointSize) {
            throw std::runtime_error("unsupported mesh output kind");
        }
        const auto type = MeshOutputType(state, output.kind);
        const bool shared = output.kind == StageOutputKind::Layer;
        output.meshDataVariable = MeshArray(state, shared ? spv::StorageClassWorkgroup : spv::StorageClassPrivate, type, shared ? mesh.maxVertices : state.laneCount);
        if (state.meshBarycentricVertex && output.kind == StageOutputKind::Parameter && std::ranges::find(state.meshExportedLocations, output.location) == state.meshExportedLocations.end()) {
            continue;
        }
        const auto layerBlock = output.kind == StageOutputKind::Layer ? state.module.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInLayer}}, {spv::OpMemberDecorate, {0u, spv::DecorationPerPrimitiveEXT}}, {spv::OpDecorate, {spv::DecorationBlock}}}, type) : 0u;
        output.variableId = MeshArray(state, spv::StorageClassOutput, output.kind == StageOutputKind::Layer ? layerBlock : type, output.kind == StageOutputKind::Layer ? mesh.maxPrimitives : mesh.maxVertices);
        state.interfaceVariables.push_back(output.variableId);
        state.module.AddName(output.variableId, output.debugName);
        if (output.kind == StageOutputKind::Parameter) {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationLocation, output.location);
        } else if (output.kind != StageOutputKind::Layer) {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationBuiltIn, output.kind == StageOutputKind::PointSize ? spv::BuiltInPointSize : spv::BuiltInPosition);
        }
        if (output.kind == StageOutputKind::Position) {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationInvariant);
        }
    }
    if (state.meshBarycentricVertex) {
        if (std::ranges::none_of(state.outputs, [](const SpirvOutputBinding& output) { return output.kind == StageOutputKind::Position; })) {
            throw std::runtime_error("barycentric emulation on the mesh path needs the mesh program to export a position");
        }
        const auto vec3 = TypeF32Vector(state, 3u);
        state.meshPositionStaging = MeshArray(state, spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), mesh.maxVertices);
        const auto defineClip = [&](std::uint32_t location, bool linear, const char* name) {
            if (location == BarycentricEmulationLayout::NoLocation) return 0u;
            const auto variable = MeshArray(state, spv::StorageClassOutput, vec3, mesh.maxVertices);
            state.interfaceVariables.push_back(variable);
            state.module.AddName(variable, name);
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationLocation, location);
            if (linear) state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationNoPerspective);
            return variable;
        };
        state.meshClipSmooth = defineClip(state.barycentricSmoothLocation, false, "meshBarycentricClipSmooth");
        state.meshClipLinear = defineClip(state.barycentricLinearLocation, true, "meshBarycentricClipLinear");
        if (state.barycentricPositionsLocation == BarycentricEmulationLayout::NoLocation) {
            throw std::runtime_error("barycentric emulation on the mesh path has no location for the corner positions");
        }
        const auto corners = state.module.Type(spv::OpTypeArray, TypeF32Vector(state, 4u), ConstantU32(state, 3u));
        state.meshFlatPositions = MeshArray(state, spv::StorageClassOutput, corners, mesh.maxPrimitives);
        state.interfaceVariables.push_back(state.meshFlatPositions);
        state.module.AddName(state.meshFlatPositions, "meshBarycentricPositions");
        state.module.AddAnnotation(spv::OpDecorate, state.meshFlatPositions, spv::DecorationLocation, state.barycentricPositionsLocation);
        state.module.AddAnnotation(spv::OpDecorate, state.meshFlatPositions, spv::DecorationPerPrimitiveEXT);
        for (const auto& [sourceLocation, relocated] : state.meshPerVertexLocations) {
            const auto source = std::ranges::find_if(state.outputs, [sourceLocation = sourceLocation](const SpirvOutputBinding& output) { return output.kind == StageOutputKind::Parameter && output.location == sourceLocation; });
            if (source == state.outputs.end()) {
                throw std::runtime_error("barycentric emulation on the mesh path: the mesh program does not export per-vertex parameter " + std::to_string(sourceLocation));
            }
            const auto staging = MeshArray(state, spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), mesh.maxVertices);
            const auto perPrimitive = MeshArray(state, spv::StorageClassOutput, corners, mesh.maxPrimitives);
            state.interfaceVariables.push_back(perPrimitive);
            state.module.AddName(perPrimitive, "meshPerVertexParameter" + std::to_string(sourceLocation));
            state.module.AddAnnotation(spv::OpDecorate, perPrimitive, spv::DecorationLocation, relocated);
            state.module.AddAnnotation(spv::OpDecorate, perPrimitive, spv::DecorationPerPrimitiveEXT);
            state.meshVertexParameters.push_back({source->meshDataVariable, staging, perPrimitive});
        }
    }
    state.meshAllocation = MeshArray(state, spv::StorageClassWorkgroup, TypeU32(state), 2u);
    state.meshPrimitiveData = MeshArray(state, spv::StorageClassPrivate, TypeU32(state), state.laneCount);
    state.meshPrimitives = MeshArray(state, spv::StorageClassOutput, mesh.outputPrimitiveSize == 1u ? TypeU32(state) : TypeU32Vector(state, mesh.outputPrimitiveSize), mesh.maxPrimitives);
    state.meshCull = MeshArray(state, spv::StorageClassOutput, TypeBool(state), mesh.maxPrimitives);
    state.interfaceVariables.push_back(state.meshPrimitives);
    state.interfaceVariables.push_back(state.meshCull);
    state.module.AddAnnotation(spv::OpDecorate, state.meshPrimitives, spv::DecorationBuiltIn, mesh.outputPrimitiveSize == 1u ? spv::BuiltInPrimitivePointIndicesEXT : mesh.outputPrimitiveSize == 2u ? spv::BuiltInPrimitiveLineIndicesEXT : spv::BuiltInPrimitiveTriangleIndicesEXT);
    state.module.AddAnnotation(spv::OpDecorate, state.meshCull, spv::DecorationBuiltIn, spv::BuiltInCullPrimitiveEXT);
    state.module.AddAnnotation(spv::OpDecorate, state.meshCull, spv::DecorationPerPrimitiveEXT);
    if (state.numWorkgroupsVariable == 0u) {
        state.numWorkgroupsVariable = DefineInterfaceVariable(state, TypeU32Vector(state, 3u), spv::StorageClassInput, "gl_NumWorkGroups");
        state.module.AddAnnotation(spv::OpDecorate, state.numWorkgroupsVariable, spv::DecorationBuiltIn, spv::BuiltInNumWorkgroups);
    }
}

void EmitPixelEntryPoint(SpirvEmitterState& state) {
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), state.mainFunc, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    state.module.AddFunction(spv::OpStore, state.pixelStopFlag, ConstantBool(state, false));
    state.module.AddFunction(spv::OpFunctionCall, TypeVoid(state), state.module.AllocateId(), state.pixelGuestFunc);
    if (state.pixelStopStored) {
        const auto stopped = state.module.AllocateId();
        const auto stop = state.module.AllocateId();
        const auto done = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeBool(state), stopped, state.pixelStopFlag);
        state.module.AddFunction(spv::OpSelectionMerge, done, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, stopped, stop, done);
        EmitLabel(state, stop);
        if (std::ranges::find(state.supportedExtensions, "SPV_KHR_terminate_invocation") != state.supportedExtensions.end()) {
            state.module.EmitExtension("SPV_KHR_terminate_invocation");
            state.module.AddFunction(spv::OpTerminateInvocation);
        } else {
            state.module.AddFunction(spv::OpKill);
        }
        EmitLabel(state, done);
    }
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
}

void EmitMeshEntryPoint(SpirvEmitterState& state) {
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), state.mainFunc, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshPrimitiveData, spv::StorageClassPrivate, TypeU32(state), ConstantU32(state, half)), ConstantU32(state, 0x80000000u));
        if (state.inputInfo.vertex->pointList) {
            const auto& vertex = *state.inputInfo.vertex;
            const bool fromVertex = (vertex.paClVsOutCntl & (1u << 16u)) != 0u;
            if (!fromVertex && !(vertex.pointSize > 0.0f)) FailEmit("mesh points need a positive PA_SU_POINT_SIZE");
            if (fromVertex && !(vertex.pointSizeMax > 0.0f)) FailEmit("mesh point sizes need a positive PA_SU_POINT_MINMAX maximum");
            const auto size = fromVertex ? std::clamp(vertex.pointSize, PointSizeFloor(vertex), vertex.pointSizeMax) : vertex.pointSize;
            state.laneHalf = half;
            state.module.AddFunction(spv::OpStore, MeshOutputPointer(state, StageOutputKind::PointSize), ConstantF32(state, std::bit_cast<std::uint32_t>(size)));
        }
    }
    const auto firstInvocation = state.module.AllocateId();
    state.laneHalf = 0;
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), firstInvocation, EmitLocalInvocationIndex(state), ConstantU32(state, 0u));
    EmitIfCondition(state, firstInvocation, [&]() {
        for (std::uint32_t field = 0; field < 2u; field++) {
            state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, field)), ConstantU32(state, 0u));
        }
    });
    state.module.AddFunction(spv::OpFunctionCall, TypeVoid(state), state.module.AllocateId(), state.meshGuestFunc);
    state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask));
    const auto& mesh = state.inputInfo.vertex->mesh;
    const auto clamp = [&](std::uint32_t value, std::uint32_t limit) {
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpExtInst, TypeU32(state), result, GlslStd450(state), GLSLstd450UMin, value, ConstantU32(state, limit));
        return result;
    };
    const auto groupsX = state.module.AllocateId();
    const auto groupsXPointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeU32(state)), groupsXPointer, state.numWorkgroupsVariable, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpLoad, TypeU32(state), groupsX, groupsXPointer);
    const auto noGroups = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), noGroups, groupsX, ConstantU32(state, 0u));
    const auto allocatedVertices = clamp(MeshLoad(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, 0u)), mesh.maxVertices);
    const auto vertices = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, TypeU32(state), vertices, noGroups, ConstantU32(state, 0u), allocatedVertices);
    const auto primitives = clamp(MeshLoad(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, 1u)), mesh.maxPrimitives);
    state.module.AddFunction(spv::OpSetMeshOutputsEXT, vertices, primitives);
    const auto anyVertex = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeU32(state), anyVertex, GlslStd450(state), GLSLstd450UMax, vertices, ConstantU32(state, 1u));
    const auto lastVertex = EmitBinaryU32(state, spv::OpISub, anyVertex, ConstantU32(state, 1u));
    const bool emulating = state.meshBarycentricVertex;
    const auto emitVertexPart = [&](std::uint32_t half) {
        state.laneHalf = half;
        const auto index = EmitLocalInvocationIndex(state);
        const auto isVertex = state.module.AllocateId();
        state.module.AddFunction(spv::OpULessThan, TypeBool(state), isVertex, index, vertices);
        EmitIfCondition(state, isVertex, [&]() {
            for (const auto& output : state.outputs) {
                if (output.kind == StageOutputKind::Layer || output.variableId == 0u) {
                    continue;
                }
                const auto type = MeshOutputType(state, output.kind);
                const auto value = MeshLoad(state, output.meshDataVariable, spv::StorageClassPrivate, type, ConstantU32(state, half));
                const auto pointer = IsMeshDistance(output.kind) ? MeshDistancePointer(state, output, index) : MeshElement(state, output.variableId, spv::StorageClassOutput, type, index);
                state.module.AddFunction(spv::OpStore, pointer, value);
            }
            if (!emulating) {
                return;
            }
            const auto position = std::ranges::find_if(state.outputs, [](const SpirvOutputBinding& output) { return output.kind == StageOutputKind::Position; });
            const auto value = MeshLoad(state, position->meshDataVariable, spv::StorageClassPrivate, TypeF32Vector(state, 4u), ConstantU32(state, half));
            state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshPositionStaging, spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), index), value);
            const auto clip = state.module.AllocateId();
            state.module.AddFunction(spv::OpVectorShuffle, TypeF32Vector(state, 3u), clip, value, value, 0u, 1u, 3u);
            for (const auto& parameter : state.meshVertexParameters) {
                const auto staged = MeshLoad(state, parameter[0], spv::StorageClassPrivate, TypeF32Vector(state, 4u), ConstantU32(state, half));
                state.module.AddFunction(spv::OpStore, MeshElement(state, parameter[1], spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), index), staged);
            }
            for (const auto variable : {state.meshClipSmooth, state.meshClipLinear}) {
                if (variable != 0u) {
                    state.module.AddFunction(spv::OpStore, MeshElement(state, variable, spv::StorageClassOutput, TypeF32Vector(state, 3u), index), clip);
                }
            }
        });
    };
    const auto emitPrimitivePart = [&](std::uint32_t half) {
        state.laneHalf = half;
        const auto index = EmitLocalInvocationIndex(state);
        const auto isPrimitive = state.module.AllocateId();
        state.module.AddFunction(spv::OpULessThan, TypeBool(state), isPrimitive, index, primitives);
        EmitIfCondition(state, isPrimitive, [&]() {
            const auto packed = MeshLoad(state, state.meshPrimitiveData, spv::StorageClassPrivate, TypeU32(state), ConstantU32(state, half));
            std::array<std::uint32_t, 3> vertex {};
            if (mesh.outputPrimitiveSize == 1u) {
                const auto field = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), field, packed, ConstantU32(state, 0u), ConstantU32(state, 9u));
                vertex.at(0) = state.module.AllocateId();
                state.module.AddFunction(spv::OpExtInst, TypeU32(state), vertex.at(0), GlslStd450(state), GLSLstd450UMin, field, lastVertex);
                const auto indicesPointer = MeshElement(state, state.meshPrimitives, spv::StorageClassOutput, TypeU32(state), index);
                state.module.AddFunction(spv::OpStore, indicesPointer, vertex.at(0));
            } else {
                for (std::uint32_t component = 0; component < mesh.outputPrimitiveSize; component++) {
                    const auto field = state.module.AllocateId();
                    state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), field, packed, ConstantU32(state, component * 10u), ConstantU32(state, 9u));
                    vertex.at(component) = state.module.AllocateId();
                    state.module.AddFunction(spv::OpExtInst, TypeU32(state), vertex.at(component), GlslStd450(state), GLSLstd450UMin, field, lastVertex);
                }
                const auto indices = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, mesh.outputPrimitiveSize), indices, std::span<const std::uint32_t>(vertex.data(), mesh.outputPrimitiveSize));
                const auto indicesPointer = MeshElement(state, state.meshPrimitives, spv::StorageClassOutput, TypeU32Vector(state, mesh.outputPrimitiveSize), index);
                state.module.AddFunction(spv::OpStore, indicesPointer, indices);
            }
            const auto nullBit = EmitBinaryU32(state, spv::OpBitwiseAnd, packed, ConstantU32(state, 0x80000000u));
            const auto culled = state.module.AllocateId();
            state.module.AddFunction(spv::OpINotEqual, TypeBool(state), culled, nullBit, ConstantU32(state, 0u));
            state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshCull, spv::StorageClassOutput, TypeBool(state), index), culled);
            for (const auto& output : state.outputs) {
                if (output.kind != StageOutputKind::Layer) {
                    continue;
                }
                const auto layer = MeshLoad(state, output.meshDataVariable, spv::StorageClassWorkgroup, TypeU32(state), vertex.at(state.inputInfo.vertex->mesh.provokingVertex));
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeU32(state)), pointer, output.variableId, index, ConstantU32(state, 0u));
                state.module.AddFunction(spv::OpStore, pointer, layer);
            }
            if (emulating) {
                const auto corners = state.module.Type(spv::OpTypeArray, TypeF32Vector(state, 4u), ConstantU32(state, 3u));
                std::array<std::uint32_t, 3> staged {};
                for (std::uint32_t component = 0; component < 3u; component++) {
                    staged.at(component) = MeshLoad(state, state.meshPositionStaging, spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), vertex.at(component));
                }
                const auto array = state.module.AllocateId();
                state.module.AddFunction(spv::OpCompositeConstruct, corners, array, staged[0], staged[1], staged[2]);
                state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshFlatPositions, spv::StorageClassOutput, corners, index), array);
                for (const auto& parameter : state.meshVertexParameters) {
                    std::array<std::uint32_t, 3> values {};
                    for (std::uint32_t component = 0; component < 3u; component++) {
                        values.at(component) = MeshLoad(state, parameter[1], spv::StorageClassWorkgroup, TypeF32Vector(state, 4u), vertex.at(component));
                    }
                    const auto perVertex = state.module.AllocateId();
                    state.module.AddFunction(spv::OpCompositeConstruct, corners, perVertex, values[0], values[1], values[2]);
                    state.module.AddFunction(spv::OpStore, MeshElement(state, parameter[2], spv::StorageClassOutput, corners, index), perVertex);
                }
            }
        });
    };
    if (!emulating) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            emitVertexPart(half);
            emitPrimitivePart(half);
        }
    } else {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            emitVertexPart(half);
        }
        state.laneHalf = 0;
        state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask));
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            emitPrimitivePart(half);
        }
    }
    state.laneHalf = 0;
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
}

void EmitMeshAllocate(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto first = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), first, EmitLocalInvocationIndex(state), ConstantU32(state, 0u));
    EmitIfCondition(state, first, [&]() {
        const auto allocation = ctx.Arg(inst, 0);
        for (std::uint32_t field = 0; field < 2u; field++) {
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), value, allocation, ConstantU32(state, field * 12u), ConstantU32(state, field == 0u ? 10u : 11u));
            const auto pointer = MeshElement(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, field));
            state.module.AddFunction(spv::OpStore, pointer, value);
        }
    });
}

std::uint32_t MeshOutputPointer(SpirvEmitterState& state, StageOutputKind kind, std::uint32_t index) {
    const auto output = std::find_if(state.outputs.begin(), state.outputs.end(), [=](const SpirvOutputBinding& binding) {
        return binding.kind == kind && binding.index == index;
    });
    if (output == state.outputs.end()) {
        throw std::runtime_error("mesh export has no output binding");
    }
    const bool shared = kind == StageOutputKind::Layer;
    return MeshElement(state, output->meshDataVariable, shared ? spv::StorageClassWorkgroup : spv::StorageClassPrivate, MeshOutputType(state, kind), shared ? EmitLocalInvocationIndex(state) : ConstantU32(state, state.laneHalf));
}

std::uint32_t MeshPrimitivePointer(SpirvEmitterState& state) {
    return MeshElement(state, state.meshPrimitiveData, spv::StorageClassPrivate, TypeU32(state), ConstantU32(state, state.laneHalf));
}

std::uint32_t EmitAndConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t mask) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), result, value, ConstantU32(state, mask));
    return result;
}

std::uint32_t EmitShiftRightConstant(SpirvEmitterState& state, std::uint32_t value, std::uint32_t shift) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), result, value, ConstantU32(state, shift));
    return result;
}

std::uint32_t EmitCompareU32Constant(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t value, std::uint32_t constant) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(opcode, TypeBool(state), result, value, ConstantU32(state, constant));
    return result;
}

std::uint32_t EmitSubConstantMinusU32(SpirvEmitterState& state, std::uint32_t constant, std::uint32_t value) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpISub, TypeU32(state), result, ConstantU32(state, constant), value);
    return result;
}

std::uint32_t EmitF32ToF16RtzBits(SpirvEmitterState& state, std::uint32_t f32) {
    const auto bits = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, f32);

    const auto sign = EmitAndConstant(state, EmitShiftRightConstant(state, bits, 16u), 0x8000u);
    const auto exponent = EmitAndConstant(state, EmitShiftRightConstant(state, bits, 23u), 0xffu);
    const auto mantissa = EmitAndConstant(state, bits, 0x007fffffu);

    const auto halfExponent = state.module.AllocateId();
    const auto normalExponent = state.module.AllocateId();
    const auto normalMantissa = EmitShiftRightConstant(state, mantissa, 13u);
    const auto normalPayload = state.module.AllocateId();
    const auto normal = state.module.AllocateId();
    state.module.AddFunction(spv::OpISub, TypeU32(state), halfExponent, exponent, ConstantU32(state, 112u));
    state.module.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), normalExponent, halfExponent, ConstantU32(state, 10u));
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), normalPayload, normalExponent, normalMantissa);
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), normal, sign, normalPayload);

    const auto mantissaWithHidden = Binary(state, spv::OpBitwiseOr, TypeU32(state), mantissa, ConstantU32(state, 0x00800000u));
    const auto rawSubShift = EmitSubConstantMinusU32(state, 126u, exponent);
    const auto exponentLt103 = EmitCompareU32Constant(state, spv::OpULessThan, exponent, 103u);
    const auto exponentGt112 = EmitCompareU32Constant(state, spv::OpUGreaterThan, exponent, 112u);
    const auto subShiftLow = Select(state, TypeU32(state), exponentLt103, ConstantU32(state, 31u), rawSubShift);
    const auto subShift = Select(state, TypeU32(state), exponentGt112, ConstantU32(state, 14u), subShiftLow);
    const auto subMantissa = state.module.AllocateId();
    const auto subnormal = state.module.AllocateId();
    state.module.AddFunction(spv::OpShiftRightLogical, TypeU32(state), subMantissa, mantissaWithHidden, subShift);
    state.module.AddFunction(spv::OpBitwiseOr, TypeU32(state), subnormal, sign, subMantissa);

    const auto nanPayload = Binary(state, spv::OpBitwiseOr, TypeU32(state), EmitShiftRightConstant(state, mantissa, 13u), ConstantU32(state, 0x0200u));
    const auto nan = Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, Binary(state, spv::OpBitwiseOr, TypeU32(state), ConstantU32(state, 0x7c00u), nanPayload));
    const auto inf = Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7c00u));
    const auto maxFinite = Binary(state, spv::OpBitwiseOr, TypeU32(state), sign, ConstantU32(state, 0x7bffu));
    const auto mantissaZero = EmitCompareU32Constant(state, spv::OpIEqual, mantissa, 0u);
    const auto special = Select(state, TypeU32(state), mantissaZero, inf, nan);

    const auto exponentLe112 = EmitCompareU32Constant(state, spv::OpULessThanEqual, exponent, 112u);
    const auto exponentGe143 = EmitCompareU32Constant(state, spv::OpUGreaterThanEqual, exponent, 143u);
    const auto exponentEq255 = EmitCompareU32Constant(state, spv::OpIEqual, exponent, 255u);
    const auto finite0 = Select(state, TypeU32(state), exponentLe112, subnormal, normal);
    const auto finite1 = Select(state, TypeU32(state), exponentLt103, sign, finite0);
    const auto finite2 = Select(state, TypeU32(state), exponentGe143, maxFinite, finite1);
    return EmitAndConstant(state, Select(state, TypeU32(state), exponentEq255, special, finite2), 0xffffu);
}

std::uint32_t EmitMinMaxU32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue) {
    const auto condition = Binary(state, maxValue ? spv::OpUGreaterThan : spv::OpULessThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU32(state), condition, lhs, rhs);
}

std::uint32_t EmitMinMaxI32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue) {
    const auto condition = Binary(state, maxValue ? spv::OpSGreaterThan : spv::OpSLessThan, TypeBool(state), lhs, rhs);
    return Select(state, TypeU32(state), condition, lhs, rhs);
}

F32Class EmitClassifyF32Bits(SpirvEmitterState& state, std::uint32_t bits) {
    F32Class cls;
    cls.bits = bits;
    const auto absBits = EmitAndConstant(state, cls.bits, 0x7fffffffu);
    cls.nan = EmitCompareU32Constant(state, spv::OpUGreaterThan, absBits, 0x7f800000u);
    cls.zero = EmitCompareU32Constant(state, spv::OpIEqual, absBits, 0u);
    return cls;
}

F32Class EmitClassifyF32(SpirvEmitterState& state, std::uint32_t value) {
    return EmitClassifyF32Bits(state, Unary(state, spv::OpBitcast, TypeU32(state), value));
}

std::uint32_t EmitClassMaskBitMatch(SpirvEmitterState& state, std::uint32_t mask, std::uint32_t bit, std::uint32_t classMatch) {
    const auto selected = EmitCompareU32Constant(state, spv::OpINotEqual, EmitAndConstant(state, mask, 1u << bit), 0u);
    return Binary(state, spv::OpLogicalAnd, TypeBool(state), selected, classMatch);
}

std::uint32_t EmitClassMaskF32(SpirvEmitterState& state, std::uint32_t value, std::uint32_t mask) {
    const auto bits = Unary(state, spv::OpBitcast, TypeU32(state), value);
    const auto signBits = EmitAndConstant(state, bits, 0x80000000u);
    const auto absBits = EmitAndConstant(state, bits, 0x7fffffffu);
    const auto exponentBits = EmitAndConstant(state, absBits, 0x7f800000u);
    const auto mantissaBits = EmitAndConstant(state, absBits, 0x007fffffu);
    const auto quietBits = EmitAndConstant(state, mantissaBits, 0x00400000u);

    const auto sign = EmitCompareU32Constant(state, spv::OpINotEqual, signBits, 0u);
    const auto positive = Unary(state, spv::OpLogicalNot, TypeBool(state), sign);
    const auto exponentZero = EmitCompareU32Constant(state, spv::OpIEqual, exponentBits, 0u);
    const auto exponentNonzero = Unary(state, spv::OpLogicalNot, TypeBool(state), exponentZero);
    const auto exponentInf = EmitCompareU32Constant(state, spv::OpIEqual, exponentBits, 0x7f800000u);
    const auto finiteExponent = Unary(state, spv::OpLogicalNot, TypeBool(state), exponentInf);
    const auto mantissaZero = EmitCompareU32Constant(state, spv::OpIEqual, mantissaBits, 0u);
    const auto mantissaNonzero = Unary(state, spv::OpLogicalNot, TypeBool(state), mantissaZero);
    const auto quiet = EmitCompareU32Constant(state, spv::OpINotEqual, quietBits, 0u);

    const auto nanCommon = Binary(state, spv::OpLogicalAnd, TypeBool(state), exponentInf, mantissaNonzero);
    const auto snan = Binary(state, spv::OpLogicalAnd, TypeBool(state), nanCommon, Unary(state, spv::OpLogicalNot, TypeBool(state), quiet));
    const auto qnan = Binary(state, spv::OpLogicalAnd, TypeBool(state), nanCommon, quiet);
    const auto inf = Binary(state, spv::OpLogicalAnd, TypeBool(state), exponentInf, mantissaZero);
    const auto normal = Binary(state, spv::OpLogicalAnd, TypeBool(state), exponentNonzero, finiteExponent);
    const auto denorm = Binary(state, spv::OpLogicalAnd, TypeBool(state), exponentZero, mantissaNonzero);
    const auto zero = EmitCompareU32Constant(state, spv::OpIEqual, absBits, 0u);

    std::uint32_t match = EmitClassMaskBitMatch(state, mask, 0u, snan);
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 1u, qnan));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 2u, Binary(state, spv::OpLogicalAnd, TypeBool(state), inf, sign)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 3u, Binary(state, spv::OpLogicalAnd, TypeBool(state), normal, sign)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 4u, Binary(state, spv::OpLogicalAnd, TypeBool(state), denorm, sign)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 5u, Binary(state, spv::OpLogicalAnd, TypeBool(state), zero, sign)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 6u, Binary(state, spv::OpLogicalAnd, TypeBool(state), zero, positive)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 7u, Binary(state, spv::OpLogicalAnd, TypeBool(state), denorm, positive)));
    match = Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 8u, Binary(state, spv::OpLogicalAnd, TypeBool(state), normal, positive)));
    return Binary(state, spv::OpLogicalOr, TypeBool(state), match, EmitClassMaskBitMatch(state, mask, 9u, Binary(state, spv::OpLogicalAnd, TypeBool(state), inf, positive)));
}

std::uint32_t EmitMinMaxF32Value(SpirvEmitterState& state, std::uint32_t lhs, std::uint32_t rhs, bool maxValue) {
    const auto lhsClass = EmitClassifyF32(state, lhs);
    const auto rhsClass = EmitClassifyF32(state, rhs);

    const auto numericCond = Binary(state, maxValue ? spv::OpFOrdGreaterThanEqual : spv::OpFOrdLessThan, TypeBool(state), lhs, rhs);
    const auto orderedBits = Select(state, TypeU32(state), numericCond, lhsClass.bits, rhsClass.bits);

    const auto bothZero = Binary(state, spv::OpLogicalAnd, TypeBool(state), lhsClass.zero, rhsClass.zero);
    const auto zeroBits = Binary(state, maxValue ? spv::OpBitwiseAnd : spv::OpBitwiseOr, TypeU32(state), lhsClass.bits, rhsClass.bits);
    const auto numericBits = Select(state, TypeU32(state), bothZero, zeroBits, orderedBits);

    const auto resultBits = Select(state, TypeU32(state), rhsClass.nan, lhsClass.bits, numericBits);
    return Unary(state, spv::OpBitcast, TypeF32(state), resultBits);
}

std::uint32_t EmitTrigCycleF32(SpirvEmitterState& state, std::uint32_t src, bool preserveSignedZero) {
    const auto rounded = state.module.AllocateId();
    const auto reduced = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), rounded, GlslStd450(state), GLSLstd450RoundEven, src);
    state.module.AddFunction(spv::OpFSub, TypeF32(state), reduced, src, rounded);
    if (!preserveSignedZero) {
        return reduced;
    }
    const auto bits = state.module.AllocateId();
    const auto absBits = state.module.AllocateId();
    const auto zero = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, src);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), absBits, bits, ConstantU32(state, 0x7fffffffu));
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), zero, absBits, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelect, TypeF32(state), result, zero, src, reduced);
    return result;
}

void EmitF16DenormPreserve(SpirvEmitterState& state) {
    if (state.f16DenormPreserveEmitted) return;
    state.module.EmitCapability(spv::CapabilityDenormPreserve);
    state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeDenormPreserve, 16u);
    state.f16DenormPreserveEmitted = true;
}

static std::uint32_t UnpackF16BitsToF32(SpirvEmitterState& state, std::uint32_t bits, bool nanBits) {
    if (!state.float16DenormPreserve) return EmitF16BitsToF32Decoded(state, bits);
    EmitF16DenormPreserve(state);
    const auto u32 = TypeU32(state);
    const auto f32 = TypeF32(state);
    const auto unpacked = state.module.AllocateId();
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32Vector(state, 2), unpacked, GlslStd450(state), GLSLstd450UnpackHalf2x16, bits);
    state.module.AddFunction(spv::OpCompositeExtract, f32, value, unpacked, 0u);
    if (!nanBits) return value;
    const auto signAndPayload = Binary(state, spv::OpShiftRightArithmetic, u32, Binary(state, spv::OpShiftLeftLogical, u32, bits, ConstantU32(state, 16u)), ConstantU32(state, 3u));
    const auto nan = Unary(state, spv::OpBitcast, f32, Binary(state, spv::OpBitwiseOr, u32, signAndPayload, ConstantU32(state, 0x7f800000u)));
    return Select(state, f32, Unary(state, spv::OpIsNan, TypeBool(state), value), nan, value);
}

std::uint32_t EmitF16BitsToF32(SpirvEmitterState& state, std::uint32_t bits) {
    return UnpackF16BitsToF32(state, bits, true);
}

std::uint32_t EmitF16BitsToF32AnyNan(SpirvEmitterState& state, std::uint32_t bits) {
    return UnpackF16BitsToF32(state, bits, false);
}

std::uint32_t EmitF16BitsToF32Decoded(SpirvEmitterState& state, std::uint32_t bits) {
    const auto u32 = TypeU32(state);
    const auto f32 = TypeF32(state);
    const auto op = [&](spv::Op opcode, std::uint32_t lhs, std::uint32_t rhs) { return Binary(state, opcode, u32, lhs, rhs); };
    const auto sign = op(spv::OpShiftLeftLogical, op(spv::OpBitwiseAnd, bits, ConstantU32(state, 0x8000u)), ConstantU32(state, 16u));
    const auto exponent = op(spv::OpBitwiseAnd, op(spv::OpShiftRightLogical, bits, ConstantU32(state, 10u)), ConstantU32(state, 0x1fu));
    const auto mantissa = op(spv::OpBitwiseAnd, bits, ConstantU32(state, 0x3ffu));
    const auto mantissa32 = op(spv::OpShiftLeftLogical, mantissa, ConstantU32(state, 13u));
    const auto normal = op(spv::OpBitwiseOr, op(spv::OpShiftLeftLogical, op(spv::OpIAdd, exponent, ConstantU32(state, 127u - 15u)), ConstantU32(state, 23u)), mantissa32);
    const auto special = op(spv::OpBitwiseOr, ConstantU32(state, 0x7f800000u), mantissa32);
    const auto subnormal = Unary(state, spv::OpBitcast, u32, Binary(state, spv::OpFMul, f32, Unary(state, spv::OpConvertUToF, f32, mantissa), ConstantF32Value(state, 0x1p-24f)));
    const auto boolean = TypeBool(state);
    const auto finite = Select(state, u32, Binary(state, spv::OpIEqual, boolean, exponent, ConstantU32(state, 0u)), subnormal, normal);
    const auto magnitude = Select(state, u32, Binary(state, spv::OpIEqual, boolean, exponent, ConstantU32(state, 0x1fu)), special, finite);
    return Unary(state, spv::OpBitcast, f32, op(spv::OpBitwiseOr, sign, magnitude));
}

void EmitProgram(SpirvEmitterState& state) {
    const auto& program = state.program;
    SpirvValueEmitContext ctx(state);
    SpirvValueEmitContext high(state);
    if (state.laneCount == 2u) {
        ctx.otherHalf = &high;
        high.otherHalf = &ctx;
        high.half = 1u;
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.requirements.pixelValidMask) {
        state.pixelValidMaskVariable = state.module.AllocateId();
        state.module.AddName(state.pixelValidMaskVariable, "pixel_valid_mask_active");
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->mergeMask != 0u) {
        state.mergeWordVariable = state.module.AllocateId();
        state.mergePendingVariable = state.module.AllocateId();
    }
    for (const IrBlock* block : program.BlockOrder()) {
        const auto label = state.module.AllocateId();
        state.labels.emplace(block, label);
    }
    DefineGetBdaPointer(state);
    DefineDot2F32F16Function(state);
    DefineReciprocalCorrectionFunction(state);
    for (const IrBlock* block : program.BlockOrder()) {
        const bool needsScratch = std::any_of(block->Instructions().begin(), block->Instructions().end(), [](const IrValue* inst) {
            return inst->Opcode() == IrOpcode::SwizzleU32;
        });
        if (needsScratch) {
            ctx.scratchU32Variable = state.module.AllocateId();
            if (state.laneCount == 2u) {
                high.scratchU32Variable = state.module.AllocateId();
            }
            break;
        }
    }
    if (program.Resources().stage == IrShaderStage::Pixel && state.bdaStopsInvocations && !OrderedPixelShader(state)) {
        state.pixelGuestFunc = state.module.AllocateId();
        state.pixelStopFlag = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassPrivate, TypeBool(state)), spv::StorageClassPrivate);
        if (state.spirvVersion >= 0x00010400u) state.interfaceVariables.push_back(state.pixelStopFlag);
        state.module.AddName(state.pixelStopFlag, "bda_stopped");
    }
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), state.meshGuestFunc != 0u ? state.meshGuestFunc : state.pixelGuestFunc != 0u ? state.pixelGuestFunc : state.mainFunc, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.entryLabel);
    if (state.requirements.functionLds && !state.pixelFunctionLdsBuffer) {
        state.module.AddFunction(spv::OpVariable, TypeU32ArrayPointer(state, spv::StorageClassFunction, LdsStorageDwords(state)), state.ldsVariable, spv::StorageClassFunction);
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddFunction(spv::OpVariable, TypeU32ArrayPointer(state, spv::StorageClassFunction, state.program.Info().scratchDwords), state.scratchVariable.at(half), spv::StorageClassFunction);
        }
    }
    if (const auto window = state.program.Metadata().vectorWindowSize; window != 0u) {
        const auto indexed = state.vectorWindowIndexed != 0u ? state.vectorWindowIndexed : window;
        const auto array = state.module.Type(spv::OpTypeArray, TypeU32(state), ConstantU32(state, indexed));
        const auto zero = state.module.Constant(spv::OpConstantNull, array);
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, array), state.vectorWindowVariable.at(half), spv::StorageClassFunction, zero);
            for (const auto slot : state.vectorWindowSlots.at(half)) {
                state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), slot, spv::StorageClassFunction, ConstantU32(state, 0u));
            }
        }
    }
    if (state.pixelValidMaskVariable != 0u) {
        state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), state.pixelValidMaskVariable, spv::StorageClassFunction);
    }
    if (state.mergePendingVariable != 0u) {
        state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), state.mergeWordVariable, spv::StorageClassFunction);
        state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), state.mergePendingVariable, spv::StorageClassFunction, ConstantU32(state, 0u));
    }
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        auto& lane = half == 0u ? ctx : high;
        if (lane.scratchU32Variable != 0u) {
            state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), lane.scratchU32Variable, spv::StorageClassFunction);
        }
    }
    EmitLdsBufferBase(state);
    if (state.requirements.ldsLock) {
        const auto firstInvocation = state.module.AllocateId();
        state.module.AddFunction(spv::OpIEqual, TypeBool(state), firstInvocation, EmitLocalInvocationIndex(state), ConstantU32(state, 0u));
        EmitIfCondition(state, firstInvocation, [&] {
            state.module.AddFunction(spv::OpStore, EmitLdsLockPointer(state), ConstantU32(state, 0u));
        });
        state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | LdsMemorySemantics(state)));
    }
    if (state.gdsVariable != 0u) {
        state.gdsLength = state.module.AllocateId();
        state.module.AddFunction(spv::OpArrayLength, TypeU32(state), state.gdsLength, state.gdsVariable, 0u);
    }
    if (state.pixelValidMaskVariable != 0u) {
        state.module.AddFunction(spv::OpStore, state.pixelValidMaskVariable, ConstantU32(state, 1u));
    }
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->ReordersSamples()) {
        const auto& order = state.inputInfo.pixel->sampleOrder;
        const auto parity = [&](std::uint32_t component) {
            const auto value = Unary(state, spv::OpBitcast, TypeF32(state), EmitBuiltinU32(state, StageInputKind::FragCoord, component));
            return EmitBinaryU32(state, spv::OpBitwiseAnd, Unary(state, spv::OpConvertFToU, TypeU32(state), value), ConstantU32(state, 1u));
        };
        const auto quad = EmitBinaryU32(state, spv::OpBitwiseOr, parity(0u), EmitBinaryU32(state, spv::OpShiftLeftLogical, parity(1u), ConstantU32(state, 1u)));
        const auto word = [&](std::size_t pixel) { return ConstantU32(state, order[pixel] == 0u ? 0x76543210u : order[pixel]); };
        auto selected = word(3u);
        for (std::uint32_t pixel = 3u; pixel-- > 0u;) selected = Select(state, TypeU32(state), EmitCompareU32Constant(state, spv::OpIEqual, quad, pixel), word(pixel), selected);
        state.sampleOrderWord = selected;
    }
    if (AlphaToCoverage(state)) StoreCoverage(state, ConstantI32(state, -1));
    EmitBdaAnchors(state);
    if (state.pointSizeVariable != 0u && state.inputInfo.vertex != nullptr && state.inputInfo.vertex->pointList) {
        const auto& vertex = *state.inputInfo.vertex;
        const bool fromVertex = (vertex.paClVsOutCntl & (1u << 16u)) != 0u;
        if (!fromVertex && !(vertex.pointSize > 0.0f)) FailEmit("a point list without a vertex point size needs a positive PA_SU_POINT_SIZE");
        if (fromVertex && !(vertex.pointSizeMax > 0.0f)) FailEmit("a vertex point size needs a positive PA_SU_POINT_MINMAX maximum");
        const auto size = fromVertex ? std::clamp(vertex.pointSize, PointSizeFloor(vertex), vertex.pointSizeMax) : vertex.pointSize;
        state.module.AddFunction(spv::OpStore, state.pointSizeVariable, ConstantF32(state, std::bit_cast<std::uint32_t>(size)));
    }
    if (OrderedPixelShader(state)) {
        state.module.AddFunction(spv::OpBeginInvocationInterlockEXT);
    }
    EmitMemoryOffsets(state);
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->QuadMasked()) {
        const auto& pixel = *state.inputInfo.pixel;
        const auto quad = EmitQuadPixel(state);
        auto bit = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, ConstantU32(state, pixel.quadPixelMask), quad), ConstantU32(state, 1u));
        if (pixel.QuadSampleMaskPartial()) {
            const auto top = ConstantU32(state, static_cast<std::uint32_t>(pixel.quadSampleMask[0]) | (static_cast<std::uint32_t>(pixel.quadSampleMask[1]) << 16u));
            const auto bottom = ConstantU32(state, static_cast<std::uint32_t>(pixel.quadSampleMask[2]) | (static_cast<std::uint32_t>(pixel.quadSampleMask[3]) << 16u));
            const auto lower = EmitCompareU32Constant(state, spv::OpIEqual, EmitBinaryU32(state, spv::OpBitwiseAnd, quad, ConstantU32(state, 2u)), 2u);
            const auto row = Select(state, TypeU32(state), lower, bottom, top);
            const auto shift = EmitBinaryU32(state, spv::OpShiftLeftLogical, EmitBinaryU32(state, spv::OpBitwiseAnd, quad, ConstantU32(state, 1u)), ConstantU32(state, 4u));
            const auto covered = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, row, shift), ConstantU32(state, 0xffffu));
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeI32(state)), pointer, state.sampleMaskInputVariable, ConstantU32(state, 0u));
            const auto rasterized = state.module.AllocateId();
            state.module.AddFunction(spv::OpLoad, TypeI32(state), rasterized, pointer);
            auto guestRasterized = Unary(state, spv::OpBitcast, TypeU32(state), rasterized);
            if (state.sampleOrderWord != 0u) guestRasterized = ReorderedSampleBits(state, guestRasterized, false);
            bit = EmitBinaryU32(state, spv::OpBitwiseAnd, guestRasterized, covered);
            state.quadSampleCoverage = Unary(state, spv::OpBitcast, TypeI32(state), covered);
        }
        const auto mask = static_cast<std::uint32_t>(pixel.quadPixelMask);
        const bool demoteCapability = std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDemoteToHelperInvocation)) != state.supportedCapabilities.end();
        const bool demoteCore = state.spirvVersion >= 0x00010600u;
        const bool demoteExtension = std::find(state.supportedExtensions.begin(), state.supportedExtensions.end(), "SPV_EXT_demote_to_helper_invocation") != state.supportedExtensions.end();
        const bool demote = demoteCapability && (demoteCore || demoteExtension);
        if (!demote && state.requirements.neighbourLaneReads) {
            throw std::runtime_error("the PA_SC_AA_MASK quad pixel mask " + std::to_string(mask) + " leaves helper pixels whose lanes the pixel program reads (implicit-LOD sampling, whole-quad mode or cross-lane reads), which needs the device's shaderDemoteToHelperInvocation");
        }
        const auto uncovered = state.module.AllocateId();
        state.module.AddFunction(spv::OpIEqual, TypeBool(state), uncovered, bit, ConstantU32(state, 0u));
        state.quadPixelUncovered = uncovered;
        const auto killLabel = state.module.AllocateId();
        const auto mergeLabel = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, uncovered, killLabel, mergeLabel);
        EmitLabel(state, killLabel);
        if (demote) {
            state.module.EmitCapability(spv::CapabilityDemoteToHelperInvocation);
            if (!demoteCore) state.module.EmitExtension("SPV_EXT_demote_to_helper_invocation");
            state.module.AddFunction(spv::OpDemoteToHelperInvocation);
            state.module.AddFunction(spv::OpBranch, mergeLabel);
        } else {
            state.module.AddFunction(spv::OpKill);
        }
        EmitLabel(state, mergeLabel);
        if (state.quadSampleCoverage != 0u) StoreCoverage(state, ConstantI32(state, -1));
    }
    if (program.BlockOrder().empty()) {
        FlushColorTargetMerge(state);
        if (OrderedPixelShader(state)) {
            state.module.AddFunction(spv::OpEndInvocationInterlockEXT);
        }
        if (state.pixelValidMaskVariable != 0u) {
            const auto maskValue = state.module.AllocateId();
            const auto active = state.module.AllocateId();
            state.module.AddFunction(spv::OpLoad, TypeU32(state), maskValue, state.pixelValidMaskVariable);
            state.module.AddFunction(spv::OpINotEqual, TypeBool(state), active, maskValue, ConstantU32(state, 0u));
            const auto inactive = state.module.AllocateId();
            const auto killLabel = state.module.AllocateId();
            const auto mergeLabel = state.module.AllocateId();
            state.module.AddFunction(spv::OpLogicalNot, TypeBool(state), inactive, active);
            state.module.AddFunction(spv::OpSelectionMerge, mergeLabel, spv::SelectionControlMaskNone);
            state.module.AddFunction(spv::OpBranchConditional, inactive, killLabel, mergeLabel);
            EmitLabel(state, killLabel);
            state.module.AddFunction(spv::OpKill);
            EmitLabel(state, mergeLabel);
        }
        state.module.AddFunction(spv::OpReturn);
    } else {
        StructuredFunctionState functionState;
        EmitControlFlow(ctx, functionState, program);
    }
    state.module.AddFunction(spv::OpFunctionEnd);
    if (state.pixelGuestFunc != 0u) EmitPixelEntryPoint(state);
    if (state.program.Resources().stage == IrShaderStage::Mesh) {
        EmitMeshEntryPoint(state);
    }
}

void EmitLabel(SpirvEmitterState& state, std::uint32_t label) {
    state.currentLabel = label;
    state.module.AddFunction(spv::OpLabel, label);
}

std::uint32_t Unary(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t type, std::uint32_t value) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(opcode, type, result, value);
    return result;
}

std::uint32_t Binary(SpirvEmitterState& state, std::uint32_t opcode, std::uint32_t type, std::uint32_t lhs, std::uint32_t rhs) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(opcode, type, result, lhs, rhs);
    return result;
}

std::uint32_t Select(SpirvEmitterState& state, std::uint32_t type, std::uint32_t condition, std::uint32_t trueValue, std::uint32_t falseValue) {
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, type, result, condition, trueValue, falseValue);
    return result;
}

std::uint32_t EmitMeshDrawParameter(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto index = inst.Argument(0)->ImmediateU32();
    if (state.program.Resources().stage != IrShaderStage::Mesh || index >= PushData::MeshDrawDwordCount) {
        ctx.Fail(inst, "invalid mesh draw parameter");
    }
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, MeshDrawPushOffsetBytes / 4u + index));
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), result, pointer);
    return result;
}

namespace {

std::uint32_t MeshPushAddress(SpirvEmitterState& state, std::uint32_t first) {
    const auto push = [&](std::uint32_t dword) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, MeshDrawPushOffsetBytes / 4u + dword));
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return Unary(state, spv::OpUConvert, TypeScalarU64(state), value);
    };
    return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), push(first), Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), push(first + 1u), ConstantU32(state, 32u)));
}

std::uint32_t MeshArgumentAddress(SpirvEmitterState& state) {
    const auto push = [&](std::uint32_t dword) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, MeshDrawPushOffsetBytes / 4u + dword));
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return Unary(state, spv::OpUConvert, TypeScalarU64(state), value);
    };
    const auto low = push(MeshArgumentAddressDword);
    const auto high = push(MeshArgumentAddressDword + 1u);
    return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), low, Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), high, ConstantU32(state, 32u)));
}

std::uint32_t LoadMeshArgumentWord(SpirvEmitterState& state, std::uint32_t address, std::uint32_t dword) {
    const auto element = Binary(state, spv::OpIAdd, TypeScalarU64(state), address, Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), Unary(state, spv::OpUConvert, TypeScalarU64(state), dword), ConstantU32(state, 2u)));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state)), pointer, element);
    const auto loaded = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), loaded, pointer, spv::MemoryAccessAlignedMask, 4u);
    return loaded;
}

}

std::uint32_t EmitMeshRestartStart(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    if (state.program.Resources().stage != IrShaderStage::Mesh) {
        ctx.Fail(inst, "mesh restart table read outside a mesh shader");
    }
    return EmitValueOrZeroIfCondition(state, ctx.Arg(inst, 1), [&]() {
        const auto address = MeshArgumentAddress(state);
        const auto length = LoadMeshArgumentWord(state, address, ConstantU32(state, MeshRestartLengthDword));
        const auto last = Binary(state, spv::OpISub, TypeU32(state), length, ConstantU32(state, 1u));
        const auto position = state.module.AllocateId();
        state.module.AddFunction(spv::OpExtInst, TypeU32(state), position, GlslStd450(state), GLSLstd450UMin, ctx.Arg(inst, 0), last);
        return LoadMeshArgumentWord(state, address, Binary(state, spv::OpIAdd, TypeU32(state), position, ConstantU32(state, MeshRestartTableDword)));
    });
}

std::uint32_t EmitMeshCapturedWord(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    if (state.program.Resources().stage != IrShaderStage::Mesh) {
        ctx.Fail(inst, "captured geometry input read outside a mesh shader");
    }
    return EmitValueOrZeroIfCondition(state, ctx.Arg(inst, 1), [&]() {
        return LoadMeshArgumentWord(state, MeshPushAddress(state, MeshCapturedItemAddressDword), ctx.Arg(inst, 0));
    });
}

std::uint32_t EmitMeshArgument(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto index = inst.Argument(0)->ImmediateU32();
    if (state.program.Resources().stage != IrShaderStage::Mesh || index >= MeshArgumentBytes / 4u) {
        ctx.Fail(inst, "invalid mesh argument");
    }
    const auto address = MeshArgumentAddress(state);
    const auto present = Binary(state, spv::OpINotEqual, TypeBool(state), address, BdaConstant(state, 0u));
    const auto before = state.currentLabel;
    const auto loadLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, present, loadLabel, merge);
    EmitLabel(state, loadLabel);
    const auto element = Binary(state, spv::OpIAdd, TypeScalarU64(state), address, BdaConstant(state, 4ull * index));
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state)), pointer, element);
    const auto loaded = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), loaded, pointer, spv::MemoryAccessAlignedMask, 4u);
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeU32(state), value, loaded, loadLabel, ConstantU32(state, 0u), before);
    return value;
}

std::uint32_t EmitGetTessellationAttribute(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitValueOrZeroIfCondition(ctx.state, ctx.Arg(inst, 2), [&]() {
        const auto value = ctx.state.module.AllocateId();
        ctx.state.module.AddFunction(spv::OpLoad, TypeU32(ctx.state), value, TessellationPointer(ctx, inst));
        return value;
    });
}

namespace {

std::uint32_t TessellationLevel(SpirvEmitterState& state, std::uint32_t index) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, TessellationLevelPushOffsetBytes / 4u + index));
    const auto word = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), word, pointer);
    const auto level = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeF32(state), level, word);
    return level;
}

std::uint32_t TessellationLevelBound(SpirvEmitterState& state, std::uint32_t level, std::uint32_t operation, float limit) {
    const auto bounded = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), bounded, GlslStd450(state), operation, level, ConstantF32Value(state, limit));
    return bounded;
}

}

void EmitSetTessellationAttribute(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitIfCondition(ctx.state, ctx.Arg(inst, 3), [&]() {
        auto value = ctx.Arg(inst, 2);
        if (inst.Argument(0)->ImmediateU32() == static_cast<std::uint32_t>(TessellationAttribute::Factor)) {
            auto& state = ctx.state;
            if (state.program.Resources().stage != IrShaderStage::TessellationControl || state.pushConstantVariable == 0u) ctx.Fail(inst, "writes tessellation factors outside a hull program");
            const bool outer = inst.Argument(1)->HasImmediate() && inst.Argument(1)->ImmediateU32() / 4u < TessellationOuterFactors(state);
            const auto minimum = TessellationLevel(state, 0u);
            const auto lowest = outer ? minimum : TessellationLevelBound(state, minimum, GLSLstd450FMax, 1.0f);
            const auto floating = state.module.AllocateId();
            state.module.AddFunction(spv::OpBitcast, TypeF32(state), floating, value);
            const auto inside = state.module.AllocateId();
            state.module.AddFunction(spv::OpFOrdGreaterThan, TypeBool(state), inside, floating, ConstantF32Value(state, outer ? 0.0f : 1.0f));
            const auto raised = state.module.AllocateId();
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), raised, GlslStd450(state), GLSLstd450FMax, floating, lowest);
            const auto clamped = state.module.AllocateId();
            state.module.AddFunction(spv::OpExtInst, TypeF32(state), clamped, GlslStd450(state), GLSLstd450FMin, raised, TessellationLevelBound(state, TessellationLevel(state, 1u), GLSLstd450FMin, state.inputInfo.vertex->tess.partitioning == 2u ? 63.0f : 64.0f));
            value = state.module.AllocateId();
            state.module.AddFunction(spv::OpSelect, TypeF32(state), value, inside, clamped, outer ? floating : lowest);
        }
        ctx.state.module.AddFunction(spv::OpStore, TessellationPointer(ctx, inst), value);
    });
}

std::uint32_t EmitGetUserData(SpirvEmitterState& state, ScalarReg reg) {
    std::uint32_t dwordIndex = 0;
    if (!UserDataDwordIndex(state, reg, dwordIndex)) {
        return ConstantU32(state, 0u);
    }
    return EmitShaderDataDwordLoad(state, dwordIndex);
}

std::uint32_t EmitGetBuiltin(SpirvValueEmitContext& ctx, const IrValue* kind, const IrValue* index) {
    return EmitBuiltinU32(ctx.state, static_cast<StageInputKind>(kind->ImmediateU32()), index->ImmediateU32());
}

std::uint32_t EmitGetAttribute(SpirvValueEmitContext& ctx, const IrValue& inst) {
    if (inst.Flags<std::uint32_t>() == 1u) {
        auto& state = ctx.state;
        const auto attribute = inst.Argument(0)->ImmediateU32();
        const auto component = inst.Argument(1)->ImmediateU32();
        if (state.inputInfo.vertex == nullptr || !state.inputInfo.vertex->fetchEmbedded || component >= 4u) ctx.Fail(inst, "invalid prepared vertex attribute");
        const auto first = PipelineSpecialization::VertexBase + attribute * PipelineSpecialization::VertexWords;
        const auto selector = state.registrationModules ? EmitAndConstant(state, Binary(state, spv::OpShiftRightLogical, TypeU32(state), EmitShaderDataDwordLoad(state, state.program.Metadata().bindings.RuntimeStateDword() + RuntimeAbi::StateVertexSelectors + attribute / RuntimeAbi::VertexSelectorsPerDword), ConstantU32(state, attribute % RuntimeAbi::VertexSelectorsPerDword * 16u + component * 3u)), 7u) : state.module.SpecializationConstant(TypeU32(state), first + component, component + 4u);
        const auto one = state.module.SpecializationConstant(TypeU32(state), first + 4u, 0x3f800000u);
        auto value = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, 1u)), one, ConstantU32(state, 0u));
        for (std::uint32_t channel = 0; channel < 4u; ++channel) value = Select(state, TypeU32(state), Binary(state, spv::OpIEqual, TypeBool(state), selector, ConstantU32(state, channel + 4u)), EmitAttributeValue(state, attribute, channel), value);
        return value;
    }
    return EmitAttributeValue(ctx.state, inst.Argument(0)->ImmediateU32(), inst.Argument(1)->ImmediateU32());
}

std::uint32_t EmitGetInterpolationParameter(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitInterpolationParameterValue(ctx.state, inst.Argument(0)->ImmediateU32(), inst.Argument(1)->ImmediateU32(), inst.Argument(2)->ImmediateU32());
}

std::uint32_t EmitGetInterpolationParameterF16(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitInterpolationParameterF16Value(ctx.state, inst.Argument(0)->ImmediateU32(), inst.Argument(1)->ImmediateU32(), inst.Argument(2)->ImmediateU32(), inst.Argument(3)->ImmediateU32() != 0u);
}

void EmitSetAttribute(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto& exp = ctx.Export(inst);
    const auto exec = ctx.Arg(inst, 1);
    if (state.program.Resources().stage == IrShaderStage::Pixel && exp.vm && state.requirements.pixelValidMask && state.pixelValidMaskVariable != 0u) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelect, TypeU32(state), value, exec, ConstantU32(state, 1u), ConstantU32(state, 0u));
        state.module.AddFunction(spv::OpStore, state.pixelValidMaskVariable, value);
    }
    if (exp.kind == ExportTargetKind::Null || exp.en == 0u) {
        return;
    }
    EmitIfCondition(state, exec, [&]() {
        const auto data = ctx.Arg(inst, 0);
        if (exp.kind == ExportTargetKind::Primitive) {
            if (state.program.Resources().stage == IrShaderStage::Mesh) {
                state.module.AddFunction(spv::OpStore, MeshPrimitivePointer(state), ExportRawComponent(ctx, data, 0));
            }
            return;
        }
        if (exp.kind == ExportTargetKind::Position && exp.index != 0u) {
            EmitAuxPositionExport(ctx, data, exp);
            return;
        }
        if (exp.kind == ExportTargetKind::MrtZ) {
            if ((exp.en & 1u) != 0u && state.depthVariable != 0u) {
                const auto raw = ExportRawComponent(ctx, data, 0);
                const auto f32 = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitcast, TypeF32(state), f32, raw);
                state.module.AddFunction(spv::OpStore, state.depthVariable, f32);
            }
            if ((exp.en & 2u) != 0u && state.stencilRefVariable != 0u) {
                auto raw = ExportRawComponent(ctx, data, 1);
                if (state.inputInfo.pixel != nullptr && state.inputInfo.pixel->psStencilOperationValueExport) raw = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, raw, ConstantU32(state, 8u)), ConstantU32(state, 0xffu));
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitcast, TypeI32(state), value, raw);
                state.module.AddFunction(spv::OpStore, state.stencilRefVariable, value);
            }
            if ((exp.en & 4u) != 0u && state.sampleMaskVariable != 0u) {
                const auto raw = ExportRawComponent(ctx, data, 2);
                const auto value = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitcast, TypeI32(state), value, raw);
                StoreCoverage(state, value);
            }
            return;
        }
        const auto variable = state.program.Resources().stage == IrShaderStage::Mesh ? 0u : OutputVariableForExport(state, exp);
        if (state.program.Resources().stage != IrShaderStage::Mesh && variable == 0u) {
            return;
        }
        const bool uintOutput = MrtOutputMode(state, exp) == 7u;
        const auto vectorType = uintOutput ? TypeU32Vector(state, 4u) : TypeF32Vector(state, 4u);
        auto value = ExportVector(ctx, data, exp, uintOutput);
        if (state.program.Resources().stage == IrShaderStage::Pixel && exp.kind == ExportTargetKind::Mrt) {
            if (exp.index >= 8u) throw std::runtime_error("fragment export target exceeds the runtime mapping table");
            std::array<std::uint32_t, 4> components{};
            for (std::uint32_t component = 0; component < components.size(); ++component) {
                const auto index = state.registrationModules ? EmitAndConstant(state, Binary(state, spv::OpShiftRightLogical, TypeU32(state), EmitShaderDataDwordLoad(state, state.program.Metadata().bindings.RuntimeStateDword() + RuntimeAbi::StateExportMappings + exp.index / 4u), ConstantU32(state, exp.index % 4u * 8u + component * 2u)), 3u) : state.module.SpecializationConstant(TypeU32(state), PipelineSpecialization::ExportBase + exp.index * 4u + component, component);
                components[component] = state.module.AllocateId();
                state.module.AddFunction(spv::OpVectorExtractDynamic, uintOutput ? TypeU32(state) : TypeF32(state), components[component], value, index);
            }
            const auto mapped = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeConstruct, vectorType, mapped, components[0], components[1], components[2], components[3]);
            value = mapped;
            if (exp.index == 0u && !uintOutput && AlphaToCoverage(state)) StoreAlphaCoverage(state, value);
            const auto mode = MrtOutputMode(state, exp);
            if (PixelInfo(state).mergeMask != 0u && exp.index == PixelInfo(state).mergeTarget) {
                if (uintOutput || mode == 5u || mode == 6u) FailEmit("a partial write of a packed color target cannot take an integer or 16-bit normalized export");
                MergeColorTarget(state, value);
            }
            if (!uintOutput && mode != 5u && mode != 6u) value = SelectExportPacking(state, exp.index, value);
        }
        if (exp.kind == ExportTargetKind::Position) {
            if (state.inputInfo.vertex == nullptr) {
                throw std::runtime_error("vertex input info is missing for a position export");
            }
            if (state.inputInfo.vertex->clipSpace.enabled) {
                value = ConvertPositionToClipSpace(state, value);
            }
            if (state.depthRangeRemap && state.program.Resources().stage == IrShaderStage::Vertex) {
                value = RemapPositionDepth(state, value);
            }
        }
        if (exp.kind == ExportTargetKind::Position && state.zeroPositionClipIndex) {
            StoreZeroPositionClipDistance(state, value);
        }
        if (state.program.Resources().stage == IrShaderStage::Mesh) {
            const auto kind = exp.kind == ExportTargetKind::Position ? StageOutputKind::Position : StageOutputKind::Parameter;
            state.module.AddFunction(spv::OpStore, MeshOutputPointer(state, kind, exp.index), value);
        } else if (exp.kind == ExportTargetKind::Position) {
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeF32Vector(state, 4u)), pointer, variable, ConstantU32(state, 0u));
            state.module.AddFunction(spv::OpStore, pointer, value);
        } else {
            state.module.AddFunction(spv::OpStore, variable, value);
        }
    });
}

std::uint32_t EmitGetShaderBase(SpirvValueEmitContext& ctx) {
    return ConstantU64(ctx.state, 0u);
}

namespace {

std::uint32_t EmitReadClock(SpirvValueEmitContext& ctx, const IrValue& inst, spv::Scope scope) {
    auto& state = ctx.state;
    const bool capability = std::find(state.supportedCapabilities.begin(), state.supportedCapabilities.end(), static_cast<std::uint32_t>(spv::CapabilityShaderClockKHR)) != state.supportedCapabilities.end();
    const bool extension = std::find(state.supportedExtensions.begin(), state.supportedExtensions.end(), "SPV_KHR_shader_clock") != state.supportedExtensions.end();
    if (!capability || !extension) ctx.Fail(inst, "reads the shader clock, which needs the device's VK_KHR_shader_clock");
    state.module.EmitCapability(spv::CapabilityShaderClockKHR);
    state.module.EmitExtension("SPV_KHR_shader_clock");
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpReadClockKHR, TypeU64(state), result, ConstantU32(state, static_cast<std::uint32_t>(scope)));
    return result;
}

}

std::uint32_t EmitShaderClock(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitReadClock(ctx, inst, ctx.state.narrowSubgroupClock ? spv::ScopeDevice : spv::ScopeSubgroup);
}

std::uint32_t EmitRealtimeClock(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitReadClock(ctx, inst, spv::ScopeDevice);
}

void EmitTessellationBase(SpirvValueEmitContext& ctx, const IrValue& inst) {
    ctx.Fail(inst, "must be lowered before SPIR-V emission");
}

}
