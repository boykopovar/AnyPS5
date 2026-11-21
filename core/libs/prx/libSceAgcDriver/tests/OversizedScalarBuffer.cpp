#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ResourceTracker.hpp"
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {

void Check(std::uint32_t stride, std::uint32_t records, bool scalar, bool dynamicStride, bool expectedBda) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.BlockOrder().push_back(&block);
    program.SetEntryBlock(block);
    program.Resources().stage = IrShaderStage::Compute;
    program.Resources().srtPlanComplete = true;
    program.Resources().userDataCount = 1u;
    const auto constant = [&](std::uint32_t word) -> IrValue& {
        auto& value = program.CreateValue(IrOpcode::Void, IrType::U32);
        value.SetImmediateU32(word);
        return value;
    };
    const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) -> IrValue& {
        auto& value = program.CreateValue(opcode, type);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return value;
    };
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& high = builder.GetUserData(static_cast<ScalarReg>(0u));
    auto& addressHigh = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&high, &constant(0xffffu)});
    auto& strideBits = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&constant(stride), &constant(16u)});
    auto& word1 = emit(IrOpcode::BitwiseOr32, IrType::U32, {&addressHigh, dynamicStride ? &high : &strideBits});
    auto& descriptor = emit(IrOpcode::GetBufferResource, IrType::BufferResource, {&constant(0x10000u), &word1, &constant(records), &constant(0x00005204u)});
    MemoryInfo memory;
    memory.kind = scalar ? ResourceKind::ScalarBuffer : ResourceKind::Buffer;
    program.Resources().memoryInfo.push_back(memory);
    auto& load = scalar ? emit(IrOpcode::ReadConstBuffer, IrType::U32, {&descriptor, &constant(0u)})
                        : emit(IrOpcode::LoadBufferU32, IrType::U32, {&descriptor, &constant(0u), &constant(0u), &constant(0u), &constant(1u)});
    load.SetFlags(MemoryFlags{0u, 0u});
    emit(IrOpcode::ReferenceU32, IrType::Void, {&load});
    ResourceTracker{}.Track(program);
    if (program.Resources().memoryInfo.front().gpuDescriptor != expectedBda || program.Info().usesDma != expectedBda) {
        std::fprintf(stderr, "stride=%u records=%u scalar=%d dynamic=%d expected=%d actual=%d dma=%d\n", stride, records, scalar, dynamicStride, expectedBda, program.Resources().memoryInfo.front().gpuDescriptor, program.Info().usesDma);
        throw std::runtime_error("oversized scalar buffer selected the wrong access path");
    }
    if (program.Info().bdaWrites) throw std::runtime_error("scalar load was classified as a write");
    if (expectedBda && !program.Info().buffers.empty()) throw std::runtime_error("oversized scalar buffer still needs a storage descriptor");
}

}

int main() {
    try {
        Check(16u, 0xffffffffu, true, false, true);
        Check(2u, 0x80000000u, true, false, true);
        Check(1u, 0xffffffffu, true, false, false);
        Check(0u, 0xffffffffu, true, false, false);
        Check(16u, 16u, true, false, false);
        Check(16u, 0xffffffffu, true, true, false);
        Check(16u, 0xffffffffu, false, false, false);
        std::puts("oversized scalar buffer path tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
