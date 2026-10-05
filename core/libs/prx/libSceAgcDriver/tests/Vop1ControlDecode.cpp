#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    const std::vector<std::uint32_t> code{
        0x7e000000u, 0x7e003600u, 0x7e008200u,
        0xd5800000u, 0x00000000u, 0xd59b0000u, 0x00000000u, 0xd5c10000u, 0x00000000u,
        0xbf810000u,
    };
    constexpr std::array expected{
        RdnaOpcode::VNop, RdnaOpcode::VPipeflush, RdnaOpcode::VClrexcp,
        RdnaOpcode::VNop, RdnaOpcode::VPipeflush, RdnaOpcode::VClrexcp,
    };
    try {
        const auto decoded = RdnaInstructionDecoder{}.Decode(code);
        if (decoded.instructions.size() != expected.size() + 1u) {
            std::fprintf(stderr, "decoded %zu instructions, expected %zu\n", decoded.instructions.size(), expected.size() + 1u);
            return 1;
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            const auto& instruction = decoded.instructions[index];
            const auto wordCount = index < 3u ? 1u : 2u;
            if (instruction.op != expected[index] || instruction.wordCount != wordCount) {
                std::fprintf(stderr, "instruction %zu decodes to the wrong opcode or length\n", index);
                return 1;
            }
            if (instruction.destination.kind != RdnaOperandKind::Null || instruction.sourceCount != 0u) {
                std::fprintf(stderr, "instruction %zu decodes with operands\n", index);
                return 1;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
    }
    return 1;
}
