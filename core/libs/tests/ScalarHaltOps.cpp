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
static void Require(bool value) { if (!value) throw std::runtime_error("scalar halt ops regression"); }

static void CheckRfeDecode() {
    const std::array<std::uint32_t, 1> code{0xBEFD220Au};
    const RdnaInstruction instruction = DecodeRdnaSop1(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::SRfeB64);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(instruction.destination.kind == RdnaOperandKind::Null);
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.source0.reg == 10u);
    Require(instruction.sourceCount == 1u);
    Require(instruction.dataDwordCount == 1u);
}

static void CheckSethaltDecode() {
    const std::array<std::uint32_t, 1> code{0xBF8D0001u};
    const RdnaInstruction instruction = DecodeRdnaSopp(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::SSethalt);
    Require(instruction.family == RdnaInstructionFamily::SOPP);
    Require(instruction.source0.kind == RdnaOperandKind::LiteralConstant);
    Require(instruction.source0.value == 1u);
    Require(instruction.sourceCount == 0u);
}

static void CheckSendmsghaltDecode() {
    const std::array<std::uint32_t, 1> code{0xBF910002u};
    const RdnaInstruction instruction = DecodeRdnaSopp(4u, code, 0u);
    Require(instruction.op == RdnaOpcode::SSendmsghalt);
    Require(instruction.source0.value == 2u);
}

static void CheckCodeEndDecode() {
    const std::array<std::uint32_t, 1> code{0xBF9F0000u};
    const RdnaInstruction instruction = DecodeRdnaSopp(8u, code, 0u);
    Require(instruction.op == RdnaOpcode::SCodeEnd);
}

static void TranslateOne(std::uint32_t word) {
    const std::array<std::uint32_t, 1> code{word};
    const RdnaInstruction instruction = DecodeRdnaInstruction(0u, code, 0u);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}

static bool ThrowsWith(std::uint32_t word, const char* name) {
    try {
        TranslateOne(word);
    } catch (const std::runtime_error& err) {
        return std::string(err.what()).find(name) != std::string::npos;
    }
    return false;
}

static void CheckTranslationBehavior() {
    Require(ThrowsWith(0xBF8D0001u, "s_sethalt at pc 0 with immediate 1 is not implemented"));
    Require(ThrowsWith(0xBF910002u, "s_sendmsghalt at pc 0 is not implemented"));
    Require(ThrowsWith(0xBF9F0000u, "s_code_end at pc 0 is not implemented"));
    TranslateOne(0xBEFD220Au);
    TranslateOne(0xBF8D0000u);
}

static void CheckProgramTranslationThrows(const std::vector<std::uint32_t>& code, const char* name) {
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
        threw = std::string(err.what()).find(name) != std::string::npos;
    }
    Require(threw);
}

static void CheckRfeControlFlow() {
    const std::vector<std::uint32_t> resolved{0xBE801F00u, 0x80009000u, 0x82018001u, 0xBE802200u, 0xBF810000u, 0x7E1402FFu, 0x00001234u, 0xDC708000u, 0x00060A02u};
    const auto decoded = RdnaInstructionDecoder{}.Decode(resolved);
    static_cast<void>(GraphBuilder{}.Build(decoded));

    const std::vector<std::uint32_t> shortForm{0xBE801F00u, 0x80008C00u, 0xBE802200u, 0xBF810000u, 0x7E1402FFu, 0x00001234u, 0xDC708000u, 0x00060A02u};
    const auto decodedShort = RdnaInstructionDecoder{}.Decode(shortForm);
    static_cast<void>(GraphBuilder{}.Build(decodedShort));

    const std::vector<std::uint32_t> dynamic{0xBE8003FFu, 0x00001000u, 0xBE810380u, 0xBE802200u, 0xBF810000u};
    const auto decodedDynamic = RdnaInstructionDecoder{}.Decode(dynamic);
    bool threw = false;
    try {
        static_cast<void>(GraphBuilder{}.Build(decodedDynamic));
    } catch (const std::invalid_argument& err) {
        threw = std::string(err.what()).find("unsupported dynamic s_rfe_b64") != std::string::npos;
    }
    Require(threw);
}

int main() {
    CheckRfeDecode();
    CheckSethaltDecode();
    CheckSendmsghaltDecode();
    CheckCodeEndDecode();
    CheckTranslationBehavior();
    CheckProgramTranslationThrows({0xBF8D0001u, 0xBF810000u}, "s_sethalt at pc 0 with immediate 1 is not implemented");
    CheckRfeControlFlow();
    return 0;
}
