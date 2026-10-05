#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar bit-count regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, IrOpcode expected) {
    const std::array<std::uint32_t, 1> code{0x80000000u | (10u << 16u) | (encoding << 8u) | 12u};
    const RdnaInstruction instruction = DecodeRdnaSop1(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(IsScalarAluOpcode(instruction.op));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    bool found = false;
    for (auto* value : block.Instructions()) found = found || value->Opcode() == expected;
    Require(found);
}
int main() {
    Check(0x0eu, RdnaOpcode::SBcnt0I32B64, IrOpcode::BitCount32);
    Check(0x12u, RdnaOpcode::SFf0I32B64, IrOpcode::FindILsb32);
}
