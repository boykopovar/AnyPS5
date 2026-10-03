#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar misc regression"); }
static void CheckSop1(std::uint32_t encoding, RdnaOpcode opcode, IrOpcode expected) {
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
    bool found = false;
    for (auto* value : block.Instructions()) found = found || value->Opcode() == expected;
    Require(found);
}
static void CheckSopk(std::uint32_t encoding, RdnaOpcode opcode) {
    const std::array<std::uint32_t, 1> code{0xb0000000u | (encoding << 23u) | 0x7fu};
    const RdnaInstruction instruction = DecodeRdnaSopk(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::SOPK);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
}
int main() {
    CheckSop1(0x17u, RdnaOpcode::SFlbitI32, IrOpcode::FindUMsb32);
    CheckSop1(0x2cu, RdnaOpcode::SQuadmaskB32, IrOpcode::BitwiseOr32);
    CheckSopk(0x18u, RdnaOpcode::SWaitcntVmcnt);
    CheckSopk(0x19u, RdnaOpcode::SWaitcntExpcnt);
    CheckSopk(0x1au, RdnaOpcode::SWaitcntLgkmcnt);
}
