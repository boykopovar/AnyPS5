#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("scalar scratch load regression"); }
static void Check(std::uint32_t opcode, RdnaOpcode expected, std::uint32_t dwords) {
    const std::array<std::uint32_t, 2> code{
        (0x3du << 26u) | (opcode << 18u) | (2u << 6u) | 1u,
        (125u << 25u) | 0x10u
    };
    const RdnaInstruction instruction = DecodeRdnaSmem(0u, code, 0u);
    Require(instruction.op == expected);
    Require(instruction.family == RdnaInstructionFamily::SMEM);
    Require(instruction.dataDwordCount == dwords);
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.destination.reg == 2u);
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.source0.reg == 2u);
    Require(instruction.memoryOffset == 0x10);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
int main() {
    Check(0x05u, RdnaOpcode::SScratchLoadDword, 1u);
    Check(0x06u, RdnaOpcode::SScratchLoadDwordx2, 2u);
    Check(0x07u, RdnaOpcode::SScratchLoadDwordx4, 4u);
}
