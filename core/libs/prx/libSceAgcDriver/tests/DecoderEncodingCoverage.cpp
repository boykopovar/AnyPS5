#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Case {
    std::array<std::uint32_t, 3> words;
    std::uint32_t wordCount;
    RdnaInstructionFamily family;
    RdnaOpcode op;
    const char* name;
};

constexpr std::uint32_t NoOperation = 0xbf800000u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::size_t Padding = 64u;

constexpr std::array<Case, 63> kEncodings{{
    {{{0xe02020c0u, 0x80047a00u}}, 2u, RdnaInstructionFamily::MUBUF, RdnaOpcode::BufferLoadUbyte, "buffer_load_ubyte"},
    {{{0xd8800078u, 0x1000201u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsAddRtnU32, "ds_add_rtn_u32"},
    {{{0xd8fa0004u, 0x1000000u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsAppend, "ds_append"},
    {{{0xdacc0000u, 0x3200231du}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsBpermuteB32, "ds_bpermute_b32"},
    {{{0xd8f60000u, 0x0u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsConsume, "ds_consume"},
    {{{0xd84c2058u, 0x100u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsMaxF32, "ds_max_f32"},
    {{{0xd8482058u, 0x204u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsMinF32, "ds_min_f32"},
    {{{0xd8280008u, 0x302u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsOrB32, "ds_or_b32"},
    {{{0xd8d4c038u, 0x8u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsSwizzleB32, "ds_swizzle_b32"},
    {{{0xd8b42008u, 0x5000500u}}, 2u, RdnaInstructionFamily::DS, RdnaOpcode::DsWrxchgRtnB32, "ds_wrxchg_rtn_b32"},
    {{{0xf0442108u, 0x70104u}}, 2u, RdnaInstructionFamily::MIMG, RdnaOpcode::ImageAtomicAdd, "image_atomic_add"},
    {{{0xf03c2108u, 0x900u}}, 2u, RdnaInstructionFamily::MIMG, RdnaOpcode::ImageAtomicSwap, "image_atomic_swap"},
    {{{0xbe913410u}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SAbsI32, "s_abs_i32"},
    {{{0xbea53c24u}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SAndSaveexecB32, "s_and_saveexec_b32"},
    {{{0xbeab446au}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SAndn1SaveexecB32, "s_andn1_saveexec_b32"},
    {{{0x94ea6b06u}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SBfeU64, "s_bfe_u64"},
    {{{0xbf970024u}}, 1u, RdnaInstructionFamily::SOPP, RdnaOpcode::SCbranchCdbg, "s_cbranch_cdbgsys"},
    {{{0xbf128002u}}, 1u, RdnaInstructionFamily::SOPC, RdnaOpcode::SCmpEqU64, "s_cmp_eq_u64"},
    {{{0xbf9f0000u}}, 1u, RdnaInstructionFamily::SOPP, RdnaOpcode::SCodeEnd, "s_code_end"},
    {{{0xbeea147eu}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SFf1I32B64, "s_ff1_i32_b64"},
    {{{0x976a8143u}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SLshl1AddU32, "s_lshl1_add_u32"},
    {{{0x8fea876au}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SLshlB64, "s_lshl_b64"},
    {{{0x846a806au}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SMaxI32, "s_max_i32"},
    {{{0x8313106au}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SMinI32, "s_min_i32"},
    {{{0x8c6b6a6bu}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SNandB32, "s_nand_b32"},
    {{{0x8cea526au}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SNandB64, "s_nand_b64"},
    {{{0x8d6a6a00u}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SNorB32, "s_nor_b32"},
    {{{0x8b087e6au}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SOrn2B32, "s_orn2_b32"},
    {{{0xbeea4022u}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SOrn2SaveexecB32, "s_orn2_saveexec_b32"},
    {{{0xbf8f0001u}}, 1u, RdnaInstructionFamily::SOPP, RdnaOpcode::SSetprio, "s_setprio"},
    {{{0x80eb6a18u}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SSubU32, "s_sub_u32"},
    {{{0x82b58035u}}, 1u, RdnaInstructionFamily::SOP2, RdnaOpcode::SSubbU32, "s_subb_u32"},
    {{{0xbefe097eu}}, 1u, RdnaInstructionFamily::SOP1, RdnaOpcode::SWqmB32, "s_wqm_b32"},
    {{{0xea6b3000u, 0x8003010bu}}, 2u, RdnaInstructionFamily::MTBUF, RdnaOpcode::TbufferLoadFormatXyzw, "tbuffer_load_format_xyzw"},
    {{{0xd70f6a0fu, 0x21f0cu}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VAddI32, "v_add_i32"},
    {{{0xd54e000bu, 0x2761710u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VAlignbitB32, "v_alignbit_b32"},
    {{{0xd54f0010u, 0x439fe80u, 0x301818u}}, 3u, RdnaInstructionFamily::VOP3, RdnaOpcode::VAlignbyteB32, "v_alignbyte_b32"},
    {{{0x7d9426f9u, 0x6868e80u}}, 2u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpEqF16, "v_cmp_eq_f16"},
    {{{0x7d040281u}}, 1u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpEqI32, "v_cmp_eq_i32"},
    {{{0x7d9900f9u, 0x86059a0fu}}, 2u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpGtF16, "v_cmp_gt_f16"},
    {{{0xd4e40006u, 0x1006au}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VCmpGtU64, "v_cmp_gt_u64"},
    {{{0x7d922af9u, 0x6868c80u}}, 2u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpLtF16, "v_cmp_lt_f16"},
    {{{0x7dda1ef9u, 0x586eb80u}}, 2u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpNeqF16, "v_cmp_neq_f16"},
    {{{0x7c100b05u}}, 1u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpUF32, "v_cmp_u_f32"},
    {{{0x7d2c2f16u}}, 1u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpxGeI32, "v_cmpx_ge_i32"},
    {{{0x7db22880u}}, 1u, RdnaInstructionFamily::VOPC, RdnaOpcode::VCmpxLtF16, "v_cmpx_lt_f16"},
    {{{0xd7680015u, 0x10125u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VCvtPknormI16F32, "v_cvt_pknorm_i16_f32"},
    {{{0xd7690004u, 0x20f06u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VCvtPknormU16F32, "v_cvt_pknorm_u16_f32"},
    {{{0x7e30770eu}}, 1u, RdnaInstructionFamily::VOP1, RdnaOpcode::VFfbhI32, "v_ffbh_i32"},
    {{{0x7e2a730au}}, 1u, RdnaInstructionFamily::VOP1, RdnaOpcode::VFfbhU32, "v_ffbh_u32"},
    {{{0xd5766a05u, 0x3feb8ffu, 0x92492492u}}, 3u, RdnaInstructionFamily::VOP3, RdnaOpcode::VMadU64U32, "v_mad_u64_u32"},
    {{{0xd5590015u, 0x4553e80u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VMed3U32, "v_med3_u32"},
    {{{0xd7512813u, 0x4061108u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VMin3F16, "v_min3_f16"},
    {{{0x741000f9u, 0x5051403u}}, 2u, RdnaInstructionFamily::VOP2, RdnaOpcode::VMinF16, "v_min_f16"},
    {{{0x7e008515u}}, 1u, RdnaInstructionFamily::VOP1, RdnaOpcode::VMovreldB32, "v_movreld_b32"},
    {{{0xd7770013u, 0x81ff03u, 0x7654321u}}, 3u, RdnaInstructionFamily::VOP3, RdnaOpcode::VPermlane16B32, "v_permlane16_b32"},
    {{{0xcc0f4012u, 0x18022511u}}, 2u, RdnaInstructionFamily::VOP3P, RdnaOpcode::VPkAddF16, "v_pk_add_f16"},
    {{{0xcc0e401bu, 0x1c6a011fu}}, 2u, RdnaInstructionFamily::VOP3P, RdnaOpcode::VPkFmaF16, "v_pk_fma_f16"},
    {{{0x782c0501u}}, 1u, RdnaInstructionFamily::VOP2, RdnaOpcode::VPkFmacF16, "v_pk_fmac_f16"},
    {{{0xcc104016u, 0x1802250cu}}, 2u, RdnaInstructionFamily::VOP3P, RdnaOpcode::VPkMulF16, "v_pk_mul_f16"},
    {{{0x54023880u}}, 1u, RdnaInstructionFamily::VOP2, RdnaOpcode::VSubrevCoCiU32, "v_subrev_co_ci_u32"},
    {{{0xd7450000u, 0x4220f00u}}, 2u, RdnaInstructionFamily::VOP3, RdnaOpcode::VXadU32, "v_xad_u32"},
    {{{0x3cc0c18du}}, 1u, RdnaInstructionFamily::VOP2, RdnaOpcode::VXnorB32, "v_xnor_b32"},
}};

}

int main() {
    int failures = 0;
    for (const auto& test : kEncodings) {
        std::vector<std::uint32_t> code(test.words.begin(), test.words.begin() + test.wordCount);
        code.resize(Padding, NoOperation);
        code.push_back(EndProgram);
        try {
            const auto decoded = RdnaInstructionDecoder{}.Decode(code);
            const RdnaInstruction& first = decoded.instructions.at(0u);
            if (first.op != test.op || first.family != test.family || first.wordCount != test.wordCount) {
                std::fprintf(stderr, "%s decodes to the wrong opcode, family or length\n", test.name);
                ++failures;
                continue;
            }
            bool encoded = true;
            for (std::uint32_t word = 0; word < test.wordCount; ++word) encoded = encoded && first.rawWords[word] == test.words[word];
            if (!encoded) {
                std::fprintf(stderr, "%s lost its encoded words\n", test.name);
                ++failures;
                continue;
            }
            const bool terminates = test.op == RdnaOpcode::SCodeEnd;
            const std::size_t expected = terminates ? 1u : Padding + 2u - test.wordCount;
            if (decoded.instructions.size() != expected) {
                std::fprintf(stderr, "%s consumed the wrong number of words\n", test.name);
                ++failures;
                continue;
            }
            if (decoded.instructions.back().op != (terminates ? RdnaOpcode::SCodeEnd : RdnaOpcode::SEndpgm)) {
                std::fprintf(stderr, "%s did not stop at its terminator\n", test.name);
                ++failures;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s was refused: %s\n", test.name, error.what());
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
