#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

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

bool RefusedAs(std::span<const std::uint32_t> words, std::string_view encoding, long long opcode) {
    try {
        static_cast<void>(Decode(words));
    } catch (const UnsupportedInstructionError& error) {
        return error.encoding != nullptr && encoding == error.encoding && error.opcodeValue == opcode;
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

void DppIsRefusedByName() {
    const std::array<std::uint32_t, 2> move{0x7e1002fau, 0xff011109u};
    Check(RefusedAs(move, "VOP1_DPP", 0x01), "DPP v_mov_b32 was not refused as VOP1_DPP");
    const std::array<std::uint32_t, 2> nop{0x7e0000fau, 0xff011100u};
    Check(RefusedAs(nop, "VOP1_DPP", 0x00), "DPP v_nop was not refused as VOP1_DPP");
    const std::array<std::uint32_t, 2> add{0x060a0cfau, 0xff011107u};
    Check(RefusedAs(add, "VOP2_DPP", 0x03), "DPP v_add_f32 was not refused as VOP2_DPP");
    const std::array<std::uint32_t, 2> carry{0x4a1416fau, 0xff011107u};
    Check(RefusedAs(carry, "VOP2_DPP", 0x25), "DPP v_add_i32, which AnyPS5 promotes to VOP3, was not refused as VOP2_DPP");
    const std::array<std::uint32_t, 2> compare{0x7c0206fau, 0xff011102u};
    Check(RefusedAs(compare, "VOPC_DPP", 0x01), "DPP v_cmp_lt_f32 was not refused as VOPC_DPP");
    const std::array<std::uint32_t, 2> integerCompare{0x7d0206fau, 0xff011102u};
    Check(RefusedAs(integerCompare, "VOPC_DPP", 0x81), "DPP v_cmp_lt_i32 was not refused as VOPC_DPP with its opcode");
    const std::array<std::uint32_t, 1> firstWordOnly{0x7c0206fau};
    Check(RefusedAs(firstWordOnly, "VOPC_DPP", 0x01), "a DPP instruction cut after its first word was not refused as VOPC_DPP");
    const std::array<std::uint32_t, 1> madmkFirstWord{0x400a0cfau};
    Check(RefusedAs(madmkFirstWord, "VOP2_DPP", 0x20), "DPP v_madmk_f32 cut before its constant was not refused as VOP2_DPP");
    const std::array<std::uint32_t, 1> highVgpr{0x7e1003fau};
    const auto fromV250 = Decode(highVgpr);
    Check(fromV250.op == RdnaOpcode::VMovB32 && fromV250.source0.kind == RdnaOperandKind::VectorRegister && fromV250.source0.reg == 250u, "v_mov_b32 from v250 was refused or decoded wrongly");
}

bool Throws(std::span<const std::uint32_t> words) {
    try {
        static_cast<void>(Decode(words));
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool ThrowsOutOfRange(std::span<const std::uint32_t> words) {
    try {
        static_cast<void>(Decode(words));
    } catch (const std::out_of_range&) {
        return true;
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

bool ThrowsMentioning(std::span<const std::uint32_t> words, const char* text) {
    try {
        static_cast<void>(Decode(words));
    } catch (const std::exception& error) {
        return std::string(error.what()).find(text) != std::string::npos;
    }
    return false;
}

constexpr std::uint32_t Mtbuf(std::uint32_t opcode, std::uint32_t dataFormat, std::uint32_t numberFormat, bool addr64) {
    return (0x3au << 26u) | (numberFormat << 23u) | (dataFormat << 19u) | (opcode << 16u) | (addr64 ? 1u << 15u : 0u) | (1u << 13u) | 0x10u;
}

constexpr std::uint32_t MtbufOperands = 2u | (6u << 8u) | (3u << 16u) | (0x80u << 24u);

void MtbufKeepsItsFormat() {
    constexpr std::uint32_t dataFormat = 14u;
    constexpr std::uint32_t numberFormat = 7u;
    const std::array<std::uint32_t, 2> store{Mtbuf(7u, dataFormat, numberFormat, false), MtbufOperands};
    const auto instruction = Decode(store);
    const auto unified = GcnBufferFormats()[(dataFormat << 3u) | numberFormat];
    Check(instruction.op == RdnaOpcode::TbufferStoreFormatXyzw, "tbuffer_store_format_xyzw did not decode as its RDNA opcode");
    Check(instruction.wordCount == 2u, "a GCN MTBUF is not two words");
    Check(unified != 0u && instruction.dataFormat == (unified & 0xfu) && instruction.numberFormat == (unified >> 4u), "MTBUF DFMT 32_32_32_32 with NFMT FLOAT did not become its unified RDNA format");
    Check(instruction.memoryOffset == 0x10u && instruction.idxen && !instruction.offen, "MTBUF offset or index flags changed");
    const std::array<std::uint32_t, 2> load{Mtbuf(0u, 4u, 4u, false), MtbufOperands};
    Check(Decode(load).op == RdnaOpcode::TbufferLoadFormatX, "tbuffer_load_format_x did not decode as its RDNA opcode");
    const std::array<std::uint32_t, 2> reserved{Mtbuf(0u, 4u, 4u, false), MtbufOperands | (1u << 21u)};
    Check(Decode(reserved).op == RdnaOpcode::TbufferLoadFormatX, "MTBUF word 1 bit 21, reserved on GCN, changed the RDNA opcode");
    const std::array<std::uint32_t, 2> addr64{Mtbuf(7u, dataFormat, numberFormat, true), MtbufOperands};
    Check(Throws(addr64), "an ADDR64 MTBUF was accepted");
    const std::array<std::uint32_t, 2> invalid{Mtbuf(7u, 0u, numberFormat, false), MtbufOperands};
    Check(Throws(invalid), "an MTBUF with DFMT INVALID was accepted");
}

void RsqClampIsTagged() {
    constexpr std::uint32_t source = 256u + 5u;
    const std::array<std::uint32_t, 1> vop1{(0x3fu << 25u) | (1u << 17u) | (0x2cu << 9u) | source};
    Check(Decode(vop1).op == RdnaOpcode::VRsqClampF32, "VOP1 v_rsq_clamp_f32 did not decode as VRsqClampF32");
    const std::array<std::uint32_t, 1> rsq{(0x3fu << 25u) | (1u << 17u) | (0x2eu << 9u) | source};
    Check(Decode(rsq).op == RdnaOpcode::VRsqF32, "VOP1 v_rsq_f32 changed its opcode");
    const std::array<std::uint32_t, 2> vop3{(0x34u << 26u) | (0x1acu << 17u) | 1u, source};
    Check(Decode(vop3).op == RdnaOpcode::VRsqClampF32, "VOP3 v_rsq_clamp_f32 did not decode as VRsqClampF32");
}

void SopkComparesAndAdd() {
    constexpr std::uint32_t immediate = 0x1234u;
    const auto sopk = [](std::uint32_t opcode) { return (0xbu << 28u) | (opcode << 23u) | (5u << 16u) | immediate; };
    const std::array<std::uint32_t, 1> addk{sopk(0x0fu)};
    Check(Decode(addk).op == RdnaOpcode::SAddI32, "s_addk_i32 did not decode as its RDNA opcode");
    const std::array<std::uint32_t, 1> cmpkEq{sopk(0x03u)};
    Check(Decode(cmpkEq).op == RdnaOpcode::SCmpEqI32, "s_cmpk_eq_i32 did not decode as its RDNA opcode");
    const std::array<std::uint32_t, 1> cmpkLe{sopk(0x0eu)};
    Check(Decode(cmpkLe).op == RdnaOpcode::SCmpLeU32, "s_cmpk_le_u32 did not decode as its RDNA opcode");
}

constexpr std::uint32_t Smrd(std::uint32_t opcode, bool immediate, std::uint32_t offset) {
    return (0x18u << 27u) | (opcode << 22u) | (8u << 15u) | (2u << 9u) | (immediate ? 1u << 8u : 0u) | offset;
}

void SmrdLiteralOffset() {
    const std::array<std::uint32_t, 2> literal{Smrd(10u, false, 0xffu), 0x40u};
    const auto instruction = Decode(literal);
    Check(instruction.op == RdnaOpcode::SBufferLoadDwordx4, "s_buffer_load_dwordx4 with a literal offset did not decode as its RDNA opcode");
    Check(instruction.wordCount == 2u, "a GCN SMRD with a literal offset is not two words");
    Check(instruction.memoryOffset == 0x100, "an SMRD literal offset is not counted in dwords");
    Check(instruction.source1.kind == RdnaOperandKind::Null, "an SMRD literal offset added an SGPR offset");
    const std::array<std::uint32_t, 1> immediate{Smrd(10u, true, 0x10u)};
    const auto immediateInstruction = Decode(immediate);
    Check(immediateInstruction.wordCount == 1u && immediateInstruction.memoryOffset == 0x40, "an SMRD immediate offset changed");
    const std::array<std::uint32_t, 2> largest{Smrd(10u, false, 0xffu), 0x3ffffu};
    Check(Decode(largest).memoryOffset == 0xffffc, "the largest SMRD literal offset RDNA encodes was refused");
    const std::array<std::uint32_t, 2> beyond{Smrd(10u, false, 0xffu), 0x40000u};
    Check(Throws(beyond), "an SMRD literal offset past the RDNA offset range was accepted");
    const std::array<std::uint32_t, 1> truncated{Smrd(10u, false, 0xffu)};
    Check(Throws(truncated), "an SMRD literal offset past the end of the code was accepted");
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

void Vop3OpcodeTakesBit16() {
    const std::array<std::uint32_t, 3> add3{0xd2db0011u, 0x04104100u, 0xbf810000u};
    const auto sum = Decode(add3);
    Check(sum.op == RdnaOpcode::VAdd3U32, "VOP3 opcode 0x16d with bit 16 set did not decode as v_add3_u32 (0x36d)");
    Check(sum.wordCount == 2u, "VOP3 v_add3_u32 is not two words");
    Check(sum.destination.kind == RdnaOperandKind::VectorRegister && sum.destination.reg == 17u, "VOP3 v_add3_u32 lost its vector destination");
    Check(sum.destination2.kind == RdnaOperandKind::Unknown, "VOP3 v_add3_u32 took bits 14:8 as a scalar destination");
    Check(sum.source0.kind == RdnaOperandKind::VectorRegister && sum.source0.reg == 0u && sum.source1.kind == RdnaOperandKind::ScalarRegister && sum.source1.reg == 32u && sum.source2.kind == RdnaOperandKind::VectorRegister && sum.source2.reg == 4u, "VOP3 v_add3_u32 lost a source");
    const std::array<std::uint32_t, 3> divScale{0xd2da0011u, 0x04104100u, 0xbf810000u};
    const auto scale = Decode(divScale);
    Check(scale.op == RdnaOpcode::VDivScaleF32, "VOP3 opcode 0x16d without bit 16 did not decode as v_div_scale_f32");
    Check(scale.destination2.kind == RdnaOperandKind::ScalarRegister && scale.destination2.reg == 0u, "VOP3 v_div_scale_f32 lost its scalar destination");
    for (const std::uint32_t opcode : {0x303u, 0x307u, 0x308u, 0x30du, 0x30eu, 0x314u, 0x346u, 0x347u, 0x351u, 0x354u, 0x357u, 0x35eu, 0x36du, 0x36fu, 0x371u, 0x372u}) {
        const std::array<std::uint32_t, 3> gcn{(0x34u << 26u) | ((opcode & 0x1ffu) << 17u) | ((opcode >> 9u) << 16u) | 1u, 0x04104100u, 0xbf810000u};
        const std::array<std::uint32_t, 3> rdna{(0x35u << 26u) | (opcode << 16u) | 1u, 0x04104100u, 0xbf810000u};
        if (Decode(gcn).op != DecodeRdnaEncodedInstruction(0u, rdna, 0u).op) {
            const auto what = "VOP3 opcode " + std::to_string(opcode) + " (bit 16 set) did not decode as the same RDNA opcode";
            Check(false, what.c_str());
        }
    }
    const std::array<std::uint32_t, 3> notRsq{(0x34u << 26u) | (0x1acu << 17u) | (1u << 16u) | 1u, 256u + 5u, 0xbf810000u};
    Check(Throws(notRsq), "VOP3 opcode 0x3ac (v_rsq_clamp_f32's low bits with bit 16 set) was accepted");
    for (const std::uint32_t select : {1u, 2u, 4u, 8u}) {
        const std::array<std::uint32_t, 3> half{(0x34u << 26u) | ((0x303u & 0x1ffu) << 17u) | (1u << 16u) | (select << 12u) | 1u, 0x04104100u, 0xbf810000u};
        Check(Throws(half), "VOP3 v_add_nc_u16 with an OP_SEL bit (15:12) set was accepted");
    }
    const std::array<std::uint32_t, 3> highScalar{0xd2da1011u, 0x04104100u, 0xbf810000u};
    const auto highScale = Decode(highScalar);
    Check(highScale.op == RdnaOpcode::VDivScaleF32 && highScale.destination2.kind == RdnaOperandKind::ScalarRegister && highScale.destination2.reg == 16u, "VOP3 v_div_scale_f32 writing s[16:17], which sets bit 12, stopped decoding");
}

void SdwaKeepsItsModifierWord() {
    const std::array<std::uint32_t, 3> compareToSgpr{0x7c0206f9u, 0x06018e02u, 0xbf810000u};
    const auto toSgpr = Decode(compareToSgpr);
    Check(toSgpr.op == RdnaOpcode::VCmpLtF32, "SDWA v_cmp_lt_f32 did not decode as its RDNA opcode");
    Check(toSgpr.wordCount == 2u, "a GCN VOPC SDWA instruction is not two words");
    Check(toSgpr.destination.kind == RdnaOperandKind::ScalarRegister && toSgpr.destination.reg == 14u, "SDWA v_cmp_lt_f32 with SD set did not write the SGPR pair its SDST names");
    Check(toSgpr.source0.kind == RdnaOperandKind::VectorRegister && toSgpr.source0.reg == 2u && toSgpr.source0.sdwaSel == 1u, "SDWA v_cmp_lt_f32 lost its first source or its byte select");
    Check(toSgpr.source1.kind == RdnaOperandKind::VectorRegister && toSgpr.source1.reg == 3u && toSgpr.source1.sdwaSel == 6u, "SDWA v_cmp_lt_f32 lost its second source");
    const std::array<std::uint32_t, 2> compareToVcc{0x7c0206f9u, 0x06060002u};
    Check(Decode(compareToVcc).destination.kind == RdnaOperandKind::VccLo, "SDWA v_cmp_lt_f32 with SD clear did not write VCC");
    const std::array<std::uint32_t, 2> compareExecToSgpr{0x7c2206f9u, 0x06018e02u};
    const auto execToSgpr = Decode(compareExecToSgpr);
    Check(execToSgpr.op == RdnaOpcode::VCmpxLtF32 && execToSgpr.destination.kind == RdnaOperandKind::ExecLo, "SDWA v_cmpx_lt_f32 did not write EXEC");
    Check(execToSgpr.destination2.kind == RdnaOperandKind::ScalarRegister && execToSgpr.destination2.reg == 14u, "SDWA v_cmpx_lt_f32 with SD set did not also write the SGPR pair its SDST names");
    const std::array<std::uint32_t, 2> compareExecToVcc{0x7c2206f9u, 0x06060002u};
    Check(Decode(compareExecToVcc).destination2.kind == RdnaOperandKind::VccLo, "SDWA v_cmpx_lt_f32 with SD clear did not also write VCC");
    const std::array<std::uint32_t, 2> add{0x060a0cf9u, 0x04060507u};
    const auto sum = Decode(add);
    Check(sum.op == RdnaOpcode::VAddF32 && sum.wordCount == 2u, "SDWA v_add_f32 did not decode as one two-word instruction");
    Check(sum.destination.kind == RdnaOperandKind::VectorRegister && sum.destination.reg == 5u && sum.destination.sdwaSel == 5u, "SDWA v_add_f32 lost its destination or its word select");
    Check(sum.source0.reg == 7u && sum.source0.sdwaSel == 6u && sum.source1.reg == 6u && sum.source1.sdwaSel == 4u, "SDWA v_add_f32 lost a source or its select");
    const std::array<std::uint32_t, 2> move{0x7e1002f9u, 0x00020609u};
    const auto moved = Decode(move);
    Check(moved.op == RdnaOpcode::VMovB32 && moved.wordCount == 2u, "SDWA v_mov_b32 did not decode as one two-word instruction");
    Check(moved.destination.reg == 8u && moved.source0.kind == RdnaOperandKind::VectorRegister && moved.source0.reg == 9u && moved.source0.sdwaSel == 2u, "SDWA v_mov_b32 lost its operands or its byte select");
    const std::array<std::uint32_t, 2> select{0x000a0cf9u, 0x06060607u};
    const auto selected = Decode(select);
    Check(selected.op == RdnaOpcode::VCndmaskB32 && selected.wordCount == 2u, "SDWA v_cndmask_b32 did not decode as its RDNA opcode in one two-word instruction");
    Check(selected.destination.reg == 5u && selected.source0.reg == 7u && selected.source1.reg == 6u, "SDWA v_cndmask_b32 lost its operands");
    const std::array<std::uint32_t, 1> truncated{0x7c0206f9u};
    Check(ThrowsOutOfRange(truncated), "a GCN SDWA instruction without its modifier word was not refused as truncated");
    const std::array<std::uint32_t, 2> promoted{0x4a1416f9u, 0x06060007u};
    Check(ThrowsMentioning(promoted, "VOP2_SDWA"), "SDWA v_add_i32, which AnyPS5 promotes to VOP3, was not refused as such");
}

void MovFedOpcodeDecode() {
    constexpr std::uint32_t src0 = 256u;

    const auto vop1 = [](std::uint32_t op) {
        return (0x3fu << 25u) | (1u << 17u) | (op << 9u) | src0;
    };
    const auto vop3_1 = [](std::uint32_t op) {
        return std::array<std::uint32_t, 2>{(0x34u << 26u) | (op << 17u) | 1u, src0};
    };

    Check(Decode(std::array<std::uint32_t, 1>{vop1(0x09u)}).op == RdnaOpcode::VMovB32, "VOP1 v_mov_fed_b32 did not decode as VMovB32");
    Check(Decode(vop3_1(0x189u)).op == RdnaOpcode::VMovB32, "VOP3 v_mov_fed_b32 did not decode as VMovB32");
}

}

int main() {
    try {
        SetregImmKeepsItsLiteral();
        DppIsRefusedByName();
        MtbufKeepsItsFormat();
        RsqClampIsTagged();
        SopkComparesAndAdd();
        SmrdLiteralOffset();
        Vop3FormsOfMappedVop2AndVopcDecode();
        SdwaKeepsItsModifierWord();
        Vop3OpcodeTakesBit16();
        MovFedOpcodeDecode();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures == 0) std::puts("GCN decode tests passed");
    return failures == 0 ? 0 : 1;
}
