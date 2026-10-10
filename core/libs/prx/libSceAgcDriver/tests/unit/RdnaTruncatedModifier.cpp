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
    return failures == 0 ? 0 : 1;
}
