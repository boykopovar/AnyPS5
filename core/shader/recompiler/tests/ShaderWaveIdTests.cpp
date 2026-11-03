#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
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

RdnaInstruction MakeWaveIdInstruction(std::uint32_t sdst) {
    const std::uint32_t word0 = (0x3Du << 26) | (0x2au << 18) | ((sdst & 0x7fu) << 6);
    const std::uint32_t word1 = 0u;
    const std::array<std::uint32_t, 2> words = {word0, word1};
    return DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
}

IrValue* FindInstruction(const IrBlock& block, IrOpcode opcode) {
    for (IrValue* val : block.Instructions()) {
        if (val->Opcode() == opcode) {
            return val;
        }
    }
    return nullptr;
}

void TestComputeStage(std::uint32_t waveSize, std::uint32_t sdst) {
    const RdnaInstruction inst = MakeWaveIdInstruction(sdst);
    EXPECT_EQ(inst.op, RdnaOpcode::SGetWaveidInWorkgroup);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, sdst);

    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.SetWaveSize(waveSize);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(inst);

    IrValue* getBuiltin = FindInstruction(block, IrOpcode::GetBuiltin);
    Require(getBuiltin != nullptr, "Compute stage must emit GetBuiltin");
    EXPECT_EQ(getBuiltin->ArgumentCount(), 2u);
    Require(getBuiltin->Argument(0)->HasImmediate(), "GetBuiltin argument 0 must be immediate");
    EXPECT_EQ(getBuiltin->Argument(0)->ImmediateU32(), static_cast<std::uint32_t>(StageInputKind::LocalInvocationIndex));
    Require(getBuiltin->Argument(1)->HasImmediate(), "GetBuiltin argument 1 must be immediate");
    EXPECT_EQ(getBuiltin->Argument(1)->ImmediateU32(), 0u);

    IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
    Require(udiv != nullptr, "Compute stage must emit UDiv32");
    EXPECT_EQ(udiv->ArgumentCount(), 2u);
    EXPECT_EQ(udiv->Argument(0), getBuiltin);
    Require(udiv->Argument(1)->HasImmediate(), "UDiv32 argument 1 must be immediate wave size");
    EXPECT_EQ(udiv->Argument(1)->ImmediateU32(), waveSize);

    IrValue* setScalar = FindInstruction(block, IrOpcode::SetScalarRegister);
    Require(setScalar != nullptr, "Compute stage must emit SetScalarRegister");
    EXPECT_EQ(setScalar->ArgumentCount(), 2u);
    EXPECT_EQ(setScalar->Argument(0)->Register().bank, RegisterBank::Scalar);
    EXPECT_EQ(setScalar->Argument(0)->Register().index, sdst);
    EXPECT_EQ(setScalar->Argument(1), udiv);
}

void TestMeshStage(std::uint32_t waveSize, std::uint32_t sdst) {
    const RdnaInstruction inst = MakeWaveIdInstruction(sdst);
    EXPECT_EQ(inst.op, RdnaOpcode::SGetWaveidInWorkgroup);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, sdst);

    IrProgram program;
    program.Resources().stage = IrShaderStage::Mesh;
    program.SetWaveSize(waveSize);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(inst);

    IrValue* getBuiltin = FindInstruction(block, IrOpcode::GetBuiltin);
    Require(getBuiltin != nullptr, "Mesh stage must emit GetBuiltin");
    EXPECT_EQ(getBuiltin->ArgumentCount(), 2u);
    Require(getBuiltin->Argument(0)->HasImmediate(), "GetBuiltin argument 0 must be immediate");
    EXPECT_EQ(getBuiltin->Argument(0)->ImmediateU32(), static_cast<std::uint32_t>(StageInputKind::LocalInvocationIndex));
    Require(getBuiltin->Argument(1)->HasImmediate(), "GetBuiltin argument 1 must be immediate");
    EXPECT_EQ(getBuiltin->Argument(1)->ImmediateU32(), 0u);

    IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
    Require(udiv != nullptr, "Mesh stage must emit UDiv32");
    EXPECT_EQ(udiv->ArgumentCount(), 2u);
    EXPECT_EQ(udiv->Argument(0), getBuiltin);
    Require(udiv->Argument(1)->HasImmediate(), "UDiv32 argument 1 must be immediate wave size");
    EXPECT_EQ(udiv->Argument(1)->ImmediateU32(), waveSize);

    IrValue* setScalar = FindInstruction(block, IrOpcode::SetScalarRegister);
    Require(setScalar != nullptr, "Mesh stage must emit SetScalarRegister");
    EXPECT_EQ(setScalar->ArgumentCount(), 2u);
    EXPECT_EQ(setScalar->Argument(0)->Register().bank, RegisterBank::Scalar);
    EXPECT_EQ(setScalar->Argument(0)->Register().index, sdst);
    EXPECT_EQ(setScalar->Argument(1), udiv);
}

void TestGraphicsStagesZero(IrShaderStage stage, std::uint32_t waveSize, std::uint32_t sdst) {
    const RdnaInstruction inst = MakeWaveIdInstruction(sdst);
    EXPECT_EQ(inst.op, RdnaOpcode::SGetWaveidInWorkgroup);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, sdst);

    IrProgram program;
    program.Resources().stage = stage;
    program.SetWaveSize(waveSize);
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(inst);

    Require(FindInstruction(block, IrOpcode::GetBuiltin) == nullptr, "Graphics stages must not emit GetBuiltin");
    Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Graphics stages must not emit UDiv32");

    IrValue* setScalar = FindInstruction(block, IrOpcode::SetScalarRegister);
    Require(setScalar != nullptr, "Graphics stages must emit SetScalarRegister");
    EXPECT_EQ(setScalar->ArgumentCount(), 2u);
    EXPECT_EQ(setScalar->Argument(0)->Register().bank, RegisterBank::Scalar);
    EXPECT_EQ(setScalar->Argument(0)->Register().index, sdst);
    Require(setScalar->Argument(1)->HasImmediate(), "SetScalarRegister value must be immediate");
    EXPECT_EQ(setScalar->Argument(1)->ImmediateU32(), 0u);
}

void TestSpecialDestinations() {
    constexpr std::uint32_t nullSdst = 125u;
    constexpr std::uint32_t m0Sdst = 124u;
    constexpr std::uint32_t vccLoSdst = 106u;
    constexpr std::uint32_t vccHiSdst = 107u;
    constexpr std::uint32_t execLoSdst = 126u;
    constexpr std::uint32_t execHiSdst = 127u;

    const RdnaInstruction nullInst = MakeWaveIdInstruction(nullSdst);
    EXPECT_EQ(nullInst.destination.kind, RdnaOperandKind::Null);

    const RdnaInstruction m0Inst = MakeWaveIdInstruction(m0Sdst);
    EXPECT_EQ(m0Inst.destination.kind, RdnaOperandKind::M0);

    const RdnaInstruction vccLoInst = MakeWaveIdInstruction(vccLoSdst);
    EXPECT_EQ(vccLoInst.destination.kind, RdnaOperandKind::VccLo);

    const RdnaInstruction vccHiInst = MakeWaveIdInstruction(vccHiSdst);
    EXPECT_EQ(vccHiInst.destination.kind, RdnaOperandKind::VccHi);

    const RdnaInstruction execLoInst = MakeWaveIdInstruction(execLoSdst);
    EXPECT_EQ(execLoInst.destination.kind, RdnaOperandKind::ExecLo);

    const RdnaInstruction execHiInst = MakeWaveIdInstruction(execHiSdst);
    EXPECT_EQ(execHiInst.destination.kind, RdnaOperandKind::ExecHi);

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(nullInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute Null destination must emit UDiv32");
        Require(FindInstruction(block, IrOpcode::SetScalarRegister) == nullptr, "Compute Null destination must not emit SetScalarRegister");
        Require(FindInstruction(block, IrOpcode::SetM0) == nullptr, "Compute Null destination must not emit SetM0");
        Require(FindInstruction(block, IrOpcode::SetVccLo) == nullptr, "Compute Null destination must not emit SetVccLo");
        Require(FindInstruction(block, IrOpcode::SetVccHi) == nullptr, "Compute Null destination must not emit SetVccHi");
        Require(FindInstruction(block, IrOpcode::SetExecLo) == nullptr, "Compute Null destination must not emit SetExecLo");
        Require(FindInstruction(block, IrOpcode::SetExecHi) == nullptr, "Compute Null destination must not emit SetExecHi");
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(nullInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh Null destination must emit UDiv32");
        Require(FindInstruction(block, IrOpcode::SetScalarRegister) == nullptr, "Mesh Null destination must not emit SetScalarRegister");
        Require(FindInstruction(block, IrOpcode::SetM0) == nullptr, "Mesh Null destination must not emit SetM0");
        Require(FindInstruction(block, IrOpcode::SetVccLo) == nullptr, "Mesh Null destination must not emit SetVccLo");
        Require(FindInstruction(block, IrOpcode::SetVccHi) == nullptr, "Mesh Null destination must not emit SetVccHi");
        Require(FindInstruction(block, IrOpcode::SetExecLo) == nullptr, "Mesh Null destination must not emit SetExecLo");
        Require(FindInstruction(block, IrOpcode::SetExecHi) == nullptr, "Mesh Null destination must not emit SetExecHi");
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Pixel;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(nullInst);

        EXPECT_EQ(block.Instructions().empty(), true);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(m0Inst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute M0 destination must emit UDiv32");
        IrValue* setM0 = FindInstruction(block, IrOpcode::SetM0);
        Require(setM0 != nullptr, "Compute M0 destination must emit SetM0");
        EXPECT_EQ(setM0->ArgumentCount(), 1u);
        EXPECT_EQ(setM0->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(m0Inst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh M0 destination must emit UDiv32");
        IrValue* setM0 = FindInstruction(block, IrOpcode::SetM0);
        Require(setM0 != nullptr, "Mesh M0 destination must emit SetM0");
        EXPECT_EQ(setM0->ArgumentCount(), 1u);
        EXPECT_EQ(setM0->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Vertex;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(m0Inst);

        Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Vertex M0 must not emit UDiv32");
        IrValue* setM0 = FindInstruction(block, IrOpcode::SetM0);
        Require(setM0 != nullptr, "Vertex M0 destination must emit SetM0");
        EXPECT_EQ(setM0->ArgumentCount(), 1u);
        Require(setM0->Argument(0)->HasImmediate(), "SetM0 argument must be immediate");
        EXPECT_EQ(setM0->Argument(0)->ImmediateU32(), 0u);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccLoInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute VccLo destination must emit UDiv32");
        IrValue* setVccLo = FindInstruction(block, IrOpcode::SetVccLo);
        Require(setVccLo != nullptr, "Compute VccLo destination must emit SetVccLo");
        EXPECT_EQ(setVccLo->ArgumentCount(), 1u);
        EXPECT_EQ(setVccLo->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccLoInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh VccLo destination must emit UDiv32");
        IrValue* setVccLo = FindInstruction(block, IrOpcode::SetVccLo);
        Require(setVccLo != nullptr, "Mesh VccLo destination must emit SetVccLo");
        EXPECT_EQ(setVccLo->ArgumentCount(), 1u);
        EXPECT_EQ(setVccLo->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Pixel;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccLoInst);

        Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Pixel VccLo must not emit UDiv32");
        IrValue* setVccLo = FindInstruction(block, IrOpcode::SetVccLo);
        Require(setVccLo != nullptr, "Pixel VccLo destination must emit SetVccLo");
        EXPECT_EQ(setVccLo->ArgumentCount(), 1u);
        Require(setVccLo->Argument(0)->HasImmediate(), "SetVccLo argument must be immediate");
        EXPECT_EQ(setVccLo->Argument(0)->ImmediateU32(), 0u);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccHiInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute VccHi destination must emit UDiv32");
        IrValue* setVccHi = FindInstruction(block, IrOpcode::SetVccHi);
        Require(setVccHi != nullptr, "Compute VccHi destination must emit SetVccHi");
        EXPECT_EQ(setVccHi->ArgumentCount(), 1u);
        EXPECT_EQ(setVccHi->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccHiInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh VccHi destination must emit UDiv32");
        IrValue* setVccHi = FindInstruction(block, IrOpcode::SetVccHi);
        Require(setVccHi != nullptr, "Mesh VccHi destination must emit SetVccHi");
        EXPECT_EQ(setVccHi->ArgumentCount(), 1u);
        EXPECT_EQ(setVccHi->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Pixel;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(vccHiInst);

        Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Pixel VccHi must not emit UDiv32");
        IrValue* setVccHi = FindInstruction(block, IrOpcode::SetVccHi);
        Require(setVccHi != nullptr, "Pixel VccHi destination must emit SetVccHi");
        EXPECT_EQ(setVccHi->ArgumentCount(), 1u);
        Require(setVccHi->Argument(0)->HasImmediate(), "SetVccHi argument must be immediate");
        EXPECT_EQ(setVccHi->Argument(0)->ImmediateU32(), 0u);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execLoInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute ExecLo destination must emit UDiv32");
        IrValue* setExecLo = FindInstruction(block, IrOpcode::SetExecLo);
        Require(setExecLo != nullptr, "Compute ExecLo destination must emit SetExecLo");
        EXPECT_EQ(setExecLo->ArgumentCount(), 1u);
        EXPECT_EQ(setExecLo->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execLoInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh ExecLo destination must emit UDiv32");
        IrValue* setExecLo = FindInstruction(block, IrOpcode::SetExecLo);
        Require(setExecLo != nullptr, "Mesh ExecLo destination must emit SetExecLo");
        EXPECT_EQ(setExecLo->ArgumentCount(), 1u);
        EXPECT_EQ(setExecLo->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Vertex;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execLoInst);

        Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Vertex ExecLo must not emit UDiv32");
        IrValue* setExecLo = FindInstruction(block, IrOpcode::SetExecLo);
        Require(setExecLo != nullptr, "Vertex ExecLo destination must emit SetExecLo");
        EXPECT_EQ(setExecLo->ArgumentCount(), 1u);
        Require(setExecLo->Argument(0)->HasImmediate(), "SetExecLo argument must be immediate");
        EXPECT_EQ(setExecLo->Argument(0)->ImmediateU32(), 0u);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Compute;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execHiInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Compute ExecHi destination must emit UDiv32");
        IrValue* setExecHi = FindInstruction(block, IrOpcode::SetExecHi);
        Require(setExecHi != nullptr, "Compute ExecHi destination must emit SetExecHi");
        EXPECT_EQ(setExecHi->ArgumentCount(), 1u);
        EXPECT_EQ(setExecHi->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Mesh;
        program.SetWaveSize(64u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execHiInst);

        IrValue* udiv = FindInstruction(block, IrOpcode::UDiv32);
        Require(udiv != nullptr, "Mesh ExecHi destination must emit UDiv32");
        IrValue* setExecHi = FindInstruction(block, IrOpcode::SetExecHi);
        Require(setExecHi != nullptr, "Mesh ExecHi destination must emit SetExecHi");
        EXPECT_EQ(setExecHi->ArgumentCount(), 1u);
        EXPECT_EQ(setExecHi->Argument(0), udiv);
    }

    {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Vertex;
        program.SetWaveSize(32u);
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(execHiInst);

        Require(FindInstruction(block, IrOpcode::UDiv32) == nullptr, "Vertex ExecHi must not emit UDiv32");
        IrValue* setExecHi = FindInstruction(block, IrOpcode::SetExecHi);
        Require(setExecHi != nullptr, "Vertex ExecHi destination must emit SetExecHi");
        EXPECT_EQ(setExecHi->ArgumentCount(), 1u);
        Require(setExecHi->Argument(0)->HasImmediate(), "SetExecHi argument must be immediate");
        EXPECT_EQ(setExecHi->Argument(0)->ImmediateU32(), 0u);
    }
}

}

int main() {
    constexpr std::array<std::uint32_t, 2> waveSizes = {32u, 64u};
    constexpr std::array<std::uint32_t, 3> registers = {0u, 5u, 105u};

    for (const std::uint32_t waveSize : waveSizes) {
        for (const std::uint32_t reg : registers) {
            TestComputeStage(waveSize, reg);
            TestMeshStage(waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::Vertex, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::Pixel, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::Fetch, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::Local, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::TessellationControl, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::TessellationEvaluation, waveSize, reg);
            TestGraphicsStagesZero(IrShaderStage::Unknown, waveSize, reg);
        }
    }

    TestSpecialDestinations();

    return 0;
}
