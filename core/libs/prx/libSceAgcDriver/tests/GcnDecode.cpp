#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <string>

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

bool Throws(std::span<const std::uint32_t> words) {
    try {
        static_cast<void>(Decode(words));
    } catch (const std::exception&) {
        return true;
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

}

int main() {
    try {
        SetregImmKeepsItsLiteral();
        MtbufKeepsItsFormat();
        RsqClampIsTagged();
        SopkComparesAndAdd();
        SmrdLiteralOffset();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures == 0) std::puts("GCN decode tests passed");
    return failures == 0 ? 0 : 1;
}
