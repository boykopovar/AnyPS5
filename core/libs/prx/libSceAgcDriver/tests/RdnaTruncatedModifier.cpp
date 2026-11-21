#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

int main() {
    struct Case {
        const char* name;
        std::uint32_t word;
    };
    const std::array<Case, 6> cases{{
        {"VOP1 SDWA", 0x7e0202f9u},
        {"VOP1 DPP", 0x7e0202fau},
        {"VOP2 SDWA", 0x4a0202f9u},
        {"VOP2 DPP", 0x4a0202fau},
        {"VOPC SDWA", 0x7c0202f9u},
        {"VOPC DPP", 0x7c0202fau},
    }};
    int failures = 0;
    for (const auto& entry : cases) {
        const std::array<std::uint32_t, 2> words{entry.word, 0xaf00e400u};
        const std::string expected = std::string("truncated ") + entry.name + " instruction";
        try {
            static_cast<void>(RdnaInstructionDecoder{}.Decode(std::span(words).first(1)));
            std::fprintf(stderr, "%s: a missing modifier word decodes\n", entry.name);
            ++failures;
        } catch (const std::out_of_range& error) {
            if (expected != error.what()) {
                std::fprintf(stderr, "%s: %s\n", entry.name, error.what());
                ++failures;
            }
        }
    }
    struct SdwaSource {
        const char* name;
        std::uint32_t word;
        std::uint32_t modifier;
        bool secondSource;
    };
    const std::array<SdwaSource, 5> literalSources{{
        {"VOP1 SDWA src0", 0x7e0602f9u, 0x008606ffu, false},
        {"VOP2 SDWA src0", 0x4a0202f9u, 0x068606ffu, false},
        {"VOP2 SDWA src1", 0x4a03fef9u, 0x86060604u, true},
        {"VOPC SDWA src0", 0x7c0202f9u, 0x068600ffu, false},
        {"VOPC SDWA src1", 0x7c03fef9u, 0x86060004u, true},
    }};
    for (const auto& entry : literalSources) {
        const std::array<std::uint32_t, 3> words{entry.word, entry.modifier, 0xbf810000u};
        try {
            const RdnaInstruction instruction = DecodeRdnaInstruction(0u, words, 0u);
            std::fprintf(stderr, "%s: scalar code 255 decodes as a %u-word instruction\n", entry.name, instruction.wordCount);
            ++failures;
        } catch (const std::invalid_argument&) {
        }
    }
    const std::array<SdwaSource, 5> scalarSources{{
        {"VOP1 SDWA src0", 0x7e0602f9u, 0x00860605u, false},
        {"VOP2 SDWA src0", 0x4a0202f9u, 0x06860605u, false},
        {"VOP2 SDWA src1", 0x4a020af9u, 0x86060604u, true},
        {"VOPC SDWA src0", 0x7c0202f9u, 0x06860005u, false},
        {"VOPC SDWA src1", 0x7c020af9u, 0x86060004u, true},
    }};
    for (const auto& entry : scalarSources) {
        const std::array<std::uint32_t, 3> words{entry.word, entry.modifier, 0xbf810000u};
        const RdnaInstruction instruction = DecodeRdnaInstruction(0u, words, 0u);
        const RdnaOperand& source = entry.secondSource ? instruction.source1 : instruction.source0;
        if (instruction.wordCount != 2u || source.kind != RdnaOperandKind::ScalarRegister || source.reg != 5u) {
            std::fprintf(stderr, "%s: s5 decodes with the wrong operands\n", entry.name);
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
