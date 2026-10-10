#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector legacy mad regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, IrOpcode expected) {
    const std::array<std::uint32_t, 2> code{(encoding << 16u) | 1u, 1u | (2u << 9u) | (3u << 18u)};
    const RdnaInstruction instruction = DecodeRdnaVop3(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::VOP3);
    Require(IsVectorAluOpcode(instruction.op));
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
    Check(0x140u, RdnaOpcode::VMadLegacyF32, IrOpcode::FPMul32);
    Check(0x150u, RdnaOpcode::VMullitF32, IrOpcode::FPMul32);
}
