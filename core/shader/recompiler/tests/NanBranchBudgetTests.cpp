#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include "Recompiler.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

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

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, "cannot set test environment");
#else
    Require(setenv(name, value, 1) == 0, "cannot set test environment");
#endif
}

constexpr std::uint32_t Vop2MulF32 = 0x08u;
constexpr std::uint32_t Vop2OpcodeShift = 25u;
constexpr std::uint32_t Vop2DestinationShift = 17u;
constexpr std::uint32_t Vop2Source1Shift = 9u;
constexpr std::uint32_t Vop2Source0VectorBase = 256u;

RdnaInstruction DecodeMulF32OfTwoRegisters(std::uint32_t index) {
    const std::uint32_t destination = 16u + index;
    const std::uint32_t source0 = 2u * index;
    const std::uint32_t source1 = 2u * index + 1u;
    const std::array<std::uint32_t, 1> words = {(Vop2MulF32 << Vop2OpcodeShift) | (destination << Vop2DestinationShift) | (source1 << Vop2Source1Shift) | (Vop2Source0VectorBase + source0)};
    const RdnaInstruction inst = DecodeRdnaVectorOp(std::span<const std::uint32_t>(words), 0u);

    EXPECT_EQ(inst.family, RdnaInstructionFamily::VOP2);
    EXPECT_EQ(inst.op, RdnaOpcode::VMulF32);
    EXPECT_EQ(inst.sourceCount, 2u);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VectorRegister);
    EXPECT_EQ(inst.destination.reg, destination);
    EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
    EXPECT_EQ(inst.source0.reg, source0);
    EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VectorRegister);
    EXPECT_EQ(inst.source1.reg, source1);
    return inst;
}

std::uint32_t CountOpcode(const IrBlock& block, IrOpcode opcode) {
    std::uint32_t count = 0u;
    for (const auto* val : block.Instructions()) {
        if (val->Opcode() == opcode) {
            ++count;
        }
    }
    return count;
}

void TestNoCounterKeepsEveryBranch() {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    for (std::uint32_t index = 0u; index < 5u; ++index) {
        context.TranslateInstruction(DecodeMulF32OfTwoRegisters(index));
    }

    EXPECT_EQ(CountOpcode(block, IrOpcode::SetVectorRegister), 5u);
    EXPECT_EQ(CountOpcode(block, IrOpcode::NanPropagateF32), 5u);
    EXPECT_EQ(CountOpcode(block, IrOpcode::FPIsNan32), 0u);
}

void TestSharedCounterStopsAtTheBudget() {
    IrProgram program;
    auto& first = program.CreateBlock();
    auto& second = program.CreateBlock();
    program.SetEntryBlock(first);
    std::uint32_t nanPropagateCount = 0u;

    {
        TranslationContext context(program, first, 256);
        context.SetNanPropagateCounter(&nanPropagateCount);
        for (std::uint32_t index = 0u; index < 2u; ++index) {
            context.TranslateInstruction(DecodeMulF32OfTwoRegisters(index));
        }
    }
    EXPECT_EQ(nanPropagateCount, 2u);
    EXPECT_EQ(CountOpcode(first, IrOpcode::SetVectorRegister), 2u);
    EXPECT_EQ(CountOpcode(first, IrOpcode::NanPropagateF32), 2u);
    EXPECT_EQ(CountOpcode(first, IrOpcode::FPIsNan32), 0u);

    {
        TranslationContext context(program, second, 256);
        context.SetNanPropagateCounter(&nanPropagateCount);
        for (std::uint32_t index = 2u; index < 5u; ++index) {
            context.TranslateInstruction(DecodeMulF32OfTwoRegisters(index));
        }
    }
    EXPECT_EQ(nanPropagateCount, 3u);
    EXPECT_EQ(CountOpcode(second, IrOpcode::SetVectorRegister), 3u);
    EXPECT_EQ(CountOpcode(second, IrOpcode::NanPropagateF32), 1u);
    EXPECT_EQ(CountOpcode(first, IrOpcode::NanPropagateF32) + CountOpcode(second, IrOpcode::NanPropagateF32), 3u);
    Require(CountOpcode(second, IrOpcode::FPIsNan32) >= 2u, "each of the two results past the budget tests for NaN in a select chain");
}

}

int main() {
    SetEnvironment("APS5_NAN_BRANCH_BUDGET", "3");
    SetEnvironment("APS5_FAST_F32", "0");
    EXPECT_EQ(NanBranchBudget(), 3u);
    EXPECT_EQ(ExactF32Arithmetic(), true);

    TestNoCounterKeepsEveryBranch();
    TestSharedCounterStopsAtTheBudget();
    return 0;
}
