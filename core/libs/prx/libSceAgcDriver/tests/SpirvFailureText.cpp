#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "PipelineSpecialization.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

enum class Failure { MissingComputeInput, UnloweredInstruction };

std::string emissionFailure(Failure failure) {
    IrProgram program;
    program.SetWaveSize(32);
    program.Resources().stage = IrShaderStage::Compute;
    program.Resources().shaderHash = 0x1173ae1000ull;
    program.Resources().srtPlanComplete = true;
    program.Resources().resourceTrackingComplete = true;
    program.Metadata().shaderInfoComplete = true;
    program.Metadata().bindingLayoutComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    program.Metadata().blockInfo.emplace_back();
    if (failure == Failure::UnloweredInstruction) block.AppendInstruction(&program.CreateValue(IrOpcode::Unreachable, IrType::Void));
    ShaderComputeInputInfo compute;
    compute.threadsNum[0] = 32;
    compute.threadsNum[1] = 1;
    compute.threadsNum[2] = 1;
    compute.hostSubgroupSize = 32;
    compute.waveSize = 32;
    ShaderStageInputInfo inputs;
    if (failure != Failure::MissingComputeInput) inputs.compute = &compute;
    CompiledBindingLayout bindings;
    bindings.layout = program.Metadata().bindings;
    SpirvTargetOptions target{};
    target.vulkanVersion = 0x00401000;
    target.spirvVersion = 0x00010300;
    target.subgroupSize = 32;
    try {
        static_cast<void>(SpirvEmitter().Emit(program, inputs, bindings, target));
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("the program emitted without a failure");
}

void requireFailure(Failure failure, std::string_view prefix, std::string_view reason) {
    const auto text = emissionFailure(failure);
    require(text.starts_with(prefix) && text.find(reason) != std::string::npos, "expected \"" + std::string(prefix) + "...\" with \"" + std::string(reason) + "\", got \"" + text + "\"");
}

}

int main() {
    try {
        requireFailure(Failure::MissingComputeInput, "SPIR-V emission failed: hash=0x1173ae1000 stage=", " reason=compute input info is missing");
        requireFailure(Failure::UnloweredInstruction, "SPIR-V emission failed: hash=0x1173ae1000 stage=", " opcode=Unreachable reason=");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvFailureText: %s\n", error.what());
        return 1;
    }
    std::puts("SpirvFailureText: emission failures name the shader hash in hex");
    return 0;
}
