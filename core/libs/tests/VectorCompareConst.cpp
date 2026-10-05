#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("vector compare const regression"); }
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
    Check(0xa0u, RdnaOpcode::VCmpFI64, false);
    Check(0xa7u, RdnaOpcode::VCmpTI64, false);
    Check(0xe0u, RdnaOpcode::VCmpFU64, false);
    Check(0xe7u, RdnaOpcode::VCmpTU64, false);
    Check(0xc8u, RdnaOpcode::VCmpFF16, false);
    Check(0xefu, RdnaOpcode::VCmpTruF16, false);
    Check(0xcfu, RdnaOpcode::VCmpOF16, false);
    Check(0xe8u, RdnaOpcode::VCmpUF16, false);
    Check(0x10u, RdnaOpcode::VCmpxFF32, true);
    Check(0x17u, RdnaOpcode::VCmpxOF32, true);
    Check(0x18u, RdnaOpcode::VCmpxUF32, true);
    Check(0x1fu, RdnaOpcode::VCmpxTruF32, true);
    Check(0x90u, RdnaOpcode::VCmpxFI32, true);
    Check(0x97u, RdnaOpcode::VCmpxTI32, true);
    Check(0xd0u, RdnaOpcode::VCmpxFU32, true);
    Check(0xd7u, RdnaOpcode::VCmpxTU32, true);
    Check(0xb0u, RdnaOpcode::VCmpxFI64, true);
    Check(0xb7u, RdnaOpcode::VCmpxTI64, true);
    Check(0xf0u, RdnaOpcode::VCmpxFU64, true);
    Check(0xf7u, RdnaOpcode::VCmpxTU64, true);
    Check(0xd8u, RdnaOpcode::VCmpxFF16, true);
    Check(0xffu, RdnaOpcode::VCmpxTruF16, true);
    Check(0xdfu, RdnaOpcode::VCmpxOF16, true);
    Check(0xf8u, RdnaOpcode::VCmpxUF16, true);
}
