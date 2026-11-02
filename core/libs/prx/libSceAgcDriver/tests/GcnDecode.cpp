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

void Vop3FormsOfMappedVop2AndVopcDecode() {
    const std::array<std::uint32_t, 3> subb{0xd2520401u, 0x001a0702u, 0xbf810000u};
    const auto borrow = Decode(subb);
    Check(borrow.op == RdnaOpcode::VSubCoCiU32, "VOP3 v_subb_u32 did not decode as v_sub_co_ci_u32");
    Check(borrow.wordCount == 2u, "VOP3 v_subb_u32 is not two words");
    Check(borrow.destination.kind == RdnaOperandKind::VectorRegister && borrow.destination.reg == 1u, "VOP3 v_subb_u32 lost its vector destination");
    Check(borrow.destination2.kind == RdnaOperandKind::ScalarRegister && borrow.destination2.reg == 4u, "VOP3 v_subb_u32 lost its borrow-out SGPR pair");
    Check(borrow.source2.kind == RdnaOperandKind::ScalarRegister && borrow.source2.reg == 6u, "VOP3 v_subb_u32 lost its borrow-in SGPR pair");
    const std::array<std::uint32_t, 3> subbrev{0xd2540401u, 0x001a0702u, 0xbf810000u};
    Check(Decode(subbrev).op == RdnaOpcode::VSubrevCoCiU32, "VOP3 v_subbrev_u32 did not decode as v_subrev_co_ci_u32");
    const std::array<std::uint32_t, 3> truF32{0xd01e0004u, 0x00020300u, 0xbf810000u};
    const auto always = Decode(truF32);
    Check(always.op == RdnaOpcode::VCmpTruF32, "VOP3 v_cmp_tru_f32 did not decode as its RDNA opcode");
    Check(always.destination.kind == RdnaOperandKind::ScalarRegister && always.destination.reg == 4u, "VOP3 v_cmp_tru_f32 lost its scalar destination");
    const std::array<std::uint32_t, 3> cmpsLtF32{0xd0820004u, 0x00020300u, 0xbf810000u};
    Check(Decode(cmpsLtF32).op == RdnaOpcode::VCmpLtF32, "VOP3 v_cmps_lt_f32 did not decode as v_cmp_lt_f32");
    const std::array<std::uint32_t, 3> cmpsTruF64{0xd0de0004u, 0x00020500u, 0xbf810000u};
    Check(Decode(cmpsTruF64).op == RdnaOpcode::VCmpTruF64, "VOP3 v_cmps_tru_f64 did not decode as v_cmp_tru_f64");
}

}

int main() {
    try {
        SetregImmKeepsItsLiteral();
        Vop3FormsOfMappedVop2AndVopcDecode();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures == 0) std::puts("GCN decode tests passed");
    return failures == 0 ? 0 : 1;
}
