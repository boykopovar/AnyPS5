#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector compare regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode) {
    const std::array<std::uint32_t, 1> code{(encoding << 17u) | (1u << 9u) | 100u};
    const RdnaInstruction instruction = DecodeRdnaVopc(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::VOPC);
    Require(IsVectorAluOpcode(instruction.op));
    Require(instruction.destination.kind == RdnaOperandKind::ExecLo);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}
int main() {
    Check(0x99u, RdnaOpcode::VCmpxLtI16);
    Check(0x9au, RdnaOpcode::VCmpxEqI16);
    Check(0x9bu, RdnaOpcode::VCmpxLeI16);
    Check(0x9cu, RdnaOpcode::VCmpxGtI16);
    Check(0x9du, RdnaOpcode::VCmpxNeI16);
    Check(0x9eu, RdnaOpcode::VCmpxGeI16);
    Check(0xbau, RdnaOpcode::VCmpxEqU16);
    Check(0xbbu, RdnaOpcode::VCmpxLeU16);
    Check(0xbdu, RdnaOpcode::VCmpxNeU16);
    Check(0xbeu, RdnaOpcode::VCmpxGeU16);
}
