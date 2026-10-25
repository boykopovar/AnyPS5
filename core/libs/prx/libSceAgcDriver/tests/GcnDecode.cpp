#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>

using namespace ShaderRecompiler;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    ++failures;
}

RdnaInstruction Decode(std::span<const std::uint32_t> words) {
    return DecodeGcnInstruction(0u, words, 0u);
}

void SetregImmKeepsItsLiteral() {
    const std::array<std::uint32_t, 3> setreg{0xba80f801u, 0x12345678u, 0xbf810000u};
    const auto instruction = Decode(setreg);
    Check(instruction.op == RdnaOpcode::SSetregImm32B32, "s_setreg_imm32_b32 did not decode as its RDNA opcode");
    Check(instruction.wordCount == 2u, "a GCN s_setreg_imm32_b32 is not two words");
    Check(instruction.source0.value == 0x12345678u, "s_setreg_imm32_b32 lost its literal");
    const std::array<std::uint32_t, 2> movk{0xb0001234u, 0xbf810000u};
    Check(Decode(movk).wordCount == 1u, "s_movk_i32 took a literal word");
}

}

int main() {
    try {
        SetregImmKeepsItsLiteral();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures == 0) std::puts("GCN decode tests passed");
    return failures == 0 ? 0 : 1;
}
