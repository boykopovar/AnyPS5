#include "ControlFlow/GraphBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar swappc regression"); }

template <typename Fn>
static void ExpectThrow(const char* needle, Fn fn) {
    try {
        fn();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(needle) == std::string::npos) {
            throw std::runtime_error(std::string("wrong error for \"") + needle + "\": " + error.what());
        }
        return;
    }
    throw std::runtime_error(std::string("expected throw containing \"") + needle + "\"");
}

static RdnaInstruction Sop1(std::uint32_t pc, std::uint32_t sdst, std::uint32_t opcode, std::uint32_t ssrc0) {
    const std::array<std::uint32_t, 2> words{0x80000000u | (0x7du << 23u) | (sdst << 16u) | (opcode << 8u) | ssrc0, 0u};
    return DecodeRdnaSop1(pc, words, 0u);
}

static RdnaInstruction Sop2Literal(std::uint32_t pc, std::uint32_t sdst, std::uint32_t opcode, std::uint32_t ssrc0, std::uint32_t literal) {
    const std::array<std::uint32_t, 2> words{0x80000000u | (opcode << 23u) | (sdst << 16u) | (0xffu << 8u) | ssrc0, literal};
    return DecodeRdnaSop2(pc, words, 0u);
}

static RdnaInstruction Sopp(std::uint32_t pc, std::uint32_t opcode) {
    const std::array<std::uint32_t, 1> words{0x80000000u | (0x7fu << 23u) | (opcode << 16u)};
    return DecodeRdnaSopp(pc, words, 0u);
}

static RdnaInstruction Sopk(std::uint32_t pc, std::uint32_t opcode, std::uint32_t sdst, std::uint32_t simm16) {
    const std::array<std::uint32_t, 1> words{0x80000000u | ((0x60u + opcode) << 23u) | (sdst << 16u) | (simm16 & 0xffffu)};
    return DecodeRdnaSopk(pc, words, 0u);
}

static RdnaProgram Program(std::initializer_list<RdnaInstruction> instructions) {
    RdnaProgram program;
    program.instructions.assign(instructions);
    return program;
}

static ControlFlowGraph Build(const RdnaProgram& program, const SwappcInfo* swappc = nullptr) {
    return GraphBuilder{}.Build(program, swappc);
}

static void CheckDecodeIdentity() {
    const std::array<std::uint32_t, 2> words{0x80000000u | (0x7du << 23u) | (8u << 16u) | (0x21u << 8u) | 4u, 0u};
    const RdnaInstruction instruction = DecodeRdnaSop1(0x24u, words, 0u);
    Require(instruction.op == RdnaOpcode::SSwappcB64);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(instruction.programCounter == 0x24u);
    Require(instruction.dataDwordCount == 2u);
    Require(instruction.sourceCount == 1u);
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister && instruction.source0.reg == 4u);
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister && instruction.destination.reg == 8u);

    const std::array<std::uint32_t, 2> pcWords{0x80000000u | (0x7du << 23u) | (125u << 16u) | (0x21u << 8u) | 4u, 0u};
    Require(DecodeRdnaSop1(0u, pcWords, 0u).op == RdnaOpcode::SSetpcB64);


    const RdnaInstruction add = Sop2Literal(0x4u, 4u, 0x00u, 4u, 0x18u);
    Require(add.op == RdnaOpcode::SAddU32);
    Require(add.wordCount == 2u);
}

static std::string TranslateDump(const RdnaInstruction& instruction) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder() = {&block};
    program.Metadata().blockInfo.resize(1);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    return ProgramToString(program);
}

static void CheckLinkModelsNextInstructionAddress() {
    const std::array<std::uint32_t, 2> getpcWords{0x80000000u | (0x7du << 23u) | (2u << 16u) | (0x1fu << 8u), 0u};
    const std::array<std::uint32_t, 2> swappcWords{0x80000000u | (0x7du << 23u) | (2u << 16u) | (0x21u << 8u) | 4u, 0u};
    const RdnaInstruction getpc = DecodeRdnaSop1(0x10u, getpcWords, 0u);
    const RdnaInstruction swappc = DecodeRdnaSop1(0x10u, swappcWords, 0u);
    Require(!TranslateDump(getpc).empty());
    Require(TranslateDump(getpc) == TranslateDump(swappc));
}

static void CheckStaticCallRegion() {
    const auto program = Program({
        Sop1(0x00u, 4u, 0x1fu, 0u),
        Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x18u),
        Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
        Sop1(0x14u, 8u, 0x21u, 4u),
        Sopp(0x18u, 0x01u),
        Sopp(0x1cu, 0x00u),
        Sop1(0x20u, 0u, 0x20u, 8u),
    });
    const auto cfg = Build(program);
    Require(cfg.FindBlockByProgramCounter(0x00u).terminator.kind == TerminatorKind::Branch);
    Require(cfg.FindBlockByProgramCounter(0x00u).terminator.trueBlock == cfg.FindBlockByProgramCounter(0x1cu).id);
    Require(cfg.FindBlockByProgramCounter(0x1cu).terminator.kind == TerminatorKind::Branch);
    Require(cfg.FindBlockByProgramCounter(0x1cu).terminator.trueBlock == cfg.FindBlockByProgramCounter(0x18u).id);
}

static void CheckFetchCallAtEntry() {
    SwappcInfo info;
    info.fetchCallAllowed = true;
    info.userDataBaseRegister = 16u;
    info.userDataCount = 8u;
    const auto program = Program({
        Sopp(0x00u, 0x00u),
        Sop1(0x04u, 2u, 0x21u, 16u),
        Sopp(0x08u, 0x01u),
    });
    const auto cfg = Build(program, &info);
    Require(cfg.hasFetchCall);
    Require(cfg.fetchCallProgramCounter == 0x04u);
}

static void CheckThrows() {
    ExpectThrow("computed/data-dependent s_swappc_b64", [] {
        Build(Program({
            Sop1(0x00u, 8u, 0x21u, 4u),
            Sopp(0x04u, 0x01u),
        }));
    });
    ExpectThrow("escapes the constant-offset call/return model", [] {
        Build(Program({
            Sop1(0x00u, 4u, 0x1fu, 0u),
            Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x18u),
            Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
            Sop1(0x14u, 8u, 0x21u, 4u),
            Sop1(0x18u, 8u, 0x03u, 8u),
            Sopp(0x1cu, 0x01u),
            Sopp(0x20u, 0x00u),
            Sop1(0x24u, 0u, 0x20u, 8u),
        }));
    });
    ExpectThrow("no paired s_setpc_b64 return", [] {
        Build(Program({
            Sop1(0x00u, 4u, 0x1fu, 0u),
            Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x18u),
            Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
            Sop1(0x14u, 8u, 0x21u, 4u),
            Sopp(0x18u, 0x01u),
            Sopp(0x1cu, 0x00u),
        }));
    });
    ExpectThrow("multiple s_setpc_b64 returns", [] {
        Build(Program({
            Sop1(0x00u, 4u, 0x1fu, 0u),
            Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x18u),
            Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
            Sop1(0x14u, 8u, 0x21u, 4u),
            Sopp(0x18u, 0x01u),
            Sopp(0x1cu, 0x00u),
            Sop1(0x20u, 0u, 0x20u, 8u),
            Sop1(0x24u, 0u, 0x20u, 8u),
        }));
    });
    ExpectThrow("is shared with another call", [] {
        Build(Program({
            Sop1(0x00u, 4u, 0x1fu, 0u),
            Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x18u),
            Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
            Sop1(0x14u, 8u, 0x21u, 4u),
            Sop1(0x18u, 8u, 0x21u, 4u),
            Sopp(0x1cu, 0x01u),
            Sopp(0x20u, 0x00u),
            Sop1(0x24u, 0u, 0x20u, 8u),
        }));
    });
    ExpectThrow("recursive scalar call", [] {
        Build(Program({
            Sop1(0x00u, 4u, 0x1fu, 0u),
            Sop2Literal(0x04u, 4u, 0x00u, 4u, 0x10u),
            Sop2Literal(0x0cu, 5u, 0x04u, 5u, 0u),
            Sop1(0x14u, 8u, 0x21u, 4u),
            Sopp(0x18u, 0x01u),
            Sop1(0x1cu, 0u, 0x20u, 8u),
        }));
    });
    ExpectThrow("computed/data-dependent s_swappc_b64", [] {
        SwappcInfo info;
        info.fetchCallAllowed = true;
        info.userDataBaseRegister = 16u;
        info.userDataCount = 8u;
        Build(Program({
            Sopp(0x00u, 0x00u),
            Sop1(0x04u, 2u, 0x21u, 4u),
            Sopp(0x08u, 0x01u),
        }), &info);
    });
    ExpectThrow("must live in the fetch-shader code", [] {
        SwappcInfo info;
        info.fetchCallAllowed = true;
        info.userDataBaseRegister = 16u;
        info.userDataCount = 8u;
        Build(Program({
            Sopp(0x00u, 0x00u),
            Sop1(0x04u, 2u, 0x21u, 16u),
            Sopp(0x08u, 0x01u),
            Sop1(0x0cu, 0u, 0x20u, 2u),
        }), &info);
    });
}

int main() {
    try {
        CheckDecodeIdentity();
        CheckLinkModelsNextInstructionAddress();
        CheckStaticCallRegion();
        CheckFetchCallAtEntry();
        CheckThrows();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "scalar swappc regression: %s\n", error.what());
        return 1;
    }
    return 0;
}
