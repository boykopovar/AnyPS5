#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <stdexcept>
#include <vector>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("image pck variants regression"); }
static std::uint32_t Word0(std::uint32_t opcode, std::uint32_t dmask, std::uint32_t dimension) {
    return (0x3cu << 26u) | ((opcode & 0x7fu) << 18u) | ((opcode >> 7u) & 1u) | (dmask << 8u) | (dimension << 3u);
}
static RdnaInstruction Decode(std::uint32_t word0, std::uint32_t word1) {
    const std::array<std::uint32_t, 2> code{word0, word1};
    return DecodeRdnaMimg(0u, code, 0u);
}
static bool Rejects(std::uint32_t word0, std::uint32_t word1) {
    try {
        (void)Decode(word0, word1);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
static void Check(RdnaOpcode op, std::uint32_t opcode, std::uint32_t dmask, std::uint32_t dwords, bool mip, bool store) {
    const RdnaInstruction instruction = Decode(Word0(opcode, dmask, 1u), (8u << 8u));
    Require(instruction.op == op);
    Require(instruction.family == RdnaInstructionFamily::MIMG);
    Require(IsImageOpcode(instruction.op));
    Require(instruction.imageOpcodeId == opcode);
    Require(instruction.imageAddressComponents == (mip ? 3u : 2u));
    Require(instruction.dataComponents == dwords);
    Require(instruction.dataDwordCount == dwords);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    std::uint32_t accesses = 0u;
    for (auto* value : block.Instructions()) {
        const auto irOpcode = value->Opcode();
        if (irOpcode != (store ? IrOpcode::ImageWrite : IrOpcode::ImageRead)) {
            continue;
        }
        ++accesses;
        const auto& memory = program.Resources().memoryInfo[value->Flags<MemoryFlags>().index];
        Require(memory.imagePacked && memory.imageHasMip == mip);
        Require(memory.dataDwords == dwords && memory.dmask == dmask);
    }
    Require(accesses == 1u);
}
int main() {
    Check(RdnaOpcode::ImageLoadPck2, 112u, 3u, 2u, false, false);
    Check(RdnaOpcode::ImageLoadPck4, 113u, 15u, 4u, false, false);
    Check(RdnaOpcode::ImageLoadMipPck2, 115u, 3u, 2u, true, false);
    Check(RdnaOpcode::ImageLoadMipPck4, 116u, 15u, 4u, true, false);
    Check(RdnaOpcode::ImageStorePck2, 118u, 3u, 2u, false, true);
    Check(RdnaOpcode::ImageStorePck4, 119u, 15u, 4u, false, true);
    Check(RdnaOpcode::ImageStoreMipPck2, 121u, 3u, 2u, true, true);
    Check(RdnaOpcode::ImageStoreMipPck4, 122u, 15u, 4u, true, true);
    for (const auto opcode : {112u, 115u, 118u, 121u}) {
        Require(Rejects(Word0(opcode, 1u, 1u), (8u << 8u)));
        Require(Rejects(Word0(opcode, 15u, 1u), (8u << 8u)));
    }
    for (const auto opcode : {113u, 116u, 119u, 122u}) {
        Require(Rejects(Word0(opcode, 3u, 1u), (8u << 8u)));
    }
    for (const auto opcode : {112u, 113u, 115u, 116u, 121u, 122u}) {
        Require(Rejects(Word0(opcode, opcode & 1u ? 15u : 3u, 6u), (8u << 8u)));
        Require(Rejects(Word0(opcode, opcode & 1u ? 15u : 3u, 1u), 0x80000000u | (8u << 8u)));
    }
    for (const auto opcode : {118u, 119u}) {
        Require(Rejects(Word0(opcode, opcode & 1u ? 15u : 3u, 1u), 0x80000000u | (8u << 8u)));
    }
}