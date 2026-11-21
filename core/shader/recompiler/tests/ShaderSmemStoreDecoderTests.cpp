#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {
struct Case {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    std::uint32_t width;
};

constexpr Case cases[] = {
    {0x10u, RdnaOpcode::SStoreDword, 1u},
    {0x11u, RdnaOpcode::SStoreDwordx2, 2u},
    {0x12u, RdnaOpcode::SStoreDwordx4, 4u},
    {0x15u, RdnaOpcode::SScratchStoreDword, 1u},
    {0x16u, RdnaOpcode::SScratchStoreDwordx2, 2u},
    {0x17u, RdnaOpcode::SScratchStoreDwordx4, 4u},
    {0x18u, RdnaOpcode::SBufferStoreDword, 1u},
    {0x19u, RdnaOpcode::SBufferStoreDwordx2, 2u},
    {0x1au, RdnaOpcode::SBufferStoreDwordx4, 4u},
};

std::array<std::uint32_t, 2> words(std::uint32_t opcode, std::uint32_t sdata, std::uint32_t sbase) {
    return {(0x3du << 26u) | (opcode << 18u) | (sdata << 6u) | sbase, 0u};
}

int passed = 0;
int failed = 0;
void check(const char* label, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    ok ? ++passed : ++failed;
}

template <typename Action>
void expectException(const char* label, Action action) {
    try {
        action();
        check(label, false);
    } catch (const std::exception&) {
        check(label, true);
    } catch (...) {
        check(label, false);
    }
}
}

int main() {
    for (const Case& item : cases) {
        const auto raw = words(item.encoding, 12u, 4u);
        try {
            const auto decoded = DecodeRdnaMemoryOp(raw, 0u);
            const bool valid = decoded.op == item.opcode &&
                               decoded.family == RdnaInstructionFamily::SMEM &&
                               decoded.sourceCount == 3u &&
                               decoded.destination.kind == RdnaOperandKind::Unknown &&
                               decoded.source0.kind == RdnaOperandKind::ScalarRegister &&
                               decoded.source0.reg == 8u &&
                               decoded.source2.kind == RdnaOperandKind::ScalarRegister &&
                               decoded.source2.reg == 12u &&
                               decoded.dataDwordCount == item.width &&
                               decoded.wordCount == 2u &&
                               decoded.rawWords[0] == raw[0] && decoded.rawWords[1] == raw[1];
            char label[90];
            std::snprintf(label, sizeof(label), "opcode 0x%02x nominal", item.encoding);
            check(label, valid);
        } catch (...) {
            check("nominal decoder raised exception", false);
        }

        const std::uint32_t lastValid = 106u - item.width;
        char label[90];
        std::snprintf(label, sizeof(label), "opcode 0x%02x last SGPR group", item.encoding);
        try {
            const auto decoded = DecodeRdnaMemoryOp(words(item.encoding, lastValid, 4u), 0u);
            check(label, decoded.source2.kind == RdnaOperandKind::ScalarRegister &&
                         decoded.source2.reg == lastValid);
        } catch (...) {
            check(label, false);
        }

        if (item.width > 1u) {
            std::snprintf(label, sizeof(label), "opcode 0x%02x SGPR overflow rejected", item.encoding);
            expectException(label, [&] {
                (void)DecodeRdnaMemoryOp(words(item.encoding, lastValid + 1u, 4u), 0u);
            });
        }

        if (item.encoding >= 0x18u) {
            std::snprintf(label, sizeof(label), "opcode 0x%02x descriptor alignment rejected", item.encoding);
            expectException(label, [&] {
                (void)DecodeRdnaMemoryOp(words(item.encoding, 12u, 3u), 0u);
            });
            std::snprintf(label, sizeof(label), "opcode 0x%02x descriptor overflow rejected", item.encoding);
            expectException(label, [&] {
                (void)DecodeRdnaMemoryOp(words(item.encoding, 12u, 54u), 0u);
            });
        }
    }

    const std::array<std::uint32_t, 1> truncated = {words(0x10u, 12u, 4u)[0]};
    expectException("truncated SMEM instruction rejected", [&] {
        (void)DecodeRdnaSmem(0u, truncated, 0u);
    });

    // Existing SMEM loads must continue to decode with a destination and two sources.
    constexpr RdnaOpcode loadOps[] = {
        RdnaOpcode::SLoadDword, RdnaOpcode::SLoadDwordx2,
        RdnaOpcode::SLoadDwordx4, RdnaOpcode::SLoadDwordx8,
        RdnaOpcode::SLoadDwordx16, RdnaOpcode::SScratchLoadDword,
        RdnaOpcode::SScratchLoadDwordx2, RdnaOpcode::SScratchLoadDwordx4,
        RdnaOpcode::SBufferLoadDword, RdnaOpcode::SBufferLoadDwordx2,
        RdnaOpcode::SBufferLoadDwordx4, RdnaOpcode::SBufferLoadDwordx8,
        RdnaOpcode::SBufferLoadDwordx16
    };
    constexpr std::uint32_t loadWidths[] = {1u, 2u, 4u, 8u, 16u, 1u, 2u, 4u, 1u, 2u, 4u, 8u, 16u};
    for (std::uint32_t opcode = 0u; opcode < 13u; ++opcode) {
        char label[90];
        std::snprintf(label, sizeof(label), "load opcode 0x%02x", opcode);
        try {
            const auto raw = words(opcode, 12u, 4u);
            const auto decoded = DecodeRdnaMemoryOp(raw, 0u);
            check(label,
                  decoded.family == RdnaInstructionFamily::SMEM &&
                  decoded.op == loadOps[opcode] &&
                  decoded.destination.kind == RdnaOperandKind::ScalarRegister &&
                  decoded.destination.reg == 12u &&
                  decoded.source0.kind == RdnaOperandKind::ScalarRegister &&
                  decoded.source0.reg == 8u &&
                  decoded.sourceCount == 2u &&
                  decoded.dataDwordCount == loadWidths[opcode] &&
                  decoded.wordCount == 2u);
        } catch (...) {
            check(label, false);
        }
    }

    std::printf("Result: %d/%d passed\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
