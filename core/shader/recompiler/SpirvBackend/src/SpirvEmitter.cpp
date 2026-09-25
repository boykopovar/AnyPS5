#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitterState.hpp"
#include "SpirvBackend/SpirvFlowEmitter.hpp"
#include "SpirvBackend/SpirvMemory/SpirvModuleSetup.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <SpirvBackend/SpirvEmitterInstructions.hpp>

namespace ShaderRecompiler {

namespace {

[[noreturn]] void FailProgram(const IrProgram& program, const char* reason) {
    throw std::runtime_error("SPIR-V emission failed: hash=0x" + std::to_string(program.Resources().shaderHash) + " stage=" + std::to_string(static_cast<unsigned>(program.Resources().stage)) + " reason=" + reason);
}

const ShaderWorkgroupInputInfo* ShaderWorkgroupInputFor(const SpirvEmitterState& state) {
    switch (state.program.Resources().stage) {
    case IrShaderStage::Compute:
        if (state.inputInfo.compute == nullptr) {
            FailProgram(state.program, "compute input info is missing");
        }
        return state.inputInfo.compute;
    case IrShaderStage::Mesh:
        if (state.inputInfo.vertex == nullptr) {
            FailProgram(state.program, "vertex input info is missing");
        }
        return &state.inputInfo.vertex->mesh;
    default:
        return nullptr;
    }
}

// Lanes of one guest wave run in lockstep, so a lane reads LDS another lane of its wave wrote without a
// barrier. Host invocations need one; it can be issued wherever the guest wave's control flow is
// uniform across the barrier's scope: the host subgroup when it holds exactly one guest wave, or the
// workgroup when the workgroup is a single wave.
std::uint32_t WaveLdsScope(const IrProgram& program, const ShaderWorkgroupInputInfo* workgroup, std::uint32_t laneCount) {
    if (program.Resources().stage != IrShaderStage::Compute || workgroup == nullptr) return 0;
    bool writes = false;
    for (const auto* block : program.BlockOrder()) {
        for (const auto* instruction : block->Instructions()) {
            const auto access = SharedAccessOf(instruction->Opcode());
            writes |= access != SharedAccess::None && access != SharedAccess::Read;
        }
    }
    if (!writes) return 0;
    const auto threads = std::max(workgroup->threadsNum[0], 1u) * std::max(workgroup->threadsNum[1], 1u) * std::max(workgroup->threadsNum[2], 1u);
    // A single-wave workgroup gets workgroup-scope barriers: on NVIDIA a subgroup-scope
    // OpControlBarrier did not make one lane's LDS writes visible to the others (Bink's decode
    // shaders read half their block as zeros), while the workgroup scope does. Debug aid:
    // APS5_WAVE_LDS_SUBGROUP=1 restores the subgroup scope for comparison.
    static const bool preferSubgroup = std::getenv("APS5_WAVE_LDS_SUBGROUP") != nullptr;
    if (!preferSubgroup && threads <= program.WaveSize()) return spv::ScopeWorkgroup;
    // A wave64 program kept at one lane per invocation (APS5_SINGLE_LANE reports a 64-wide host) spans
    // two real subgroups, so only the workgroup scope covers it; hosts wider than 32 lanes are not
    // distinguished from that case and get the same, still correct, scope.
    if (laneCount == 2u || (program.WaveSize() == workgroup->hostSubgroupSize && workgroup->hostSubgroupSize <= 32u)) return spv::ScopeSubgroup;
    return threads <= program.WaveSize() ? spv::ScopeWorkgroup : 0u;
}

}

SpirvEmitterState::SpirvEmitterState(const IrProgram& program, const ShaderStageInputInfo& inputInfo) : module(program.Resources().stage == IrShaderStage::Mesh ? 0x00010400u : 0x00010300u), program(program), inputInfo(inputInfo), requirements(AnalyzeProgramRequirements(program)) {
}

SpirvValueEmitContext::SpirvValueEmitContext(SpirvEmitterState& state) : state(state) {
}

std::uint32_t SpirvValueEmitContext::Def(const IrValue* value) {
    if (value == nullptr) {
        Fail("direct SPIR-V emitter received a null value");
    }
    IrValue* resolved = value->Resolve();
    if (resolved == nullptr) {
        Fail("direct SPIR-V emitter received a non-value argument");
    }
    if (resolved->HasImmediate()) {
        switch (resolved->Type()) {
        case IrType::Bool:
            return ConstantBool(state, resolved->ImmediateBool());
        case IrType::U8:
            return ConstantU32(state, resolved->ImmediateU8());
        case IrType::U16:
            return ConstantU32(state, resolved->ImmediateU16());
        case IrType::U32:
            return ConstantU32(state, resolved->ImmediateU32());
        case IrType::U64:
            return ConstantU64(state, resolved->ImmediateU64());
        case IrType::F16:
            return ConstantU32(state, resolved->ImmediateF16Bits());
        case IrType::F32:
            return ConstantF32(state, std::bit_cast<std::uint32_t>(resolved->ImmediateF32()));
        default:
            break;
        }
    }
    return Result(*resolved);
}

std::uint32_t SpirvValueEmitContext::Arg(const IrValue& inst, std::size_t index) {
    return Def(inst.Argument(index));
}

std::uint32_t SpirvValueEmitContext::HalfArg(const IrValue& inst, std::size_t index, std::uint32_t half) {
    return half == this->half ? Arg(inst, index) : otherHalf->Arg(inst, index);
}

std::uint32_t SpirvValueEmitContext::Ballot(const IrValue* predicate) {
    const auto ballotType = TypeU32Vector(state, 4u);
    const auto scope = ConstantU32(state, spv::ScopeSubgroup);
    const auto low = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformBallot, ballotType, low, scope, otherHalf == nullptr || half == 0u ? Def(predicate) : otherHalf->Def(predicate));
    if (otherHalf == nullptr) {
        return low;
    }
    const auto high = state.module.AllocateId();
    const auto lowWord = state.module.AllocateId();
    const auto highWord = state.module.AllocateId();
    const auto ballot = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformBallot, ballotType, high, scope, half == 1u ? Def(predicate) : otherHalf->Def(predicate));
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), lowWord, low, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), highWord, high, 0u);
    state.module.AddFunction(spv::OpCompositeConstruct, ballotType, ballot, lowWord, highWord, ConstantU32(state, 0u), ConstantU32(state, 0u));
    return ballot;
}

std::uint32_t SpirvValueEmitContext::FirstLane(std::uint32_t ballot) {
    if (otherHalf == nullptr) {
        const auto result = state.module.AllocateId();
        state.module.AddFunction(spv::OpGroupNonUniformBallotFindLSB, TypeU32(state), result, ConstantU32(state, spv::ScopeSubgroup), ballot);
        return result;
    }
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    const auto lowFirst = state.module.AllocateId();
    const auto highFirst = state.module.AllocateId();
    const auto lowActive = state.module.AllocateId();
    const auto result = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0u);
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1u);
    state.module.AddFunction(spv::OpExtInst, TypeU32(state), lowFirst, GlslStd450(state), GLSLstd450FindILsb, low);
    state.module.AddFunction(spv::OpExtInst, TypeU32(state), highFirst, GlslStd450(state), GLSLstd450FindILsb, high);
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), lowActive, low, ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelect, TypeU32(state), result, lowActive, lowFirst, EmitAddU32(state, highFirst, ConstantU32(state, 32u)));
    return result;
}

std::uint32_t SpirvValueEmitContext::Shuffle(const IrValue& inst, std::size_t index, std::uint32_t lane) {
    const auto type = TypeId(state, inst.Argument(index)->Type());
    const auto scope = ConstantU32(state, spv::ScopeSubgroup);
    const auto low = state.module.AllocateId();
    if (otherHalf == nullptr) {
        state.module.AddFunction(spv::OpGroupNonUniformShuffle, type, low, scope, Arg(inst, index), lane);
        return low;
    }
    const auto physicalLane = EmitBinaryU32(state, spv::OpBitwiseAnd, lane, ConstantU32(state, 31u));
    const auto high = state.module.AllocateId();
    const auto inHigh = state.module.AllocateId();
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, type, low, scope, HalfArg(inst, index, 0u), physicalLane);
    state.module.AddFunction(spv::OpGroupNonUniformShuffle, type, high, scope, HalfArg(inst, index, 1u), physicalLane);
    state.module.AddFunction(spv::OpINotEqual, TypeBool(state), inHigh, EmitBinaryU32(state, spv::OpBitwiseAnd, lane, ConstantU32(state, 32u)), ConstantU32(state, 0u));
    state.module.AddFunction(spv::OpSelect, type, value, inHigh, high, low);
    return value;
}

std::uint32_t SpirvValueEmitContext::Result(const IrValue& inst) {
    if (const auto found = definitions.find(&inst); found != definitions.end()) {
        return found->second;
    }
    const auto id = state.module.AllocateId();
    definitions.emplace(&inst, id);
    return id;
}

std::uint32_t SpirvValueEmitContext::Define(const IrValue& inst, std::uint32_t value) {
    if (const auto found = definitions.find(&inst); found != definitions.end()) {
        if (found->second != value) {
            state.module.AddFunction(spv::OpCopyObject, TypeId(state, inst.Type()), found->second, value);
        }
        return found->second;
    }
    definitions.emplace(&inst, value);
    return value;
}

std::uint32_t SpirvValueEmitContext::ResourceIndex(const IrValue* value, IrOpcode opcode) {
    const IrValue* resolved = value != nullptr ? value->Resolve() : nullptr;
    if (resolved == nullptr || resolved->Opcode() != opcode) {
        Fail("typed resource handle has the wrong producer");
    }
    return resolved->Flags<std::uint32_t>();
}

const IrValue* SpirvValueEmitContext::ImageAddress(const IrValue* value) {
    const IrValue* resolved = value != nullptr ? value->Resolve() : nullptr;
    if (resolved == nullptr || resolved->Opcode() != IrOpcode::MakeImageAddress) {
        Fail("typed image address was not constructed by MakeImageAddress");
    }
    return resolved;
}

const MemoryInfo& SpirvValueEmitContext::Memory(const IrValue& inst) const {
    return state.program.Resources().memoryInfo.at(inst.Flags<MemoryFlags>().index);
}

const ExportInfo& SpirvValueEmitContext::Export(const IrValue& inst) const {
    return state.program.Metadata().exportInfo.at(inst.Flags<ExportFlags>().index);
}

std::uint32_t SpirvValueEmitContext::Label(const IrBlock* block) const {
    return state.labels.at(block);
}

[[noreturn]] void SpirvValueEmitContext::Fail(const char* reason) const {
    FailProgram(state.program, reason);
}

[[noreturn]] void SpirvValueEmitContext::Fail(const IrValue& inst, const char* reason) const {
    throw std::runtime_error("SPIR-V emission failed: hash=0x" + std::to_string(state.program.Resources().shaderHash) + " stage=" + std::to_string(static_cast<unsigned>(state.program.Resources().stage)) + " opcode=" + std::string(IrOpcodeName(inst.Opcode())) + " reason=" + reason);
}

std::vector<std::uint32_t> SpirvEmitter::Emit(const IrProgram& program, const BindingAllocationResult& bindings, const SpirvTargetOptions& target) const {
    return Emit(program, ShaderStageInputInfo {}, bindings, target);
}

std::vector<std::uint32_t> SpirvEmitter::Emit(const IrProgram& program, const ShaderStageInputInfo& inputInfo, const BindingAllocationResult& bindings, const SpirvTargetOptions& target) const {
    if (program.Resources().stage != IrShaderStage::Compute && program.Resources().stage != IrShaderStage::Vertex && program.Resources().stage != IrShaderStage::Pixel && program.Resources().stage != IrShaderStage::Mesh && program.Resources().stage != IrShaderStage::Local && program.Resources().stage != IrShaderStage::TessellationControl && program.Resources().stage != IrShaderStage::TessellationEvaluation) {
        FailProgram(program, "binary SPIR-V emitter received an unsupported shader stage");
    }
    if (!program.Resources().srtPlanComplete || !program.Resources().resourceTrackingComplete || !program.Metadata().shaderInfoComplete || !program.Metadata().bindingLayoutComplete) {
        FailProgram(program, "SPIR-V emitter requires a fully planned native shader program");
    }
    ValidateProgram(program, true);
    ValidateBdaTarget(program, target);
    SpirvEmitterState state(program, inputInfo);
    state.module.RequireVersion(target.spirvVersion);
    const auto* workgroup = ShaderWorkgroupInputFor(state);
    state.laneCount = workgroup != nullptr && program.WaveSize() == 64u && workgroup->hostSubgroupSize == 32u ? 2u : 1u;
    state.waveLdsScope = WaveLdsScope(program, workgroup, state.laneCount);
    if (const char* guard = std::getenv("APS5_LOOP_GUARD")) state.loopGuardLimit = static_cast<std::uint32_t>(std::strtoul(guard, nullptr, 0));
    // Stopped invocations would leave the wave LDS barriers incomplete.
    state.bdaStopsInvocations = state.waveLdsScope == 0 && BdaInvocationsMayStop(program);
    EmitModuleHeader(state, bindings);
    EmitProgram(state);
    state.module.EmitEntryPoint(ExecutionModelForStage(state.program.Resources().stage), state.mainFunc, "main", state.interfaceVariables);
    return state.module.Finalize();
}

}
