#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "SpirvBackend/SpirvOptimizer.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <initializer_list>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> recompile(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 12> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu, 0u, 0u, 0u, 0u, 7u, 9u, 0x10002000u, 0u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    require(!result.spirv.empty(), "the shader did not recompile");
    return result.spirv.Copy();
}

void requireDistinctTargets(const char* name, std::span<const std::uint32_t> code) {
    const auto words = recompile(code);
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        require(count != 0 && cursor + count <= words.size(), std::string(name) + ": truncated SPIR-V instruction");
        if ((words[cursor] & 0xffffu) == spv::OpBranchConditional) {
            require(words[cursor + 2] != words[cursor + 3], std::string(name) + ": OpBranchConditional with the same true and false label, invalid in SPIR-V 1.6");
        }
        cursor += count;
    }
}

#if ANYPS5_ENABLE_SPIRV_TOOLS
enum class EntryFact { None, Saturate, Alias, Product };

void requireCrossRegionNan(bool conjunct, bool certified = false, bool unproved = false, EntryFact entryFact = EntryFact::None) {
    IrProgram program;
    program.SetWaveSize(32);
    program.Resources().stage = IrShaderStage::Compute;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    program.Metadata().blockInfo.emplace_back();
    const auto constant = [&](std::uint32_t word) {
        auto& value = program.CreateValue(IrOpcode::Void, IrType::U32);
        value.SetImmediateU32(word);
        return &value;
    };
    const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& value = program.CreateValue(opcode, type);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return &value;
    };
    MemoryInfo lds;
    lds.kind = ResourceKind::Lds;
    program.Resources().memoryInfo.push_back(lds);
    auto* zero = constant(0u);
    auto* nan = constant(0x7fc12345u);
    const auto isNan = [&](IrValue* value) {
        auto* magnitude = emit(IrOpcode::BitwiseAnd32, IrType::U32, {value, constant(0x7fffffffu)});
        return emit(IrOpcode::UGreaterThan32, IrType::Bool, {magnitude, constant(0x7f800000u)});
    };
    const auto select = [&](IrValue* condition) { return emit(IrOpcode::SelectU32, IrType::U32, {condition, nan, zero}); };
    auto* entry = certified ? emit(IrOpcode::LaneId, IrType::U32, {}) : nan;
    IrValue* alias = nullptr;
    if (entryFact != EntryFact::None) {
        auto* value = emit(IrOpcode::BitCastF32U32, IrType::F32, {entry});
        if (entryFact == EntryFact::Saturate) value = emit(IrOpcode::FPSaturate32, IrType::F32, {value});
        if (entryFact == EntryFact::Product) value = emit(IrOpcode::FPMul32, IrType::F32, {value, emit(IrOpcode::BitCastF32U32, IrType::F32, {constant(0x7f7fffffu)})});
        alias = emit(IrOpcode::BitCastU32F32, IrType::U32, {value});
        if (entryFact != EntryFact::Alias) entry = alias;
        emit(IrOpcode::ControlNop, IrType::Void, {});
    }
    auto* first = select(isNan(entry));
    auto* second = select(isNan(entryFact == EntryFact::Alias ? alias : first));
    auto* condition = isNan(second);
    if (certified) {
        auto* magnitude = emit(IrOpcode::BitwiseAnd32, IrType::U32, {second, constant(0x7fffffffu)});
        auto* infinity = emit(IrOpcode::IEqual32, IrType::Bool, {magnitude, constant(unproved ? 0x3f800000u : 0x7f800000u)});
        condition = emit(IrOpcode::LogicalOr, IrType::Bool, {condition, infinity});
    } else emit(IrOpcode::ControlNop, IrType::Void, {});
    if (conjunct) condition = emit(IrOpcode::LogicalAnd, IrType::Bool, {condition, condition});
    auto* third = select(condition);
    auto* fourth = select(isNan(third));
    auto& active = program.CreateValue(IrOpcode::Void, IrType::Bool);
    active.SetImmediateBool(true);
    emit(IrOpcode::WriteSharedU32, IrType::Void, {zero, fourth, &active});
    ShaderComputeInputInfo compute;
    compute.threadsNum[0] = 32;
    compute.threadsNum[1] = 1;
    compute.threadsNum[2] = 1;
    compute.hostSubgroupSize = 32;
    compute.waveSize = 32;
    compute.ldsSizeDwords = 1;
    ShaderStageInputInfo inputs;
    inputs.compute = &compute;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00403000u;
    target.spirvVersion = 0x00010600u;
    target.subgroupSize = 32;
    const auto words = ValidateAndOptimizeSpirv(SpirvEmitter().Emit(program, inputs, bindings, target), target.vulkanVersion, target.spirvVersion, false, !certified, !certified);
    std::unordered_set<std::uint32_t> combinedConditions;
    std::size_t combinedSelects = 0;
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    bool storedNan = false;
    std::size_t entryChecks = 0u;
    std::unordered_map<std::uint32_t, std::array<std::uint32_t, 2>> magnitudeComparisons;
    for (std::size_t cursor = 5; cursor < words.size(); cursor += words[cursor] >> 16u) {
        const auto opcode = words[cursor] & 0xffffu;
        if (opcode == spv::OpConstant && (words[cursor] >> 16u) == 4u) constants.emplace(words[cursor + 2], words[cursor + 3]);
        if (opcode == spv::OpUGreaterThan) magnitudeComparisons.emplace(words[cursor + 2], std::array{words[cursor + 3], words[cursor + 4]});
        if (opcode == spv::OpSelect) {
            const auto comparison = magnitudeComparisons.find(words[cursor + 3]);
            if (comparison != magnitudeComparisons.end() && comparison->second == std::array{words[cursor + 4], words[cursor + 5]}) ++entryChecks;
        }
        if (opcode == spv::OpLogicalOr) combinedConditions.insert(words[cursor + 2]);
        if (opcode == spv::OpSelect && combinedConditions.contains(words[cursor + 3])) ++combinedSelects;
        if (opcode == spv::OpStore) {
            const auto value = constants.find(words[cursor + 2]);
            storedNan |= value != constants.end() && value->second == 0x7fc12345u;
        }
    }
    if (entryFact != EntryFact::None) require(entryChecks == (entryFact == EntryFact::Saturate ? 0u : 1u), "an entry certificate must reuse magnitude aliases and unconditional bounds without assuming an arithmetic result is bounded");
    if (certified) require(combinedSelects == (unproved ? 2u : 1u), "a certified finite condition must retain only its exact-fallback selection");
    else require(storedNan, "a NaN predicate crossing arithmetic regions lost its exceptional value");
}
#endif

}

int main() {
    try {
        constexpr std::array<std::uint32_t, 2> zeroOffsetBranch{0xbf840000u, 0xbf810000u};
        requireDistinctTargets("s_cbranch_scc0 to the next instruction", zeroOffsetBranch);
        constexpr std::array<std::uint32_t, 4> unalignedStore{0x7e020281u, 0xe0700000u, 0x80020100u, 0xbf810000u};
        requireDistinctTargets("store through a descriptor with an unaligned base", unalignedStore);
#if ANYPS5_ENABLE_SPIRV_TOOLS
        requireCrossRegionNan(false);
        requireCrossRegionNan(true);
        requireCrossRegionNan(false, true);
        requireCrossRegionNan(false, true, true);
        requireCrossRegionNan(false, true, false, EntryFact::Saturate);
        requireCrossRegionNan(false, true, false, EntryFact::Alias);
        requireCrossRegionNan(false, true, false, EntryFact::Product);
#endif
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvBranchTargets: %s\n", error.what());
        return 1;
    }
    std::puts("SpirvBranchTargets: no conditional branch names one label twice");
    return 0;
}
