#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <cstdint>
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
    RdnaOperandKind source0Kind;
    std::uint32_t source0Value;
    std::uint32_t attribute;
    std::uint32_t channel;
    bool absolute;
    bool negate;
    bool clamp;
    std::uint32_t omod;
};

struct Refused {
    std::vector<std::uint32_t> words;
    const char* reason;
    const char* name;
};

}

int main() {
    constexpr std::uint32_t Destination = 5;
    const std::vector<Accepted> accepted{
        {{0xd6000005u, 0x02020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr0.x", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, false, false, 0u},
        {{0xd6010005u, 0x02020440u}, RdnaOpcode::VInterpP2F32, "v_interp_p2_f32_e64 v5, v2, attr0.y", RdnaOperandKind::VectorRegister, 2u, 0u, 1u, false, false, false, 0u},
        {{0xd6020005u, 0x02000200u}, RdnaOpcode::VInterpMovF32, "v_interp_mov_f32_e64 v5, p20, attr0.x", RdnaOperandKind::IntegerInlineConstant, 1u, 0u, 0u, false, false, false, 0u},
        {{0xd6000005u, 0x0202041fu}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr31.x", RdnaOperandKind::VectorRegister, 2u, 31u, 0u, false, false, false, 0u},
        {{0xd6000005u, 0x02020420u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr32.x", RdnaOperandKind::VectorRegister, 2u, 32u, 0u, false, false, false, 0u},
        {{0xd6000005u, 0x0203fe00u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v255, attr0.x", RdnaOperandKind::VectorRegister, 255u, 0u, 0u, false, false, false, 0u},
        {{0xd6000205u, 0x02020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, |v2|, attr0.x", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, true, false, false, 0u},
        {{0xd6000005u, 0x42020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, -v2, attr0.x", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, true, false, 0u},
        {{0xd6008005u, 0x02020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr0.x clamp", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, false, true, 0u},
        {{0xd6000005u, 0x0a020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr0.x mul:2", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, false, false, 1u},
        {{0xd6000005u, 0x12020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr0.x mul:4", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, false, false, 2u},
        {{0xd6000005u, 0x1a020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, v2, attr0.x div:2", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, false, false, false, 3u},
        {{0xd6000205u, 0x42020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, -|v2|, attr0.x", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, true, true, false, 0u},
        {{0xd6000205u, 0x5a020400u}, RdnaOpcode::VInterpP1F32, "v_interp_p1_f32_e64 v5, -|v2|, attr0.x div:2", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, true, true, false, 3u},
        {{0xd6010005u, 0x42020481u}, RdnaOpcode::VInterpP2F32, "v_interp_p2_f32_e64 v5, -v2, attr1.z", RdnaOperandKind::VectorRegister, 2u, 1u, 2u, false, true, false, 0u},
        {{0xd6018205u, 0x02020400u}, RdnaOpcode::VInterpP2F32, "v_interp_p2_f32_e64 v5, |v2|, attr0.x clamp", RdnaOperandKind::VectorRegister, 2u, 0u, 0u, true, false, true, 0u},
        {{0xd6028005u, 0x02000000u}, RdnaOpcode::VInterpMovF32, "v_interp_mov_f32_e64 v5, p10, attr0.x clamp", RdnaOperandKind::IntegerInlineConstant, 0u, 0u, 0u, false, false, true, 0u},
        {{0xd6020005u, 0x0a000000u}, RdnaOpcode::VInterpMovF32, "v_interp_mov_f32_e64 v5, p10, attr0.x mul:2", RdnaOperandKind::IntegerInlineConstant, 0u, 0u, 0u, false, false, false, 1u},
    };
    const std::vector<Refused> refused{
        {{0xd6000005u, 0x02000400u}, "not a vector register", "v_interp_p1_f32_e64 with a non-VGPR coordinate"},
        {{0xd6020005u, 0x02000600u}, "not an interpolation parameter", "v_interp_mov_f32_e64 with a reserved parameter"},
        {{0xd6000005u, 0x02060400u}, "third source is not the inline constant 0", "v_interp_p1_f32_e64 with a third source other than the inline constant 0"},
        {{0xd6000805u, 0x02020400u}, "modifiers are not implemented", "v_interp_p1_f32_e64 with op_sel"},
        {{0xd6000005u, 0x02020500u}, "modifiers are not implemented", "v_interp_p1_f32_e64 with the packed-half selector"},
        {{0xd6000105u, 0x02020400u}, "modifiers are not implemented", "v_interp_p1_f32_e64 with an absolute source 0"},
        {{0xd6020005u, 0x22000200u}, "modifiers are not implemented", "v_interp_mov_f32_e64 with a negated parameter"},
        {{0xd7424005u, 0x00020201u}, "modifiers are not implemented", "v_interp_p1ll_f16 with the high-half destination selector"},
    };
    int failures = 0;
    for (const auto& test : accepted) {
        auto code = test.words;
        code.push_back(0xbf810000u);
        try {
            const auto decoded = RdnaInstructionDecoder{}.Decode(code);
            if (decoded.instructions.empty()) {
                std::fprintf(stderr, "%s decoded no instruction\n", test.name);
                ++failures;
                continue;
            }
            const auto& instruction = decoded.instructions[0];
            const auto source0Value = test.source0Kind == RdnaOperandKind::VectorRegister ? instruction.source0.reg : instruction.source0.value;
            if (instruction.op != test.op || instruction.wordCount != test.words.size() || instruction.family != RdnaInstructionFamily::VOP3 ||
                instruction.destination.kind != RdnaOperandKind::VectorRegister || instruction.destination.reg != Destination ||
                instruction.destination.clamp != test.clamp || instruction.destination.omod != test.omod || instruction.sourceCount != 3u ||
                instruction.source0.kind != test.source0Kind || source0Value != test.source0Value ||
                instruction.source0.absolute != test.absolute || instruction.source0.negate != test.negate ||
                instruction.source1.kind != RdnaOperandKind::IntegerInlineConstant || instruction.source1.value != test.attribute ||
                instruction.source2.kind != RdnaOperandKind::IntegerInlineConstant || instruction.source2.value != test.channel) {
                std::fprintf(stderr, "%s decoded to the wrong opcode or operands\n", test.name);
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
    for (const std::uint32_t selector : {0x0u, 0x4000u}) {
        const std::vector<std::uint32_t> code{0xd75a0005u | selector, 0x041e0201u, 0xbf810000u};
        try {
            const auto decoded = RdnaInstructionDecoder{}.Decode(code);
            const auto& instruction = decoded.instructions.at(0);
            const std::uint32_t half = selector != 0u ? 5u : 6u;
            if (instruction.op != RdnaOpcode::VInterpP2F16 || instruction.destination.reg != Destination || instruction.destination.sdwaSel != half ||
                instruction.sourceCount != 4u || instruction.source0.reg != 1u || instruction.source1.value != 1u || instruction.source3.reg != 7u) {
                std::fprintf(stderr, "v_interp_p2_f16 v5, v1, attr1.x, v7%s decoded to the wrong opcode or operands\n", selector != 0u ? " op_sel:[0,0,0,1]" : "");
                ++failures;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "v_interp_p2_f16 v5, v1, attr1.x, v7%s was refused: %s\n", selector != 0u ? " op_sel:[0,0,0,1]" : "", error.what());
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
