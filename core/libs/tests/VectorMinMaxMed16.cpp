#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector min/max/med regression"); }
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
    Check(0x352u, RdnaOpcode::VMin3I16, IrOpcode::SMinTri32);
    Check(0x353u, RdnaOpcode::VMin3U16, IrOpcode::UMinTri32);
    Check(0x355u, RdnaOpcode::VMax3I16, IrOpcode::SMaxTri32);
    Check(0x356u, RdnaOpcode::VMax3U16, IrOpcode::UMaxTri32);
    Check(0x359u, RdnaOpcode::VMed3U16, IrOpcode::UMedTri32);
}
