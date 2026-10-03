#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("image atomic regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, IrOpcode expected, std::uint32_t dmask = 1u) {
    const std::array<std::uint32_t, 2> code{(0x3cu << 26u) | (encoding << 18u) | (dmask << 8u) | (1u << 3u), 0u};
    const RdnaInstruction instruction = DecodeRdnaMimg(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.family == RdnaInstructionFamily::MIMG);
    Require(IsImageOpcode(instruction.op));
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
    Check(0x10u, RdnaOpcode::ImageAtomicCmpswap, IrOpcode::ImageAtomicCmpSwap32, 3u);
    Check(0x1cu, RdnaOpcode::ImageAtomicDec, IrOpcode::ImageAtomicDec32);
    Check(0x1bu, RdnaOpcode::ImageAtomicInc, IrOpcode::ImageAtomicInc32);
    Check(0x16u, RdnaOpcode::ImageAtomicSmax, IrOpcode::ImageAtomicSMax32);
    Check(0x14u, RdnaOpcode::ImageAtomicSmin, IrOpcode::ImageAtomicSMin32);
    Check(0x12u, RdnaOpcode::ImageAtomicSub, IrOpcode::ImageAtomicISub32);
}
