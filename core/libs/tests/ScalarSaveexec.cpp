#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar saveexec regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode) {
    const std::array<std::uint32_t, 1> code{0x80000000u | (106u << 16u) | (encoding << 8u) | 10u};
    const RdnaInstruction instruction = DecodeRdnaSop1(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(IsScalarAluOpcode(instruction.op));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}
int main() {
    Check(0x3du, RdnaOpcode::SOrSaveexecB32);
    Check(0x3eu, RdnaOpcode::SXorSaveexecB32);
    Check(0x3fu, RdnaOpcode::SAndn2SaveexecB32);
    Check(0x41u, RdnaOpcode::SNandSaveexecB32);
    Check(0x42u, RdnaOpcode::SNorSaveexecB32);
    Check(0x43u, RdnaOpcode::SXnorSaveexecB32);
    Check(0x45u, RdnaOpcode::SOrn1SaveexecB32);
    Check(0x46u, RdnaOpcode::SAndn1WrexecB32);
    Check(0x47u, RdnaOpcode::SAndn2WrexecB32);
    Check(0x29u, RdnaOpcode::SNandSaveexecB64);
    Check(0x2au, RdnaOpcode::SNorSaveexecB64);
    Check(0x2bu, RdnaOpcode::SXnorSaveexecB64);
    Check(0x38u, RdnaOpcode::SOrn1SaveexecB64);
    Check(0x39u, RdnaOpcode::SAndn1WrexecB64);
    Check(0x3au, RdnaOpcode::SAndn2WrexecB64);
}
