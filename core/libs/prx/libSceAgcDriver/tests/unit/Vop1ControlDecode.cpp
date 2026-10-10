#include <Testing/Test.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <array>
#include <string>
#include <vector>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

const Case controlOpcodes{"Decode_Vop1AndVop3ControlOpcodes_DecodeWithoutOperands", [] {
    const std::vector<std::uint32_t> code{
        0x7e000000u, 0x7e003600u, 0x7e008200u,
        0xd5800000u, 0x00000000u, 0xd59b0000u, 0x00000000u, 0xd5c10000u, 0x00000000u,
        0xbf810000u,
    };
    constexpr std::array expected{
        RdnaOpcode::VNop, RdnaOpcode::VPipeflush, RdnaOpcode::VClrexcp,
        RdnaOpcode::VNop, RdnaOpcode::VPipeflush, RdnaOpcode::VClrexcp,
    };
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    RequireEqual(decoded.instructions.size(), expected.size() + 1u, "decoded instruction count");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto& instruction = decoded.instructions[index];
        const auto wordCount = index < 3u ? 1u : 2u;
        Require(instruction.op == expected[index] && instruction.wordCount == wordCount, "instruction " + std::to_string(index) + " decodes to the wrong opcode or length");
        Require(instruction.destination.kind == RdnaOperandKind::Null && instruction.sourceCount == 0u, "instruction " + std::to_string(index) + " decodes with operands");
    }
}};

} // namespace
