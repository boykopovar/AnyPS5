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
    return failures == 0 ? 0 : 1;
}
