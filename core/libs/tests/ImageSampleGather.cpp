#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("image sample/gather regression"); }
static void Check(std::uint32_t encoding, RdnaOpcode opcode, std::uint32_t dmask) {
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
}
static void CheckPck(std::uint32_t encoding, RdnaOpcode opcode, std::uint32_t dmask, std::uint32_t dataComponents) {
    const std::array<std::uint32_t, 2> code{(0x3cu << 26u) | (encoding << 18u) | (dmask << 8u) | (1u << 3u), 0u};
    const RdnaInstruction instruction = DecodeRdnaMimg(0u, code, 0u);
    Require(instruction.op == opcode);
    Require(instruction.imageOpcodeId == encoding);
    Require(instruction.imageDmask == dmask);
    Require(instruction.dataComponents == dataComponents);
    Require(instruction.dataDwordCount == dataComponents);
    Require(IsImageOpcode(instruction.op));
}
static void CheckPckRejectsZeroMask(std::uint32_t encoding) {
    const std::array<std::uint32_t, 2> code{(0x3cu << 26u) | (encoding << 18u) | (1u << 3u), 0u};
    bool rejected = false;
    try {
        DecodeRdnaMimg(0u, code, 0u);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
}
int main() {
    Check(0x26u, RdnaOpcode::ImageSampleBCl, 0xfu);
    Check(0x36u, RdnaOpcode::ImageSampleBClO, 0xfu);
    Check(0x35u, RdnaOpcode::ImageSampleBO, 0xfu);
    Check(0x28u, RdnaOpcode::ImageSampleC, 0xfu);
    Check(0x2du, RdnaOpcode::ImageSampleCB, 0xfu);
    Check(0x2eu, RdnaOpcode::ImageSampleCBCl, 0xfu);
    Check(0x3eu, RdnaOpcode::ImageSampleCBClO, 0xfu);
    Check(0x3du, RdnaOpcode::ImageSampleCBO, 0xfu);
    Check(0x29u, RdnaOpcode::ImageSampleCCl, 0xfu);
    Check(0x39u, RdnaOpcode::ImageSampleCClO, 0xfu);
    Check(0x2au, RdnaOpcode::ImageSampleCD, 0xfu);
    Check(0x2bu, RdnaOpcode::ImageSampleCDCl, 0xfu);
    Check(0x3bu, RdnaOpcode::ImageSampleCDClO, 0xfu);
    Check(0x3au, RdnaOpcode::ImageSampleCDO, 0xfu);
    Check(0x2cu, RdnaOpcode::ImageSampleCL, 0xfu);
    Check(0x3fu, RdnaOpcode::ImageSampleCLzO, 0xfu);
    Check(0x3cu, RdnaOpcode::ImageSampleCLO, 0xfu);
    Check(0x38u, RdnaOpcode::ImageSampleCO, 0xfu);
    Check(0x21u, RdnaOpcode::ImageSampleCl, 0xfu);
    Check(0x31u, RdnaOpcode::ImageSampleClO, 0xfu);
    Check(0x22u, RdnaOpcode::ImageSampleD, 0xfu);
    Check(0x23u, RdnaOpcode::ImageSampleDCl, 0xfu);
    Check(0x32u, RdnaOpcode::ImageSampleDO, 0xfu);
    Check(0x37u, RdnaOpcode::ImageSampleLzO, 0xfu);
    Check(0x30u, RdnaOpcode::ImageSampleO, 0xfu);
    Check(0x40u, RdnaOpcode::ImageGather4, 0x8u);
    Check(0x45u, RdnaOpcode::ImageGather4B, 0x8u);
    Check(0x46u, RdnaOpcode::ImageGather4BCl, 0x8u);
    Check(0x56u, RdnaOpcode::ImageGather4BClO, 0x8u);
    Check(0x55u, RdnaOpcode::ImageGather4BO, 0x8u);
    Check(0x41u, RdnaOpcode::ImageGather4Cl, 0x8u);
    Check(0x51u, RdnaOpcode::ImageGather4ClO, 0x8u);
    Check(0x4du, RdnaOpcode::ImageGather4CB, 0x8u);
    Check(0x4eu, RdnaOpcode::ImageGather4CBCl, 0x8u);
    Check(0x5eu, RdnaOpcode::ImageGather4CBClO, 0x8u);
    Check(0x5du, RdnaOpcode::ImageGather4CBO, 0x8u);
    Check(0x49u, RdnaOpcode::ImageGather4CCl, 0x8u);
    Check(0x59u, RdnaOpcode::ImageGather4CClO, 0x8u);
    Check(0x4cu, RdnaOpcode::ImageGather4CL, 0x8u);
    Check(0x5cu, RdnaOpcode::ImageGather4CLO, 0x8u);
    Check(0x44u, RdnaOpcode::ImageGather4L, 0x8u);
    Check(0x54u, RdnaOpcode::ImageGather4LO, 0x8u);
    Check(0x50u, RdnaOpcode::ImageGather4O, 0x8u);
    CheckPck(0x62u, RdnaOpcode::ImageGather4hPck, 0x1u, 1u);
    CheckPck(0x62u, RdnaOpcode::ImageGather4hPck, 0x3u, 2u);
    CheckPck(0x62u, RdnaOpcode::ImageGather4hPck, 0xFu, 4u);
    CheckPck(0x63u, RdnaOpcode::ImageGather8hPck, 0x1u, 1u);
    CheckPck(0x63u, RdnaOpcode::ImageGather8hPck, 0xFu, 4u);
    CheckPckRejectsZeroMask(0x62u);
    CheckPckRejectsZeroMask(0x63u);
}
