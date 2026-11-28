#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include "Recompiler.hpp"
#include "Translation/TranslationContext.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string_view>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
        std::abort();
    }
}

template <typename T, typename U>
void ExpectEq(const T& actual, const U& expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "ExpectEq failed: %s\n", message);
        std::abort();
    }
}
#define EXPECT_EQ(a, b) ExpectEq((a), (b), #a " == " #b)

alignas(256) constexpr std::array<std::uint32_t, 49> ExecKeptCode{
    0x34020082, 0xe0301000, 0x80000401, 0xbf8c3f70, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280,
    0x7e1c0280, 0xbe940380, 0xbea603ff, 0x5a5a5a5a, 0x7daa0880, 0xbda6000a, 0x80148114, 0x4a141481,
    0x7e160214, 0x7e180226, 0xbe96047e, 0x7e1a0216, 0x7e1c0217, 0xbf098414, 0xbf850001, 0xbe26fff6,
    0xbe98047e, 0xbefe04c1, 0x7e1e0226, 0x7e200218, 0x7e220219, 0x7e240214, 0xe0701000, 0x80010a01,
    0xe0701100, 0x80010b01, 0xe0701200, 0x80010c01, 0xe0701300, 0x80010d01, 0xe0701400, 0x80010e01,
    0xe0701500, 0x80010f01, 0xe0701600, 0x80011001, 0xe0701700, 0x80011101, 0xe0701800, 0x80011201,
    0xbf810000,
};

alignas(256) std::array<std::uint32_t, 64> TestInput{};
alignas(256) std::array<std::uint32_t, 64 * 9> TestOutput{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void TestWave32SubvectorLoopBeginIR() {
    IrProgram program;
    program.SetWaveSize(32u);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);

    constexpr std::uint32_t beginWord = (0xbu << 28) | (27u << 23) | (10u << 16) | 0x000au;
    const auto inst = DecodeRdnaInstruction(0u, std::span(&beginWord, 1), 0u);
    EXPECT_EQ(inst.op, RdnaOpcode::SSubvectorLoopBegin);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, 10u);

    context.TranslateInstruction(inst);

    BasicBlock cfgBlock{};
    cfgBlock.id = 1;
    cfgBlock.terminator.kind = TerminatorKind::ConditionalBranch;
    cfgBlock.terminator.condition = BranchCondition::ScalarInstruction;
    BlockInfo blockInfo{};
    context.AddBranchCondition(cfgBlock, blockInfo);
    Require(blockInfo.condition != nullptr, "branch condition not set");

    const IrValue& branchCond = *blockInfo.condition;
    EXPECT_EQ(branchCond.Opcode(), IrOpcode::IEqual32);
    EXPECT_EQ(branchCond.Argument(0)->Opcode(), IrOpcode::BitwiseOr32);
    EXPECT_EQ(branchCond.Argument(1)->HasImmediate(), true);
    EXPECT_EQ(branchCond.Argument(1)->ImmediateU32(), 0u);

    bool foundSetScalar = false;
    bool foundSetExecLo = false;
    bool foundSetExecHi = false;
    bool foundSetExec = false;
    for (const auto* val : block.Instructions()) {
        if (val->Opcode() == IrOpcode::SetScalarRegister) {
            EXPECT_EQ(val->Argument(0)->Register().index, 10u);
            const IrValue* assigned = val->Argument(1);
            EXPECT_EQ(assigned->Opcode(), IrOpcode::SelectU32);
            EXPECT_EQ(assigned->Argument(0), &branchCond);
            EXPECT_EQ(assigned->Argument(1)->Opcode(), IrOpcode::GetScalarRegister);
            EXPECT_EQ(assigned->Argument(1)->Argument(0)->Register().index, 10u);
            EXPECT_EQ(assigned->Argument(2)->Opcode(), IrOpcode::SelectU32);
            foundSetScalar = true;
        }
        if (val->Opcode() == IrOpcode::SetExecLo) {
            EXPECT_EQ(val->Argument(0)->Opcode(), IrOpcode::GetExecLo);
            foundSetExecLo = true;
        }
        if (val->Opcode() == IrOpcode::SetExecHi) {
            EXPECT_EQ(val->Argument(0)->Opcode(), IrOpcode::SelectU32);
            foundSetExecHi = true;
        }
        if (val->Opcode() == IrOpcode::SetExec) {
            foundSetExec = true;
        }
    }
    EXPECT_EQ(foundSetScalar, true);
    EXPECT_EQ(foundSetExecLo, true);
    EXPECT_EQ(foundSetExecHi, true);
    EXPECT_EQ(foundSetExec, true);
}

void TestWave32SubvectorLoopEndIR() {
    IrProgram program;
    program.SetWaveSize(32u);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);

    constexpr std::uint32_t endWord = (0xbu << 28) | (28u << 23) | (10u << 16) | 0xfff6u;
    const auto inst = DecodeRdnaInstruction(0u, std::span(&endWord, 1), 0u);
    EXPECT_EQ(inst.op, RdnaOpcode::SSubvectorLoopEnd);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, 10u);

    context.TranslateInstruction(inst);

    BasicBlock cfgBlock{};
    cfgBlock.id = 2;
    cfgBlock.terminator.kind = TerminatorKind::ConditionalBranch;
    cfgBlock.terminator.condition = BranchCondition::ScalarInstruction;
    BlockInfo blockInfo{};
    context.AddBranchCondition(cfgBlock, blockInfo);
    Require(blockInfo.condition != nullptr, "branch condition not set");

    const IrValue& branchCond = *blockInfo.condition;
    EXPECT_EQ(branchCond.Opcode(), IrOpcode::LogicalNot);

    bool foundSetScalar = false;
    bool foundSetExecLo = false;
    bool foundSetExecHi = false;
    bool foundSetExec = false;
    for (const auto* val : block.Instructions()) {
        if (val->Opcode() == IrOpcode::SetScalarRegister) {
            EXPECT_EQ(val->Argument(0)->Register().index, 10u);
            const IrValue* assigned = val->Argument(1);
            EXPECT_EQ(assigned->Opcode(), IrOpcode::SelectU32);
            foundSetScalar = true;
        }
        if (val->Opcode() == IrOpcode::SetExecLo) {
            EXPECT_EQ(val->Argument(0)->Opcode(), IrOpcode::SelectU32);
            foundSetExecLo = true;
        }
        if (val->Opcode() == IrOpcode::SetExecHi) {
            EXPECT_EQ(val->Argument(0)->Opcode(), IrOpcode::SelectU32);
            foundSetExecHi = true;
        }
        if (val->Opcode() == IrOpcode::SetExec) {
            foundSetExec = true;
        }
    }
    EXPECT_EQ(foundSetScalar, true);
    EXPECT_EQ(foundSetExecLo, true);
    EXPECT_EQ(foundSetExecHi, true);
    EXPECT_EQ(foundSetExec, true);
}

void TestWave64SubvectorLoopIR() {
    {
        IrProgram program;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        constexpr std::uint32_t beginWord = (0xbu << 28) | (27u << 23) | (12u << 16) | 0x000au;
        const auto inst = DecodeRdnaInstruction(0u, std::span(&beginWord, 1), 0u);
        EXPECT_EQ(inst.op, RdnaOpcode::SSubvectorLoopBegin);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
        EXPECT_EQ(inst.destination.reg, 12u);

        context.TranslateInstruction(inst);

        BasicBlock cfgBlock{};
        cfgBlock.id = 1;
        cfgBlock.terminator.kind = TerminatorKind::ConditionalBranch;
        cfgBlock.terminator.condition = BranchCondition::ScalarInstruction;
        BlockInfo blockInfo{};
        context.AddBranchCondition(cfgBlock, blockInfo);
        Require(blockInfo.condition != nullptr, "branch condition not set");

        const IrValue& branchCond = *blockInfo.condition;
        EXPECT_EQ(branchCond.Opcode(), IrOpcode::IEqual32);
        EXPECT_EQ(branchCond.Argument(0)->Opcode(), IrOpcode::BitwiseOr32);
        EXPECT_EQ(branchCond.Argument(1)->HasImmediate(), true);
        EXPECT_EQ(branchCond.Argument(1)->ImmediateU32(), 0u);
    }
    {
        IrProgram program;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        constexpr std::uint32_t endWord = (0xbu << 28) | (28u << 23) | (12u << 16) | 0xfff6u;
        const auto inst = DecodeRdnaInstruction(0u, std::span(&endWord, 1), 0u);
        EXPECT_EQ(inst.op, RdnaOpcode::SSubvectorLoopEnd);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
        EXPECT_EQ(inst.destination.reg, 12u);

        context.TranslateInstruction(inst);

        BasicBlock cfgBlock{};
        cfgBlock.id = 2;
        cfgBlock.terminator.kind = TerminatorKind::ConditionalBranch;
        cfgBlock.terminator.condition = BranchCondition::ScalarInstruction;
        BlockInfo blockInfo{};
        context.AddBranchCondition(cfgBlock, blockInfo);
        Require(blockInfo.condition != nullptr, "branch condition not set");

        const IrValue& branchCond = *blockInfo.condition;
        EXPECT_EQ(branchCond.Opcode(), IrOpcode::LogicalNot);
    }
}

void TestSubvectorLoopRefusal() {
    {
        IrProgram program;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        constexpr std::uint32_t vccBeginWord = (0xbu << 28) | (27u << 23) | (106u << 16) | 0x000au;
        const auto inst = DecodeRdnaInstruction(0u, std::span(&vccBeginWord, 1), 0u);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VccLo);

        bool threw = false;
        try {
            context.TranslateInstruction(inst);
        } catch (const std::runtime_error& error) {
            threw = true;
            EXPECT_EQ(std::string_view(error.what()).find("with a destination that is not an SGPR is not measured") != std::string_view::npos, true);
        }
        EXPECT_EQ(threw, true);
    }
    {
        IrProgram program;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        constexpr std::uint32_t vccEndWord = (0xbu << 28) | (28u << 23) | (106u << 16) | 0xfff6u;
        const auto inst = DecodeRdnaInstruction(0u, std::span(&vccEndWord, 1), 0u);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VccLo);

        bool threw = false;
        try {
            context.TranslateInstruction(inst);
        } catch (const std::runtime_error& error) {
            threw = true;
            EXPECT_EQ(std::string_view(error.what()).find("with a destination that is not an SGPR is not measured") != std::string_view::npos, true);
        }
        EXPECT_EQ(threw, true);
    }
}

void TestSpirvGeneration(std::uint32_t waveSize) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto inputDesc = BufferDescriptor(TestInput.data(), static_cast<std::uint32_t>(TestInput.size() * 4u));
    const auto outputDesc = BufferDescriptor(TestOutput.data(), static_cast<std::uint32_t>(TestOutput.size() * 4u));
    std::copy(inputDesc.begin(), inputDesc.end(), userData.begin());
    std::copy(outputDesc.begin(), outputDesc.end(), userData.begin() + 4);

    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(ExecKeptCode.data()), std::as_bytes(std::span(ExecKeptCode))}}};
    const ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};

    SpirvTarget target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = waveSize;

    RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(ExecKeptCode.data()), ExecKeptCode, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;

    const auto result = ShaderRecompiler::Recompile(request);
    EXPECT_EQ(result.spirv.empty(), false);
    EXPECT_EQ(result.spirv[0], 0x07230203u);
}

}

int main() {
    TestWave32SubvectorLoopBeginIR();
    TestWave32SubvectorLoopEndIR();
    TestWave64SubvectorLoopIR();
    TestSubvectorLoopRefusal();
    TestSpirvGeneration(32u);
    TestSpirvGeneration(64u);
    std::puts("All ShaderWave32SubvectorLoopTests passed.");
    return 0;
}
