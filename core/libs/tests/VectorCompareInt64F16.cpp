#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector compare int64/f16 regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, bool exec) {
    const std::array<std::uint32_t, 1> code{(encoding << 17u) | (1u << 9u) | 100u};
    const RdnaInstruction instruction = DecodeRdnaVopc(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::VOPC);
    Require(IsVectorAluOpcode(instruction.op));
    Require(instruction.destination.kind == (exec ? RdnaOperandKind::ExecLo : RdnaOperandKind::VccLo));
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}
int main() {
    Check(0xa1u, RdnaOpcode::VCmpLtI64, false);
    Check(0xa3u, RdnaOpcode::VCmpLeI64, false);
    Check(0xa4u, RdnaOpcode::VCmpGtI64, false);
    Check(0xa6u, RdnaOpcode::VCmpGeI64, false);
    Check(0xe3u, RdnaOpcode::VCmpLeU64, false);
    Check(0xe6u, RdnaOpcode::VCmpGeU64, false);
    Check(0xb1u, RdnaOpcode::VCmpxLtI64, true);
    Check(0xb2u, RdnaOpcode::VCmpxEqI64, true);
    Check(0xb3u, RdnaOpcode::VCmpxLeI64, true);
    Check(0xb4u, RdnaOpcode::VCmpxGtI64, true);
    Check(0xb6u, RdnaOpcode::VCmpxGeI64, true);
    Check(0xf1u, RdnaOpcode::VCmpxLtU64, true);
    Check(0xf2u, RdnaOpcode::VCmpxEqU64, true);
    Check(0xf3u, RdnaOpcode::VCmpxLeU64, true);
    Check(0xf4u, RdnaOpcode::VCmpxGtU64, true);
    Check(0xf6u, RdnaOpcode::VCmpxGeU64, true);
    Check(0xe9u, RdnaOpcode::VCmpNgeF16, false);
    Check(0xeeu, RdnaOpcode::VCmpNltF16, false);
    Check(0xecu, RdnaOpcode::VCmpNleF16, false);
    Check(0xeau, RdnaOpcode::VCmpNlgF16, false);
    Check(0x9fu, RdnaOpcode::VCmpxClassF16, true);
    Check(0xddu, RdnaOpcode::VCmpxLgF16, true);
    Check(0xf9u, RdnaOpcode::VCmpxNgeF16, true);
    Check(0xfeu, RdnaOpcode::VCmpxNltF16, true);
    Check(0xfcu, RdnaOpcode::VCmpxNleF16, true);
    Check(0xfau, RdnaOpcode::VCmpxNlgF16, true);
}
