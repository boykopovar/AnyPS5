#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvSubgroup.hpp"
#include "SpirvBackend/SpirvMemory/SpirvBufferAccess.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <SpirvBackend/SpirvEmitterInstructions.hpp>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <array>
#include <span>
#include <algorithm>

namespace ShaderRecompiler
{
    namespace {

        constexpr std::uint32_t PsInputFlatShade = 0x00000400u;
        constexpr std::uint32_t PixelParameterLimit = 32u;

        bool IsVertexLikeStage(const SpirvEmitterState& state) {
            return StageOf(state) == IrShaderStage::Vertex || StageOf(state) == IrShaderStage::Local;
        }

        std::uint32_t PixelInputLocation(const ShaderPixelInputInfo& info, std::uint32_t input) {
            if (input >= PixelParameterLimit || input >= info.inputNum) {
                FailEmit("pixel interpolator index is out of range");
            }
            if (info.InputIsDefault(input)) {
                FailEmit("pixel input " + std::to_string(input) + " reads a default value, not a parameter");
            }
            return info.InputSlot(input);
        }

        bool PixelInputIsCustom(const ShaderPixelInputInfo& info, std::uint32_t input) {
            return info.InputIsCustom(input);
        }

        bool PixelInputIsFlat(const ShaderPixelInputInfo& info, std::uint32_t input) {
            return input < info.inputNum && input < PixelParameterLimit && (info.interpolatorSettings[input] & PsInputFlatShade) != 0u && !PixelInputIsCustom(info, input);
        }

    }

    std::vector<FragmentParameter> DescribeFragmentParameters(const IrProgram& program, const ShaderStageInputInfo& inputInfo) {
        if (inputInfo.pixel == nullptr) FailEmit("pixel input info is missing");
        const auto& pixel = *inputInfo.pixel;
        std::vector<FragmentParameter> result;
        for (const auto& input : program.Info().inputs) {
            if (input.kind != StageInputKind::Parameter) continue;
            const auto location = PixelInputLocation(pixel, input.location);
            if (std::any_of(result.begin(), result.end(), [&](const auto& parameter) { return parameter.location == location; })) continue;
            result.push_back({location, location, PixelInputIsFlat(pixel, input.location), input.perVertex, PixelInputIsCustom(pixel, input.location)});
        }
        return result;
    }

    std::uint32_t PixelParameterLocation(const SpirvEmitterState& state, std::uint32_t attr) {
        if (StageOf(state) != IrShaderStage::Pixel) {
            return attr;
        }
        return PixelInputLocation(PixelInfo(state), attr);
    }

    bool PixelParameterIsFlat(const SpirvEmitterState& state, std::uint32_t attr) {
        return StageOf(state) == IrShaderStage::Pixel && PixelInputIsFlat(PixelInfo(state), attr);
    }

    bool PixelParameterIsCustom(const SpirvEmitterState& state, std::uint32_t attr) {
        return StageOf(state) == IrShaderStage::Pixel && PixelInputIsCustom(PixelInfo(state), attr);
    }

    bool PixelParameterIsDefault(const SpirvEmitterState& state, std::uint32_t attr) {
        return StageOf(state) == IrShaderStage::Pixel && PixelInfo(state).InputIsDefault(attr);
    }

    bool PixelParameterIsLinear(const SpirvEmitterState& state, std::uint32_t attr) {
        const auto& metadata = state.program.Metadata();
        return StageOf(state) == IrShaderStage::Pixel && PixelInfo(state).InputIsLinear(attr, metadata.pixelLinearInputs, metadata.pixelPerspectiveInputs);
    }

    VertexInputScalarKind VertexParameterScalarKind(const SpirvEmitterState& state, std::uint32_t location) {
        if (!IsVertexLikeStage(state)) {
            return VertexInputScalarKind::Float;
        }
        const auto& vertex = VertexInfo(state);
        if (location >= static_cast<std::uint32_t>(ShaderVertexInputInfo::MaxResources) || location >= static_cast<std::uint32_t>(vertex.resourcesNum)) {
            return VertexInputScalarKind::Float;
        }
        const auto numeric = VertexInputNumericClass(vertex.resources[location].Format());
        if (numeric == IrTextureNumericClass::Uint) return VertexInputScalarKind::Uint;
        if (numeric == IrTextureNumericClass::Sint) return VertexInputScalarKind::Sint;
        return VertexInputScalarKind::Float;
    }

    std::uint32_t VertexParameterComponentCount(const SpirvInputBinding& input) {
        return std::clamp(input.componentCount, 1u, 4u);
    }

    std::uint32_t VertexParameterScalarType(SpirvEmitterState& state, VertexInputScalarKind kind) {
        switch (kind) {
        case VertexInputScalarKind::Sint: return TypeI32(state);
        case VertexInputScalarKind::Uint: return TypeU32(state);
        case VertexInputScalarKind::Float: return TypeF32(state);
        }
        FailEmit("vertex input scalar kind is invalid");
    }

    std::uint32_t OutputVariableForExport(const SpirvEmitterState& state, const ExportInfo& exp) {
        if (exp.kind == ExportTargetKind::Position && exp.index == 0) {
            return state.perVertexVariable;
        }
        if (exp.kind == ExportTargetKind::MrtZ) {
            return state.depthVariable;
        }
        const auto expectedKind = exp.kind == ExportTargetKind::Mrt ? StageOutputKind::Mrt : StageOutputKind::Parameter;
        for (const auto& binding : state.outputs) {
            if (binding.kind == expectedKind && binding.index == exp.index) {
                return binding.variableId;
            }
        }
        return 0;
    }

    std::uint32_t InputVariableForKind(const SpirvEmitterState& state, StageInputKind kind) {
        for (const auto& input : state.inputs) {
            if (input.kind == kind) {
                return input.variableId;
            }
        }
        return 0;
    }

    bool OrderedPixelShader(const SpirvEmitterState& state) {
        return state.program.Resources().stage == IrShaderStage::Pixel && state.inputInfo.pixel != nullptr && state.inputInfo.pixel->psOrderedPixelShader;
    }

    const SpirvInputBinding* SpirvInputBindingForParameter(const SpirvEmitterState& state, std::uint32_t location) {
        for (const auto& input : state.inputs) {
            if (input.kind == StageInputKind::Parameter && input.location == location) {
                return &input;
            }
        }
        return nullptr;
    }

    std::uint32_t EmitVertexParameterComponentU32(SpirvEmitterState& state, const SpirvInputBinding& input, std::uint32_t component) {
        const auto count = VertexParameterComponentCount(input);
        const auto kind = VertexParameterScalarKind(state, input.location);
        const auto scalarType = VertexParameterScalarType(state, kind);
        const auto raw = state.module.AllocateId();
        if (count == 1u) {
            state.module.AddFunction(spv::OpLoad, scalarType, raw, input.variableId);
        } else {
            const auto pointerType = TypePointer(state, spv::StorageClassInput, scalarType);
            const auto pointer = state.module.AllocateId();
            state.module.AddFunction(spv::OpAccessChain, pointerType, pointer, input.variableId, ConstantU32(state, component));
            state.module.AddFunction(spv::OpLoad, scalarType, raw, pointer);
        }
        if (kind == VertexInputScalarKind::Uint) {
            return raw;
        }
        const auto bits = state.module.AllocateId();
        state.module.AddFunction(spv::OpBitcast, TypeU32(state), bits, raw);
        return bits;
    }

    std::uint32_t EmitInputComponentU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component) {
        const auto variable = InputVariableForKind(state, kind);
        if (variable == 0) {
            FailEmit("stage input " + std::to_string(static_cast<std::uint32_t>(kind)) + " was not declared");
        }
        const auto pointer = state.module.AllocateId();
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpAccessChain, TypePointer(state, spv::StorageClassInput, TypeU32(state)), pointer, variable, ConstantU32(state, component));
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
        return value;
    }

    std::uint32_t EmitLocalInvocationIndex(SpirvEmitterState& state) {
        const auto variable = InputVariableForKind(state, StageInputKind::LocalInvocationIndex);
        if (variable == 0) {
            FailEmit("LocalInvocationIndex was not declared");
        }
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, variable);
        if (state.laneCount == 2) {
            const auto waveBase = EmitBinaryU32(state, spv::OpBitwiseAnd, value, ConstantU32(state, ~31u));
            return EmitAddU32(state, EmitAddU32(state, value, waveBase), ConstantU32(state, state.laneHalf * 32));
        }
        return value;
    }
}
