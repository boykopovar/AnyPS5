#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>

using namespace ShaderRecompiler;

namespace {

struct ShiftAddOpcode {
    std::uint32_t word0;
    std::uint32_t word1;
    RdnaOpcode op;
    const char* name;
};

constexpr std::array<ShiftAddOpcode, 2> Opcodes{{
    {0xd746000au, 0x041a0b04u, RdnaOpcode::VLshlAddU32, "v_lshl_add_u32"},
    {0xd7470012u, 0x041a0b04u, RdnaOpcode::VAddLshlU32, "v_add_lshl_u32"},
}};

bool ExpectRefused(std::uint32_t word0, std::uint32_t word1, const char* label) {
    const std::array<std::uint32_t, 3> code{word0, word1, 0xbf810000u};
    std::string message;
    try {
        static_cast<void>(RdnaInstructionDecoder{}.Decode(code));
    } catch (const std::exception& error) {
        message = error.what();
    }
    if (message.find("VOP3 source modifiers are not implemented") == std::string::npos) {
        std::fprintf(stderr, "%s was not refused with VOP3 source modifiers diagnostic (got: %s)\n", label, message.c_str());
        return false;
    }
    return true;
}

}

int main() {
    int failures = 0;
    for (const auto& entry : Opcodes) {
        for (std::uint32_t absBits = 0; absBits < 8u; ++absBits) {
            for (std::uint32_t negBits = 0; negBits < 8u; ++negBits) {
                const std::uint32_t w0 = entry.word0 | (absBits << 8u);
                const std::uint32_t w1 = entry.word1 | (negBits << 29u);
                const std::array<std::uint32_t, 3> code{w0, w1, 0xbf810000u};
                try {
                    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
                    if (decoded.instructions.empty()) {
                        std::fprintf(stderr, "%s decoded no instructions\n", entry.name);
                        ++failures;
                        continue;
                    }
                    const auto& inst = decoded.instructions[0];
                    const bool absOk = inst.source0.absolute == ((absBits & 1u) != 0u) &&
                                       inst.source1.absolute == ((absBits & 2u) != 0u) &&
                                       inst.source2.absolute == ((absBits & 4u) != 0u);
                    const bool negOk = inst.source0.negate == ((negBits & 1u) != 0u) &&
                                       inst.source1.negate == ((negBits & 2u) != 0u) &&
                                       inst.source2.negate == ((negBits & 4u) != 0u);
                    if (inst.op != entry.op || inst.wordCount != 2u || !absOk || !negOk || inst.destination.clamp || inst.destination.omod != 0u) {
                        std::fprintf(stderr, "%s abs=%u neg=%u decoded unexpected fields\n", entry.name, absBits, negBits);
                        ++failures;
                    }
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "%s abs=%u neg=%u refused: %s\n", entry.name, absBits, negBits, error.what());
                    ++failures;
                }
            }
        }

        if (!ExpectRefused(entry.word0 | (1u << 15u), entry.word1, "clamp")) ++failures;
        if (!ExpectRefused(entry.word0 | (1u << 15u) | (1u << 8u), entry.word1 | (1u << 29u), "clamp+abs+neg")) ++failures;

        for (std::uint32_t omod = 1; omod < 4u; ++omod) {
            if (!ExpectRefused(entry.word0, entry.word1 | (omod << 27u), "omod")) ++failures;
            if (!ExpectRefused(entry.word0 | (2u << 8u), entry.word1 | (omod << 27u) | (4u << 29u), "omod+abs+neg")) ++failures;
        }

        for (std::uint32_t opSel = 1; opSel < 16u; ++opSel) {
            if (!ExpectRefused(entry.word0 | (opSel << 11u), entry.word1, "op_sel")) ++failures;
            if (!ExpectRefused(entry.word0 | (opSel << 11u) | (1u << 8u), entry.word1 | (1u << 30u), "op_sel+abs+neg")) ++failures;
        }

        if (!ExpectRefused(entry.word0 | (1u << 15u) | (8u << 11u), entry.word1 | (1u << 27u), "clamp+op_sel+omod")) ++failures;
    }
    if (failures == 0) {
        std::puts("vop3 shift add decode tests passed");
        return 0;
    }
    return 1;
}
