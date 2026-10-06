#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) throw std::runtime_error("scalar bfe64 regression");
}

static void Check(std::uint32_t encoding, RdnaOpcode opcode, bool sign) {
    const std::array<std::uint32_t, 1> code{0x80000000u | (encoding << 23u) | (2u << 16u) | (20u << 8u) | 10u};
    const RdnaInstruction instruction = DecodeRdnaSop2(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::SOP2);
    Require(instruction.dataDwordCount == 2u);
    Require(IsScalarAluOpcode(instruction.op));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    bool arithmetic = false;
    bool masked = false;
    for (auto* value : block.Instructions()) {
        arithmetic = arithmetic || value->Opcode() == IrOpcode::ShiftRightArithmetic64;
        masked = masked || value->Opcode() == IrOpcode::BitwiseAnd64;
    }
    Require(masked);
    Require(arithmetic == sign);
}

int main() {
    Check(0x2au, RdnaOpcode::SBfeI64, true);
    Check(0x29u, RdnaOpcode::SBfeU64, false);
}