#include "Translation/TranslationContext.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar call regression"); }

static void CheckDecodeAndBranch() {
    const std::array<std::uint32_t, 1> code{0xbb040008u};
    const RdnaInstruction instruction = DecodeRdnaSopk(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::SCallB64);
    Require(instruction.family == RdnaInstructionFamily::SOPK);
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.destination.reg == 4u);
    Require(instruction.dataDwordCount == 2u);
    Require(instruction.branchTarget == 36u);
}

static void CheckNegativeOffset() {
    const std::uint32_t word = 0xb8000000u | (0x16u << 23u) | (2u << 16u) | static_cast<std::uint16_t>(-4);
    const std::array<std::uint32_t, 1> code{word};
    const RdnaInstruction instruction = DecodeRdnaSopk(16u, code, 0u);
    Require(instruction.op == RdnaOpcode::SCallB64);
    Require(instruction.destination.reg == 2u);
    Require(instruction.branchTarget == 4u);
}

static void CheckTranslationThrows() {
    const std::array<std::uint32_t, 1> code{0xbb040008u};
    const RdnaInstruction instruction = DecodeRdnaSopk(0u, code, 0u);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

static void CheckProgramGraphBuilderAndTranslationThrows() {
    const std::vector<std::uint32_t> code{0xbb040001u, 0xbf810000u, 0xbf810000u};
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    const auto cfg = GraphBuilder{}.Build(decoded);
    ShaderComputeInputInfo computeInfo{};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.inputInfo.compute = &computeInfo;
    bool threw = false;
    try {
        static_cast<void>(InstructionTranslator{}.Translate(decoded, cfg, options));
    } catch (const std::runtime_error& err) {
        threw = std::string(err.what()).find("s_call_b64 at pc 0 is not implemented") != std::string::npos;
    }
    Require(threw);
}

int main() {
    CheckDecodeAndBranch();
    CheckNegativeOffset();
    CheckTranslationThrows();
    CheckProgramGraphBuilderAndTranslationThrows();
    return 0;
}
