#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Accepted {
    std::vector<std::uint32_t> words;
    RdnaOpcode op;
    const char* name;
};

struct Refused {
    std::vector<std::uint32_t> words;
    const char* reason;
    const char* name;
};

}

int main() {
    const std::vector<Accepted> accepted{
        {{0x2c040300u}, RdnaOpcode::VLshrrevB32, "v_lshrrev_b32"},
        {{0x6e040300u, 0x00003c00u}, RdnaOpcode::VFmamkF16, "v_fmamk_f16"},
        {{0x78040300u}, RdnaOpcode::VPkFmacF16, "v_pk_fmac_f16"},
        {{0xd5160002u, 0x00020300u}, RdnaOpcode::VLshrrevB32, "VOP3 v_lshrrev_b32"},
        {{0xd7640002u, 0x00020300u}, RdnaOpcode::VBcntU32B32, "VOP3 v_bcnt_u32_b32"},
        {{0xd7650002u, 0x00020300u}, RdnaOpcode::VMbcntLoU32B32, "VOP3 v_mbcnt_lo_u32_b32"},
        {{0xd7660002u, 0x00020300u}, RdnaOpcode::VMbcntHiU32B32, "VOP3 v_mbcnt_hi_u32_b32"},
        {{0xd52b0002u, 0x80020300u}, RdnaOpcode::VMacF32, "VOP3 v_fmac_f32 with a negated accumulator"},
        {{0xd52b0402u, 0x00020300u}, RdnaOpcode::VMacF32, "VOP3 v_fmac_f32 with an absolute accumulator"},
        {{0xd51f0002u, 0x60020300u}, RdnaOpcode::VMacF32, "VOP3 v_mac_f32 with negated sources"},
        {{0xd5368302u, 0x68020300u}, RdnaOpcode::VFmacF16, "VOP3 v_fmac_f16 with source modifiers, clamp and mul:2"},
        {{0xd5360402u, 0x80020300u}, RdnaOpcode::VFmacF16, "VOP3 v_fmac_f16 with accumulator modifiers"},
    };
    const std::vector<Refused> refused{
        {{0x2a040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x15"},
        {{0x2e040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x17"},
        {{0x32040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x19"},
        {{0x44040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x22"},
        {{0x46040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x23"},
        {{0x48040300u}, "VOP2 opcode is not implemented", "VOP2 opcode 0x24"},
        {{0xd5150002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x115"},
        {{0xd5170002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x117"},
        {{0xd5190002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x119"},
        {{0xd5220002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x122"},
        {{0xd5230002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x123"},
        {{0xd5240002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x124"},
        {{0xd5370002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x137"},
        {{0xd5380002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x138"},
        {{0xd53c0002u, 0x00020300u}, "VOP3 opcode is not implemented", "VOP3 opcode 0x13c"},
        {{0xd51f0002u, 0x80020300u}, "VOP3 source modifiers are not implemented", "VOP3 v_mac_f32 with a negated accumulator"},
        {{0xd5060402u, 0x00020300u}, "VOP3 source modifiers are not implemented", "VOP3 opcode 0x106 with an absolute accumulator"},
        {{0xd5360802u, 0x00020300u}, "VOP3 source modifiers are not implemented", "VOP3 v_fmac_f16 with op_sel"},
    };
    int failures = 0;
    for (const auto& test : accepted) {
        auto code = test.words;
        code.push_back(0xbf810000u);
        try {
            const auto decoded = RdnaInstructionDecoder{}.Decode(code);
            if (decoded.instructions.empty() || decoded.instructions[0].op != test.op || decoded.instructions[0].wordCount != test.words.size()) {
                std::fprintf(stderr, "%s decodes to the wrong opcode or length\n", test.name);
                ++failures;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s was refused: %s\n", test.name, error.what());
            ++failures;
        }
    }
    for (const auto& test : refused) {
        auto code = test.words;
        code.push_back(0xbf810000u);
        std::string refusal;
        try {
            static_cast<void>(RdnaInstructionDecoder{}.Decode(code));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        if (refusal.find(test.reason) == std::string::npos) {
            std::fprintf(stderr, "%s was not refused\n", test.name);
            ++failures;
        }
    }
    try {
        const std::vector<std::uint32_t> code{0xd52b0402u, 0x80020300u, 0xbf810000u};
        const auto decoded = RdnaInstructionDecoder{}.Decode(code);
        const RdnaInstruction& fmac = decoded.instructions.at(0);
        if (!fmac.source2.absolute || !fmac.source2.negate || fmac.source0.absolute || fmac.source0.negate || fmac.source1.absolute || fmac.source1.negate) {
            std::fprintf(stderr, "VOP3 v_fmac_f32 does not carry only its accumulator modifiers\n");
            ++failures;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VOP3 v_fmac_f32 with accumulator modifiers was refused: %s\n", error.what());
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
