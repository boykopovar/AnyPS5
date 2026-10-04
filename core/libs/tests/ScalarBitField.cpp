#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) { if (!value) throw std::runtime_error("scalar bit field regression"); }

int main() {
    const std::array<std::uint32_t, 1> code{0x80000000u | (0x2au << 23u) | (106u << 16u) | (12u << 8u) | 10u};
    const RdnaInstruction instruction = DecodeRdnaSop2(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::SBfeI64);
    Require(instruction.family == RdnaInstructionFamily::SOP2);
    Require(instruction.dataDwordCount == 2u);
    Require(IsScalarAluOpcode(instruction.op));

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    bool hasArithmeticShift = false;
    for (const IrValue* value : block.Instructions()) {
        hasArithmeticShift |= value->Opcode() == IrOpcode::ShiftRightArithmetic64;
    }
    Require(hasArithmeticShift);
}
