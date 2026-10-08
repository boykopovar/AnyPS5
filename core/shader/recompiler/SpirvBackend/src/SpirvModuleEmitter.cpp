#include "SpirvBackend/SpirvModuleEmitter.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvFlowEmitter.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
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

std::uint32_t EmitBuiltinU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component) {
    if (kind == StageInputKind::LocalInvocationIndex) {
        return EmitLocalInvocationIndex(state);
    }
    if (kind == StageInputKind::DispatchThreadLimit) {
        return EmitShaderDataDwordLoad(state, state.program.Metadata().bindings.DispatchThreadLimitDword() + component);
    }
    if (state.laneCount == 2 && (kind == StageInputKind::LocalInvocationId || kind == StageInputKind::GlobalInvocationId)) {
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
        const auto group = EmitInputComponentU32(state, StageInputKind::WorkgroupId, component);
        return EmitBinaryU32(state, spv::OpIAdd, local, EmitBinaryU32(state, spv::OpIMul, group, ConstantU32(state, size)));
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
    if (kind == StageInputKind::VertexIndex || kind == StageInputKind::InstanceIndex || kind == StageInputKind::InvocationId || kind == StageInputKind::PrimitiveId || kind == StageInputKind::Layer || kind == StageInputKind::SampleId) {
        const auto value = state.module.AllocateId();
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeI32(state), value, variable);
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
        for (std::uint32_t vertex = 0; vertex < 3u; vertex++) {
            const auto pointer = state.module.AllocateId();
            const auto weight = state.module.AllocateId();
            const auto product = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeF32(state)), pointer, barycentric, ConstantU32(state, vertex));
            state.module.AddFunction(spv::OpLoad, TypeF32(state), weight, pointer);
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
    if (PixelParameterIsDefault(state, attr)) {
        return mode == 2u ? EmitAttributeValue(state, attr, chan) : ConstantU32(state, 0u);
    }
    const auto* input = SpirvInputBindingForParameter(state, attr);
    if (input == nullptr) {
        throw std::runtime_error("interpolation parameter refers to an undefined attribute");
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

std::uint32_t MrtOutputMode(const SpirvEmitterState& state, const ExportInfo& exp) {
    if (state.program.Resources().stage != IrShaderStage::Pixel || exp.kind != ExportTargetKind::Mrt) {
        return 0u;
    }
    if (state.inputInfo.pixel == nullptr || exp.index >= std::size(state.inputInfo.pixel->targetOutputMode)) {
        return 0u;
    }
    return state.inputInfo.pixel->targetOutputMode[exp.index];
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
    const auto exported = exp.en & (mode == 1u ? 0x1u : mode == 2u ? 0x3u : mode == 3u ? 0x9u : 0xfu);
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
                raw.at(component) = ExportRawComponent(ctx, data, component);
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
            state.module.AddFunction(spv::OpStore, state.pointSizeVariable, f32);
            continue;
        }
        const auto storeDistance = [&](std::uint32_t variable, std::uint32_t index) {
            if (index == std::numeric_limits<std::uint32_t>::max()) {
                return;
            }
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeF32(state)), pointer, variable, ConstantU32(state, index));
            state.module.AddFunction(spv::OpStore, pointer, f32);
        };
        storeDistance(state.clipDistanceVariable, output.clipDistance);
        storeDistance(state.cullDistanceVariable, output.cullDistance);
    }
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

std::uint32_t MeshOutputType(SpirvEmitterState& state, StageOutputKind kind) {
    return kind == StageOutputKind::Layer ? TypeU32(state) : TypeF32Vector(state, 4u);
}

}

namespace {

std::uint32_t TessellationPointer(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto kind = static_cast<TessellationAttribute>(inst.Argument(0)->ImmediateU32());
    const auto& tess = state.inputInfo.vertex->tess;
    const auto variable = state.tessVariables.at(static_cast<std::uint32_t>(kind));
    if (variable == 0u) {
        throw std::runtime_error("tessellation attribute has no interface variable");
    }
    const bool input = kind == TessellationAttribute::ControlInput || kind == TessellationAttribute::EvaluationInput;
    const auto storage = input ? spv::StorageClassInput : spv::StorageClassOutput;
    const auto pointer = state.module.AllocateId();
    if (kind == TessellationAttribute::Factor) {
        if (!inst.Argument(1)->HasImmediate()) {
            throw std::runtime_error("tessellation factor index must be immediate");
        }
        const auto index = inst.Argument(1)->ImmediateU32() / 4u;
        const bool outer = index < 3u;
        if (index >= 4u) {
            throw std::runtime_error("tessellation factor index out of range");
        }
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, storage, TypeF32(state)), pointer, outer ? variable : state.tessInnerVariable, ConstantU32(state, outer ? index : index - 3u));
        return pointer;
    }
    auto address = ctx.Arg(inst, 1);
    if (kind == TessellationAttribute::PatchOutput) {
        address = EmitBinaryU32(state, spv::OpISub, address, ConstantU32(state, state.tessPatchBase));
    }
    const bool local = kind == TessellationAttribute::LocalOutput || kind == TessellationAttribute::ControlInput;
    const auto stride = local ? tess.lsStride : tess.hsStride;
    const auto offset = kind == TessellationAttribute::PatchOutput ? address : EmitBinaryU32(state, spv::OpUMod, address, ConstantU32(state, stride));
    const auto attribute = EmitBinaryU32(state, spv::OpShiftRightLogical, offset, ConstantU32(state, 4u));
    const auto component = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, offset, ConstantU32(state, 2u)), ConstantU32(state, 3u));
    const auto type = TypePointer(state, storage, TypeU32(state));
    if (kind == TessellationAttribute::LocalOutput || kind == TessellationAttribute::PatchOutput) {
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, attribute, component);
    } else {
        const auto vertex = kind == TessellationAttribute::ControlOutput ? EmitLaneId(state) : EmitBinaryU32(state, spv::OpUDiv, address, ConstantU32(state, stride));
        state.module.AddFunction(spv::OpAccessChain, type, pointer, variable, vertex, attribute, component);
    }
    return pointer;
}

}

void DefineTessellationInterfaces(SpirvEmitterState& state) {
    std::array<bool, 6> used {};
    std::uint32_t patchBegin = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t patchEnd = 0u;
    for (const IrBlock* block : state.program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::GetTessellationAttribute && inst->Opcode() != IrOpcode::SetTessellationAttribute) {
                continue;
            }
            const auto kind = inst->Argument(0)->ImmediateU32();
            used.at(kind) = true;
            if (kind == static_cast<std::uint32_t>(TessellationAttribute::PatchOutput)) {
                if (!inst->Argument(1)->HasImmediate()) {
                    throw std::runtime_error("tessellation patch output offset must be immediate");
                }
                patchBegin = std::min(patchBegin, inst->Argument(1)->ImmediateU32());
                patchEnd = std::max(patchEnd, inst->Argument(1)->ImmediateU32() + 4u);
            }
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
        const bool input = kind == TessellationAttribute::ControlInput || kind == TessellationAttribute::EvaluationInput;
        const auto storage = input ? spv::StorageClassInput : spv::StorageClassOutput;
        std::uint32_t type;
        if (kind == TessellationAttribute::Factor) {
            type = array(TypeF32(state), 4u);
        } else if (kind == TessellationAttribute::PatchOutput) {
            state.tessPatchBase = patchBegin;
            type = array(TypeU32Vector(state, 4u), (patchEnd - patchBegin + 15u) / 16u);
        } else {
            const bool local = kind == TessellationAttribute::LocalOutput || kind == TessellationAttribute::ControlInput;
            const auto stride = local ? tess.lsStride : tess.hsStride;
            type = array(TypeU32Vector(state, 4u), (stride + 15u) / 16u);
            if (kind != TessellationAttribute::LocalOutput) {
                type = array(type, local ? tess.inputControlPoints : tess.outputControlPoints);
            }
        }
        auto& variable = state.tessVariables.at(index);
        variable = DefineInterfaceVariable(state, type, storage, "tess_attributes");
        if (kind == TessellationAttribute::Factor) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, spv::BuiltInTessLevelOuter);
            state.tessInnerVariable = DefineInterfaceVariable(state, array(TypeF32(state), 2u), storage, "tess_inner");
            state.module.AddAnnotation(spv::OpDecorate, state.tessInnerVariable, spv::DecorationBuiltIn, spv::BuiltInTessLevelInner);
            state.module.AddAnnotation(spv::OpDecorate, state.tessInnerVariable, spv::DecorationPatch);
        } else {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationLocation, kind == TessellationAttribute::PatchOutput ? (tess.hsStride + 15u) / 16u : 0u);
        }
        if (kind == TessellationAttribute::Factor || kind == TessellationAttribute::PatchOutput) {
            state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationPatch);
        }
    }
}

void DefineTessellationExecutionModes(SpirvEmitterState& state) {
    const auto& tess = state.inputInfo.vertex->tess;
    if (tess.domain != 1u || tess.partitioning != 2u || tess.outputTopology != 2u) {
        throw std::runtime_error("unsupported tessellation domain, partitioning, or output topology");
    }
    state.module.EmitCapability(spv::CapabilityTessellation);
    if (state.program.Resources().stage == IrShaderStage::TessellationControl) {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeOutputVertices, tess.outputControlPoints);
    } else {
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeTriangles);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeSpacingFractionalOdd);
        state.module.AddExecutionMode(state.mainFunc, spv::ExecutionModeVertexOrderCw);
    }
}

void DefineMeshOutputs(SpirvEmitterState& state) {
    if (state.inputInfo.vertex == nullptr) {
        throw std::runtime_error("vertex input info is missing for mesh output definition");
    }
    const auto& mesh = state.inputInfo.vertex->mesh;
    for (auto& output : state.outputs) {
        if (output.kind != StageOutputKind::Position && output.kind != StageOutputKind::Parameter && output.kind != StageOutputKind::Layer) {
            throw std::runtime_error("unsupported mesh output kind");
        }
        const auto type = MeshOutputType(state, output.kind);
        output.variableId = MeshArray(state, spv::StorageClassOutput, type, output.kind == StageOutputKind::Layer ? mesh.maxPrimitives : mesh.maxVertices);
        const bool shared = output.kind == StageOutputKind::Layer;
        output.meshDataVariable = MeshArray(state, shared ? spv::StorageClassWorkgroup : spv::StorageClassPrivate, type, shared ? mesh.maxVertices : state.laneCount);
        state.interfaceVariables.push_back(output.variableId);
        state.module.AddName(output.variableId, output.debugName);
        if (output.kind == StageOutputKind::Parameter) {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationLocation, output.location);
        } else {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationBuiltIn, output.kind == StageOutputKind::Layer ? spv::BuiltInLayer : spv::BuiltInPosition);
        }
        if (output.kind == StageOutputKind::Layer) {
            state.module.AddAnnotation(spv::OpDecorate, output.variableId, spv::DecorationPerPrimitiveEXT);
        }
    }
    state.meshAllocation = MeshArray(state, spv::StorageClassWorkgroup, TypeU32(state), 2u);
    state.meshPrimitiveData = MeshArray(state, spv::StorageClassPrivate, TypeU32(state), state.laneCount);
    state.meshPrimitives = MeshArray(state, spv::StorageClassOutput, TypeU32Vector(state, 3u), mesh.maxPrimitives);
    state.meshCull = MeshArray(state, spv::StorageClassOutput, TypeBool(state), mesh.maxPrimitives);
    state.interfaceVariables.push_back(state.meshPrimitives);
    state.interfaceVariables.push_back(state.meshCull);
    state.module.AddAnnotation(spv::OpDecorate, state.meshPrimitives, spv::DecorationBuiltIn, spv::BuiltInPrimitiveTriangleIndicesEXT);
    state.module.AddAnnotation(spv::OpDecorate, state.meshCull, spv::DecorationBuiltIn, spv::BuiltInCullPrimitiveEXT);
    state.module.AddAnnotation(spv::OpDecorate, state.meshCull, spv::DecorationPerPrimitiveEXT);
}

void EmitMeshEntryPoint(SpirvEmitterState& state) {
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), state.mainFunc, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshPrimitiveData, spv::StorageClassPrivate, TypeU32(state), ConstantU32(state, half)), ConstantU32(state, 0x80000000u));
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
    const auto vertices = clamp(MeshLoad(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, 0u)), mesh.maxVertices);
    const auto primitives = clamp(MeshLoad(state, state.meshAllocation, spv::StorageClassWorkgroup, TypeU32(state), ConstantU32(state, 1u)), mesh.maxPrimitives);
    state.module.AddFunction(spv::OpSetMeshOutputsEXT, vertices, primitives);
    const auto anyVertex = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeU32(state), anyVertex, GlslStd450(state), GLSLstd450UMax, vertices, ConstantU32(state, 1u));
    const auto lastVertex = EmitBinaryU32(state, spv::OpISub, anyVertex, ConstantU32(state, 1u));
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        state.laneHalf = half;
        const auto index = EmitLocalInvocationIndex(state);
        const auto isVertex = state.module.AllocateId();
        state.module.AddFunction(spv::OpULessThan, TypeBool(state), isVertex, index, vertices);
        EmitIfCondition(state, isVertex, [&]() {
            for (const auto& output : state.outputs) {
                if (output.kind == StageOutputKind::Layer) {
                    continue;
                }
                const auto type = MeshOutputType(state, output.kind);
                const auto value = MeshLoad(state, output.meshDataVariable, spv::StorageClassPrivate, type, ConstantU32(state, half));
                const auto pointer = MeshElement(state, output.variableId, spv::StorageClassOutput, type, index);
                state.module.AddFunction(spv::OpStore, pointer, value);
            }
        });
        const auto isPrimitive = state.module.AllocateId();
        state.module.AddFunction(spv::OpULessThan, TypeBool(state), isPrimitive, index, primitives);
        EmitIfCondition(state, isPrimitive, [&]() {
            const auto packed = MeshLoad(state, state.meshPrimitiveData, spv::StorageClassPrivate, TypeU32(state), ConstantU32(state, half));
            std::array<std::uint32_t, 3> vertex {};
            for (std::uint32_t component = 0; component < 3u; component++) {
                const auto field = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitFieldUExtract, TypeU32(state), field, packed, ConstantU32(state, component * 10u), ConstantU32(state, 9u));
                vertex.at(component) = state.module.AllocateId();
                state.module.AddFunction(spv::OpExtInst, TypeU32(state), vertex.at(component), GlslStd450(state), GLSLstd450UMin, field, lastVertex);
            }
            const auto triangle = state.module.AllocateId();
            state.module.AddFunction(spv::OpCompositeConstruct, TypeU32Vector(state, 3u), triangle, vertex[0], vertex[1], vertex[2]);
            const auto trianglePointer = MeshElement(state, state.meshPrimitives, spv::StorageClassOutput, TypeU32Vector(state, 3u), index);
            state.module.AddFunction(spv::OpStore, trianglePointer, triangle);
            const auto nullBit = EmitBinaryU32(state, spv::OpBitwiseAnd, packed, ConstantU32(state, 0x80000000u));
            const auto culled = state.module.AllocateId();
            state.module.AddFunction(spv::OpINotEqual, TypeBool(state), culled, nullBit, ConstantU32(state, 0u));
            state.module.AddFunction(spv::OpStore, MeshElement(state, state.meshCull, spv::StorageClassOutput, TypeBool(state), index), culled);
            for (const auto& output : state.outputs) {
                if (output.kind != StageOutputKind::Layer) {
                    continue;
                }
                const auto layer = MeshLoad(state, output.meshDataVariable, spv::StorageClassWorkgroup, TypeU32(state), vertex.at(state.inputInfo.vertex->mesh.provokingVertex));
                const auto pointer = MeshElement(state, output.variableId, spv::StorageClassOutput, TypeU32(state), index);
                state.module.AddFunction(spv::OpStore, pointer, layer);
            }
        });
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
    const auto exponentBits = EmitAndConstant(state, absBits, 0x7f800000u);
    const auto mantissaBits = EmitAndConstant(state, absBits, 0x007fffffu);
    const auto exponentMax = EmitCompareU32Constant(state, spv::OpIEqual, exponentBits, 0x7f800000u);
    const auto mantissaNonzero = EmitCompareU32Constant(state, spv::OpINotEqual, mantissaBits, 0u);
    cls.nan = Binary(state, spv::OpLogicalAnd, TypeBool(state), exponentMax, mantissaNonzero);
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

    const auto lhsNanBits = Select(state, TypeU32(state), lhsClass.nan, rhsClass.bits, numericBits);
    const auto resultBits = Select(state, TypeU32(state), rhsClass.nan, lhsClass.bits, lhsNanBits);
    return Unary(state, spv::OpBitcast, TypeF32(state), resultBits);
}

std::uint32_t EmitFlushF32DenormToSignedZero(SpirvEmitterState& state, std::uint32_t value) {
    const auto bits = state.module.AllocateId();
    const auto absBits = state.module.AllocateId();
    const auto signBits = state.module.AllocateId();
    const auto nonZero = state.module.AllocateId();
    const auto subnormal = state.module.AllocateId();
    const auto flush = state.module.AllocateId();
    const auto selected = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, value);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), absBits, bits, ConstantU32(state, 0x7fffffffu));
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), signBits, bits, ConstantU32(state, 0x80000000u));
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), nonZero, absBits, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpULessThan, TypeBool(state), subnormal, absBits, ConstantU32(state, 0x00800000u));
    state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), flush, nonZero, subnormal);
    state.module.AddFunction(spv::OpSelect, TypeU32(state), selected, flush, signBits, bits);
    state.module.AddFunction(spv::OpBitcast, TypeF32(state), result, selected);
    return result;
}

std::uint32_t EmitTrigCycleF32(SpirvEmitterState& state, std::uint32_t src, bool preserveSignedZero) {
    const auto fract = state.module.AllocateId();
    const auto bits = state.module.AllocateId();
    const auto absBits = state.module.AllocateId();
    const auto large = state.module.AllocateId();
    const auto finite = state.module.AllocateId();
    const auto largeFinite = state.module.AllocateId();
    const auto reduced = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32(state), fract, GlslStd450(state), GLSLstd450Fract, src);
    state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, src);
    state.module.AddFunction(spv::OpBitwiseAnd, TypeU32(state), absBits, bits, ConstantU32(state, 0x7fffffffu));
    state.module.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), large, absBits, ConstantU32(state, 0x4b000000u));
    state.module.AddFunction(spv::OpULessThan, TypeBool(state), finite, absBits, ConstantU32(state, 0x7f800000u));
    state.module.AddFunction(spv::OpLogicalAnd, TypeBool(state), largeFinite, large, finite);
    state.module.AddFunction(spv::OpSelect, TypeF32(state), reduced, largeFinite, ConstantF32Value(state, 0.0f), fract);
    if (!preserveSignedZero) {
        return reduced;
    }
    const auto zero = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpIEqual, TypeBool(state), zero, absBits, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelect, TypeF32(state), result, zero, src, reduced);
    return result;
}

std::uint32_t EmitF16BitsToF32(SpirvEmitterState& state, std::uint32_t bits) {
    const auto unpacked = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpExtInst, TypeF32Vector(state, 2), unpacked, GlslStd450(state), GLSLstd450UnpackHalf2x16, bits);
    state.module.AddFunction(spv::OpCompositeExtract, TypeF32(state), result, unpacked, 0u);
    return result;
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
    for (const IrBlock* block : program.BlockOrder()) {
        const auto label = state.module.AllocateId();
        state.labels.emplace(block, label);
    }
    DefineGetBdaPointer(state);
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
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), state.meshGuestFunc != 0u ? state.meshGuestFunc : state.mainFunc, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.entryLabel);
    if (state.requirements.functionLds) {
        state.module.AddFunction(spv::OpVariable, TypeU32ArrayPointer(state, spv::StorageClassFunction, LdsDwordCount(state)), state.ldsVariable, spv::StorageClassFunction);
    }
    if (state.requirements.functionScratch) {
        for (std::uint32_t half = 0; half < state.laneCount; half++) {
            state.module.AddFunction(spv::OpVariable, TypeU32ArrayPointer(state, spv::StorageClassFunction, state.program.Info().scratchDwords), state.scratchVariable.at(half), spv::StorageClassFunction);
        }
    }
    if (state.pixelValidMaskVariable != 0u) {
        state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), state.pixelValidMaskVariable, spv::StorageClassFunction);
    }
    for (std::uint32_t half = 0; half < state.laneCount; half++) {
        auto& lane = half == 0u ? ctx : high;
        if (lane.scratchU32Variable != 0u) {
            state.module.AddFunction(spv::OpVariable, TypePointer(state, spv::StorageClassFunction, TypeU32(state)), lane.scratchU32Variable, spv::StorageClassFunction);
        }
    }
    if (state.requirements.ldsLock) {
        state.module.AddFunction(spv::OpStore, EmitLdsLockPointer(state), ConstantU32(state, 0u));
        state.module.AddFunction(spv::OpControlBarrier, ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::ScopeWorkgroup), ConstantU32(state, spv::MemorySemanticsAcquireReleaseMask | spv::MemorySemanticsWorkgroupMemoryMask));
    }
    if (state.gdsVariable != 0u) {
        state.gdsLength = state.module.AllocateId();
        state.module.AddFunction(spv::OpArrayLength, TypeU32(state), state.gdsLength, state.gdsVariable, 0u);
    }
    if (state.pixelValidMaskVariable != 0u) {
        state.module.AddFunction(spv::OpStore, state.pixelValidMaskVariable, ConstantU32(state, 1u));
    }
    if (OrderedPixelShader(state)) {
        state.module.AddFunction(spv::OpBeginInvocationInterlockEXT);
    }
    EmitMemoryOffsets(state);
    if (state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->quadPixelMask != 0xfu) {
        const auto coordinate = [&](std::uint32_t component) {
            const auto value = Unary(state, spv::OpBitcast, TypeF32(state), EmitBuiltinU32(state, StageInputKind::FragCoord, component));
            return EmitBinaryU32(state, spv::OpBitwiseAnd, Unary(state, spv::OpConvertFToU, TypeU32(state), value), ConstantU32(state, 1u));
        };
        const auto quad = EmitBinaryU32(state, spv::OpBitwiseOr, coordinate(0u), EmitBinaryU32(state, spv::OpShiftLeftLogical, coordinate(1u), ConstantU32(state, 1u)));
        const auto bit = EmitBinaryU32(state, spv::OpBitwiseAnd, EmitBinaryU32(state, spv::OpShiftRightLogical, ConstantU32(state, state.inputInfo.pixel->quadPixelMask), quad), ConstantU32(state, 1u));
        const auto mask = static_cast<std::uint32_t>(state.inputInfo.pixel->quadPixelMask);
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
    }
    if (program.BlockOrder().empty()) {
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

std::uint32_t EmitMeshArgument(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto index = inst.Argument(0)->ImmediateU32();
    if (state.program.Resources().stage != IrShaderStage::Mesh || index >= MeshArgumentBytes / 4u) {
        ctx.Fail(inst, "invalid mesh argument");
    }
    const auto push = [&](std::uint32_t dword) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePushConstantElementPointer(state), pointer, state.pushConstantVariable, ConstantU32(state, 0u), ConstantU32(state, MeshDrawPushOffsetBytes / 4u + dword));
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return Unary(state, spv::OpUConvert, TypeScalarU64(state), value);
    };
    const auto low = push(MeshArgumentAddressDword);
    const auto high = push(MeshArgumentAddressDword + 1u);
    const auto address = Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), low, Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), high, ConstantU32(state, 32u)));
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

void EmitSetTessellationAttribute(SpirvValueEmitContext& ctx, const IrValue& inst) {
    EmitIfCondition(ctx.state, ctx.Arg(inst, 3), [&]() {
        auto value = ctx.Arg(inst, 2);
        if (inst.Argument(0)->ImmediateU32() == static_cast<std::uint32_t>(TessellationAttribute::Factor)) {
            const auto floating = ctx.state.module.AllocateId();
            ctx.state.module.AddFunction(spv::OpBitcast, TypeF32(ctx.state), floating, value);
            value = floating;
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
    return EmitAttributeValue(ctx.state, inst.Argument(0)->ImmediateU32(), inst.Argument(1)->ImmediateU32());
}

std::uint32_t EmitGetInterpolationParameter(SpirvValueEmitContext& ctx, const IrValue& inst) {
    return EmitInterpolationParameterValue(ctx.state, inst.Argument(0)->ImmediateU32(), inst.Argument(1)->ImmediateU32(), inst.Argument(2)->ImmediateU32());
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
            if ((exp.en & 4u) != 0u && state.sampleMaskVariable != 0u) {
                const auto raw = ExportRawComponent(ctx, data, 2);
                const auto value = state.module.AllocateId();
                const auto pointer = state.module.AllocateId();
                state.module.AddFunction(spv::OpBitcast, TypeI32(state), value, raw);
                state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassOutput, TypeI32(state)), pointer, state.sampleMaskVariable, ConstantU32(state, 0u));
                state.module.AddFunction(spv::OpStore, pointer, value);
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
        if (state.program.Resources().stage == IrShaderStage::Pixel && exp.kind == ExportTargetKind::Mrt && exp.index < state.inputInfo.pixel->targetExportMapping.size()) {
            const auto& mapping = state.inputInfo.pixel->targetExportMapping.at(exp.index);
            if (!mapping.IsIdentity()) {
                const auto mapped = state.module.AllocateId();
                state.module.AddFunction(spv::OpVectorShuffle, vectorType, mapped, value, value, mapping.Map(0), mapping.Map(1), mapping.Map(2), mapping.Map(3));
                value = mapped;
            }
        }
        if (exp.kind == ExportTargetKind::Position) {
            if (state.inputInfo.vertex == nullptr) {
                throw std::runtime_error("vertex input info is missing for a position export");
            }
            if (state.inputInfo.vertex->clipSpace.enabled) {
                value = ConvertPositionToClipSpace(state, value);
            }
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
