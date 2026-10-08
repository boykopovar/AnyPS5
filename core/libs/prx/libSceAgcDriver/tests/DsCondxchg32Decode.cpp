#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Expected {
    std::uint32_t destinationRegister;
    std::uint32_t addressRegister;
    std::uint32_t dataRegister;
    std::uint32_t memoryOffset;
    bool gds;
};

bool check(const RdnaInstruction& instruction, const Expected& expected, std::size_t index) {
    if (instruction.op != RdnaOpcode::DsCondxchg32RtnB64 || instruction.wordCount != 2u) {
        std::fprintf(stderr, "instruction %zu decodes to the wrong opcode or length\n", index);
        return false;
    }
    if (instruction.destination.kind != RdnaOperandKind::VectorRegister || instruction.destination.reg != expected.destinationRegister) {
        std::fprintf(stderr, "instruction %zu has a wrong destination\n", index);
        return false;
    }
    if (instruction.sourceCount != 2u || instruction.source0.reg != expected.addressRegister || instruction.source1.reg != expected.dataRegister) {
        std::fprintf(stderr, "instruction %zu has wrong operands\n", index);
        return false;
    }
    if (instruction.memoryOffset != expected.memoryOffset || instruction.gds != expected.gds) {
        std::fprintf(stderr, "instruction %zu has wrong offset or gds flag\n", index);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    // ds_condxchg32_rtn_b64 v[0:1], v2, v[4:5]
    // ds_condxchg32_rtn_b64 v[8:9], v10, v[12:13] offset:255
    // ds_condxchg32_rtn_b64 v[2:3], v4, v[6:7] gds
    const std::vector<std::uint32_t> code{
        0xD9F80000u, 0x00000402u,
        0xD9F800FFu, 0x08000C0Au,
        0xD9FA0000u, 0x02000604u,
        0xBF810000u,
    };
    constexpr std::array<Expected, 3> expected{
        Expected{0u, 2u, 4u, 0u, false},
        Expected{8u, 10u, 12u, 255u, false},
        Expected{2u, 4u, 6u, 0u, true},
    };
    try {
        const auto decoded = RdnaInstructionDecoder{}.Decode(code);
        if (decoded.instructions.size() != expected.size() + 1u) {
            std::fprintf(stderr, "decoded %zu instructions, expected %zu\n", decoded.instructions.size(), expected.size() + 1u);
            return 1;
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!check(decoded.instructions[index], expected[index], index)) {
                return 1;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
    }
    return 1;
}
