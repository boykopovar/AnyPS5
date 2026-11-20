#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <spirv/unified1/spirv.hpp>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

IrValue& builtin(IrBuilder& builder, StageInputKind kind) {
    return builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(kind)), &builder.Constant(0u)});
}

std::vector<std::uint32_t> emit(IrShaderStage stage, const std::vector<StageInputKind>& kinds, bool wave64) {
    IrProgram program;
    IrBuilder builder{program};
    program.SetWaveSize(wave64 ? 64u : 32u);
    program.Resources().stage = stage;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    program.Metadata().blockInfo.emplace_back();
    builder.SetInsertionPoint(block);
    for (const auto kind : kinds) {
        auto& value = builtin(builder, kind);
        static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&value}));
    }
    ShaderComputeInputInfo compute;
    compute.threadsNum[0] = wave64 ? 64u : 32u;
    compute.threadsNum[1] = 1u;
    compute.threadsNum[2] = 1u;
    compute.hostSubgroupSize = 32u;
    compute.waveSize = wave64 ? 64u : 32u;
    ShaderStageInputInfo inputs;
    inputs.compute = &compute;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = 32u;
    return SpirvEmitter().Emit(program, inputs, bindings, target);
}

bool declaresBuiltin(const std::vector<std::uint32_t>& words, spv::BuiltIn builtin) {
    for (std::size_t at = 5; at < words.size(); at += words[at] >> 16u) {
        const auto count = words[at] >> 16u;
        if (count == 0u || at + count > words.size()) throw std::runtime_error("malformed SPIR-V instruction");
        if ((words[at] & 0xffffu) == spv::OpDecorate && count == 4u && words[at + 2u] == spv::DecorationBuiltIn && words[at + 3u] == static_cast<std::uint32_t>(builtin)) return true;
    }
    return false;
}

void checkComputeBuiltins() {
    const auto workgroup = emit(IrShaderStage::Compute, {StageInputKind::WorkgroupId}, false);
    require(declaresBuiltin(workgroup, spv::BuiltInWorkgroupId), "a compute program that reads WorkgroupId did not declare it");

    const auto global = emit(IrShaderStage::Compute, {StageInputKind::GlobalInvocationId}, true);
    require(declaresBuiltin(global, spv::BuiltInGlobalInvocationId), "a wave64 compute program that reads GlobalInvocationId did not declare it");
    require(declaresBuiltin(global, spv::BuiltInWorkgroupId), "the emulated GlobalInvocationId did not declare WorkgroupId");
    require(declaresBuiltin(global, spv::BuiltInLocalInvocationIndex), "the emulated GlobalInvocationId did not declare LocalInvocationIndex");

    const auto local = emit(IrShaderStage::Compute, {StageInputKind::LocalInvocationIndex}, false);
    require(declaresBuiltin(local, spv::BuiltInLocalInvocationIndex), "a compute program that reads LocalInvocationIndex did not declare it");
}

void checkUnusedBuiltinsStayUndeclared() {
    const auto words = emit(IrShaderStage::Compute, {}, false);
    require(!declaresBuiltin(words, spv::BuiltInWorkgroupId), "a compute program without WorkgroupId declared it");
    require(!declaresBuiltin(words, spv::BuiltInGlobalInvocationId), "a compute program without GlobalInvocationId declared it");
}

}

int main() {
    try {
        checkComputeBuiltins();
        checkUnusedBuiltinsStayUndeclared();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
