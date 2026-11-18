#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

struct Form {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    std::uint32_t dwords;
    std::uint32_t dataRegisters;
};

constexpr Form DsForms[] = {
    {0xc0u, RdnaOpcode::DsAddSrc2U64, 2u, 0u},
    {0xc1u, RdnaOpcode::DsSubSrc2U64, 2u, 0u},
    {0xc2u, RdnaOpcode::DsRsubSrc2U64, 2u, 0u},
    {0xc3u, RdnaOpcode::DsIncSrc2U64, 2u, 0u},
    {0xc4u, RdnaOpcode::DsDecSrc2U64, 2u, 0u},
    {0xc5u, RdnaOpcode::DsMinSrc2I64, 2u, 0u},
    {0xc6u, RdnaOpcode::DsMaxSrc2I64, 2u, 0u},
    {0xc7u, RdnaOpcode::DsMinSrc2U64, 2u, 0u},
    {0xc8u, RdnaOpcode::DsMaxSrc2U64, 2u, 0u},
    {0xc9u, RdnaOpcode::DsAndSrc2B64, 2u, 0u},
    {0xcau, RdnaOpcode::DsOrSrc2B64, 2u, 0u},
    {0xcbu, RdnaOpcode::DsXorSrc2B64, 2u, 0u},
    {0xcdu, RdnaOpcode::DsWriteSrc2B64, 2u, 0u},
    {0xd2u, RdnaOpcode::DsMinSrc2F64, 2u, 0u},
    {0xd3u, RdnaOpcode::DsMaxSrc2F64, 2u, 0u},
};

constexpr Form SmemForms[] = {
    {0x10u, RdnaOpcode::SStoreDword, 1u, 1u},
    {0x11u, RdnaOpcode::SStoreDwordx2, 2u, 2u},
    {0x12u, RdnaOpcode::SStoreDwordx4, 4u, 4u},
    {0x15u, RdnaOpcode::SScratchStoreDword, 1u, 1u},
    {0x16u, RdnaOpcode::SScratchStoreDwordx2, 2u, 2u},
    {0x17u, RdnaOpcode::SScratchStoreDwordx4, 4u, 4u},
    {0x18u, RdnaOpcode::SBufferStoreDword, 1u, 1u},
    {0x19u, RdnaOpcode::SBufferStoreDwordx2, 2u, 2u},
    {0x1au, RdnaOpcode::SBufferStoreDwordx4, 4u, 4u},
    {0x40u, RdnaOpcode::SBufferAtomicSwap, 1u, 1u},
    {0x41u, RdnaOpcode::SBufferAtomicCmpswap, 1u, 2u},
    {0x42u, RdnaOpcode::SBufferAtomicAdd, 1u, 1u},
    {0x43u, RdnaOpcode::SBufferAtomicSub, 1u, 1u},
    {0x44u, RdnaOpcode::SBufferAtomicSmin, 1u, 1u},
    {0x45u, RdnaOpcode::SBufferAtomicUmin, 1u, 1u},
    {0x46u, RdnaOpcode::SBufferAtomicSmax, 1u, 1u},
    {0x47u, RdnaOpcode::SBufferAtomicUmax, 1u, 1u},
    {0x48u, RdnaOpcode::SBufferAtomicAnd, 1u, 1u},
    {0x49u, RdnaOpcode::SBufferAtomicOr, 1u, 1u},
    {0x4au, RdnaOpcode::SBufferAtomicXor, 1u, 1u},
    {0x4bu, RdnaOpcode::SBufferAtomicInc, 1u, 1u},
    {0x4cu, RdnaOpcode::SBufferAtomicDec, 1u, 1u},
    {0x60u, RdnaOpcode::SBufferAtomicSwapX2, 2u, 2u},
    {0x61u, RdnaOpcode::SBufferAtomicCmpswapX2, 2u, 4u},
    {0x62u, RdnaOpcode::SBufferAtomicAddX2, 2u, 2u},
    {0x63u, RdnaOpcode::SBufferAtomicSubX2, 2u, 2u},
    {0x64u, RdnaOpcode::SBufferAtomicSminX2, 2u, 2u},
    {0x65u, RdnaOpcode::SBufferAtomicUminX2, 2u, 2u},
    {0x66u, RdnaOpcode::SBufferAtomicSmaxX2, 2u, 2u},
    {0x67u, RdnaOpcode::SBufferAtomicUmaxX2, 2u, 2u},
    {0x68u, RdnaOpcode::SBufferAtomicAndX2, 2u, 2u},
    {0x69u, RdnaOpcode::SBufferAtomicOrX2, 2u, 2u},
    {0x6au, RdnaOpcode::SBufferAtomicXorX2, 2u, 2u},
    {0x6bu, RdnaOpcode::SBufferAtomicIncX2, 2u, 2u},
    {0x6cu, RdnaOpcode::SBufferAtomicDecX2, 2u, 2u},
    {0x80u, RdnaOpcode::SAtomicSwap, 1u, 1u},
    {0x81u, RdnaOpcode::SAtomicCmpswap, 1u, 2u},
    {0x82u, RdnaOpcode::SAtomicAdd, 1u, 1u},
    {0x83u, RdnaOpcode::SAtomicSub, 1u, 1u},
    {0x84u, RdnaOpcode::SAtomicSmin, 1u, 1u},
    {0x85u, RdnaOpcode::SAtomicUmin, 1u, 1u},
    {0x86u, RdnaOpcode::SAtomicSmax, 1u, 1u},
    {0x87u, RdnaOpcode::SAtomicUmax, 1u, 1u},
    {0x88u, RdnaOpcode::SAtomicAnd, 1u, 1u},
    {0x89u, RdnaOpcode::SAtomicOr, 1u, 1u},
    {0x8au, RdnaOpcode::SAtomicXor, 1u, 1u},
    {0x8bu, RdnaOpcode::SAtomicInc, 1u, 1u},
    {0x8cu, RdnaOpcode::SAtomicDec, 1u, 1u},
    {0xa0u, RdnaOpcode::SAtomicSwapX2, 2u, 2u},
    {0xa1u, RdnaOpcode::SAtomicCmpswapX2, 2u, 4u},
    {0xa2u, RdnaOpcode::SAtomicAddX2, 2u, 2u},
    {0xa3u, RdnaOpcode::SAtomicSubX2, 2u, 2u},
    {0xa4u, RdnaOpcode::SAtomicSminX2, 2u, 2u},
    {0xa5u, RdnaOpcode::SAtomicUminX2, 2u, 2u},
    {0xa6u, RdnaOpcode::SAtomicSmaxX2, 2u, 2u},
    {0xa7u, RdnaOpcode::SAtomicUmaxX2, 2u, 2u},
    {0xa8u, RdnaOpcode::SAtomicAndX2, 2u, 2u},
    {0xa9u, RdnaOpcode::SAtomicOrX2, 2u, 2u},
    {0xaau, RdnaOpcode::SAtomicXorX2, 2u, 2u},
    {0xabu, RdnaOpcode::SAtomicIncX2, 2u, 2u},
    {0xacu, RdnaOpcode::SAtomicDecX2, 2u, 2u},
};

void Check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

template <typename TAction>
void RequireThrows(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::exception& error) {
        Check(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error("invalid instruction was accepted");
}

void CheckDsForms() {
    for (const auto& form : DsForms) {
        for (auto offset : {0u, 1u, 0x3fffu, 0x4000u, 0x7fffu}) {
            for (auto addr : {0u, 17u, 255u}) {
                for (auto gds : {0u, 1u}) {
                    const std::array words{0xd8000000u | (form.encoding << 18u) | (gds << 17u) | offset, 0xfffefdu << 8u | addr};
                    const auto inst = DecodeRdnaInstruction(0x40u, words, 0u);
                    const auto signedOffset = offset < 0x4000u ? offset : offset - 0x8000u;
                    Check(inst.op == form.opcode && inst.opcodeId == form.encoding, "DS opcode");
                    Check(inst.family == RdnaInstructionFamily::DS && inst.gds == (gds != 0u), "DS family or GDS");
                    Check(inst.dataDwordCount == 2u && inst.dataBits == 32u, "DS width");
                    Check(inst.sourceCount == 1u && inst.source0.kind == RdnaOperandKind::VectorRegister && inst.source0.reg == addr, "DS address");
                    Check(inst.destination.kind == RdnaOperandKind::None && inst.source1.kind == RdnaOperandKind::None && inst.source2.kind == RdnaOperandKind::None, "DS unused fields became operands");
                    Check(inst.memoryOffset == 0u && inst.secondaryOffset == signedOffset * 4u, "DS signed dword offset");
                    Check(inst.wordCount == 2u && inst.programCounter == 0x40u && inst.rawWords[0] == words[0] && inst.rawWords[1] == words[1], "DS raw words");
                    RequireThrows([&] { DecodeRdnaInstruction(0u, std::span(words).first(1u), 0u); }, "truncated DS");
                }
            }
        }
        const std::array dynamic{0xd8008000u | (form.encoding << 18u), 3u};
        RequireThrows([&] { DecodeRdnaInstruction(0u, dynamic, 0u); }, "offset taken from the address");
        const std::array stream{0xbf800000u, 0xd8000004u | (form.encoding << 18u), 255u, 0xbf810000u};
        const auto program = RdnaInstructionDecoder{}.Decode(stream);
        Check(program.instructions.size() == 3u && program.instructions[1].op == form.opcode && program.instructions[1].programCounter == 4u && program.instructions[2].programCounter == 12u, "DS stream alignment");
    }
}

void CheckSmemForms() {
    for (const auto& form : SmemForms) {
        const bool atomic = form.encoding >= 0x40u;
        const bool buffer = atomic ? form.encoding < 0x80u : form.encoding >= 0x18u;
        for (auto offset : {0u, 4u, 0xfffffu, 0x100000u, 0x1fffffu}) {
            for (auto soffset : {0u, 101u, 106u, 107u, 124u, 125u}) {
                for (auto flags : {0u, 1u, 2u, 3u}) {
                    const std::array words{0xf4000000u | (form.encoding << 18u) | ((flags & 1u) << 16u) | ((flags >> 1u) << 14u) | (20u << 6u) | 4u, (soffset << 25u) | offset};
                    if (buffer && offset >= 0x100000u) {
                        RequireThrows([&] { DecodeRdnaInstruction(0u, words, 0u); }, "negative SMEM buffer offset");
                        continue;
                    }
                    const auto inst = DecodeRdnaInstruction(0x80u, words, 0u);
                    Check(inst.op == form.opcode && inst.opcodeId == form.encoding && IsScalarMemoryOpcode(inst.op), "SMEM opcode classification");
                    Check(inst.family == RdnaInstructionFamily::SMEM && inst.wordCount == 2u && inst.programCounter == 0x80u, "SMEM length or PC");
                    Check(inst.rawWords[0] == words[0] && inst.rawWords[1] == words[1], "SMEM raw words");
                    Check(inst.dataDwordCount == form.dwords && inst.dataBits == 32u && inst.dataComponents == form.dataRegisters, "SMEM input or memory width");
                    Check(inst.glc == ((flags & 1u) != 0u) && inst.dlc == ((flags & 2u) != 0u), "SMEM cache flags");
                    Check(inst.memoryOffset == (offset < 0x100000u ? offset : offset | 0xffe00000u), "SMEM signed offset");
                    Check(inst.sourceCount == 3u && inst.source0.kind == RdnaOperandKind::ScalarRegister && inst.source0.reg == 8u, "SMEM base");
                    const auto expectedOffset = DecodeRdnaScalarSource(soffset, 0x80u);
                    Check(inst.source1.kind == expectedOffset.kind && inst.source1.reg == expectedOffset.reg, "SMEM register offset");
                    Check(inst.source2.kind == RdnaOperandKind::ScalarRegister && inst.source2.reg == 20u, "SMEM data source");
                    if (atomic && inst.glc) {
                        Check(inst.destination.kind == RdnaOperandKind::ScalarRegister && inst.destination.reg == 20u, "SMEM tied return");
                    } else {
                        Check(inst.destination.kind == RdnaOperandKind::None, "SMEM invented a return");
                    }
                    RequireThrows([&] { DecodeRdnaInstruction(0u, std::span(words).first(1u), 0u); }, "truncated SMEM");
                }
            }
        }
        const std::array stream{0xbf800000u, 0xf4000004u | (form.encoding << 18u) | (20u << 6u), 125u << 25u, 0xbf810000u};
        const auto program = RdnaInstructionDecoder{}.Decode(stream);
        Check(program.instructions.size() == 3u && program.instructions[1].op == form.opcode && program.instructions[1].programCounter == 4u && program.instructions[2].programCounter == 12u, "SMEM stream alignment");
        const auto badData = form.dataRegisters > 1u ? 107u : 108u;
        const std::array overflow{0xf4000004u | (form.encoding << 18u) | (badData << 6u), 125u << 25u};
        RequireThrows([&] { DecodeRdnaInstruction(0u, overflow, 0u); }, form.dataRegisters > 1u ? "data register range overflow" : "scalar destination");
        const std::array lastTuple{0xf4000004u | (form.encoding << 18u) | ((108u - form.dataRegisters) << 6u), 125u << 25u};
        const auto last = DecodeRdnaInstruction(0u, lastTuple, 0u);
        Check(last.op == form.opcode && last.dataComponents == form.dataRegisters, "last SMEM data tuple");
        if (form.dataRegisters > 1u) {
            const std::array misaligned{0xf4000004u | (form.encoding << 18u) | (21u << 6u), 125u << 25u};
            RequireThrows([&] { DecodeRdnaInstruction(0u, misaligned, 0u); }, "data register alignment");
        }
        if (buffer) {
            const std::array lastDescriptor{0xf4000034u | (form.encoding << 18u) | (20u << 6u), 125u << 25u};
            Check(DecodeRdnaInstruction(0u, lastDescriptor, 0u).source0.reg == 104u, "last SMEM buffer descriptor");
            const std::array misaligned{0xf4000003u | (form.encoding << 18u) | (20u << 6u), 125u << 25u};
            RequireThrows([&] { DecodeRdnaInstruction(0u, misaligned, 0u); }, "buffer descriptor alignment");
        }
    }
}

void CheckGoldenForms() {
    struct Golden {
        std::uint32_t word0;
        std::uint32_t word1;
        RdnaOpcode opcode;
    };
    constexpr Golden forms[] = {
        {0xf4400042u, 0x00000000u, RdnaOpcode::SStoreDword},
        {0xf4440082u, 0x00000000u, RdnaOpcode::SStoreDwordx2},
        {0xf4480102u, 0x00000000u, RdnaOpcode::SStoreDwordx4},
        {0xf4600044u, 0x00000000u, RdnaOpcode::SBufferStoreDword},
        {0xf4640084u, 0x00000000u, RdnaOpcode::SBufferStoreDwordx2},
        {0xf4680104u, 0x00000000u, RdnaOpcode::SBufferStoreDwordx4},
        {0xf4541942u, 0x00000000u, RdnaOpcode::SScratchStoreDword},
        {0xf4590082u, 0xca000000u, RdnaOpcode::SScratchStoreDwordx2},
        {0xf45d0102u, 0x00000000u, RdnaOpcode::SScratchStoreDwordx4},
        {0xf6080141u, 0xca000000u, RdnaOpcode::SAtomicAdd},
        {0xf6880281u, 0xca000000u, RdnaOpcode::SAtomicAddX2},
        {0xf6200141u, 0xca000000u, RdnaOpcode::SAtomicAnd},
        {0xf6a00281u, 0xfa000064u, RdnaOpcode::SAtomicAndX2},
        {0xf6040281u, 0xca000000u, RdnaOpcode::SAtomicCmpswap},
        {0xf6840501u, 0xca000000u, RdnaOpcode::SAtomicCmpswapX2},
        {0xf6b00281u, 0xca000000u, RdnaOpcode::SAtomicDecX2},
        {0xf6ac0281u, 0xca000000u, RdnaOpcode::SAtomicIncX2},
        {0xf6240141u, 0xfa000064u, RdnaOpcode::SAtomicOr},
        {0xf6180141u, 0xca000000u, RdnaOpcode::SAtomicSmax},
        {0xf6100141u, 0xca000000u, RdnaOpcode::SAtomicSmin},
        {0xf60c0141u, 0xca000000u, RdnaOpcode::SAtomicSub},
        {0xf6000141u, 0xca000000u, RdnaOpcode::SAtomicSwap},
        {0xf69c0281u, 0xca000000u, RdnaOpcode::SAtomicUmaxX2},
        {0xf6140141u, 0xca000000u, RdnaOpcode::SAtomicUmin},
        {0xf6280141u, 0xca000000u, RdnaOpcode::SAtomicXor},
        {0xf5080142u, 0xca000000u, RdnaOpcode::SBufferAtomicAdd},
        {0xf5880282u, 0x04000000u, RdnaOpcode::SBufferAtomicAddX2},
        {0xf5201942u, 0x04000000u, RdnaOpcode::SBufferAtomicAnd},
        {0xf5a00284u, 0x04000000u, RdnaOpcode::SBufferAtomicAndX2},
        {0xf5040282u, 0x04000000u, RdnaOpcode::SBufferAtomicCmpswap},
        {0xf5840502u, 0xca000000u, RdnaOpcode::SBufferAtomicCmpswapX2},
        {0xf5300142u, 0x04000000u, RdnaOpcode::SBufferAtomicDec},
        {0xf52c1942u, 0x04000000u, RdnaOpcode::SBufferAtomicInc},
        {0xf5ac0282u, 0xfa000064u, RdnaOpcode::SBufferAtomicIncX2},
        {0xf5240144u, 0x04000000u, RdnaOpcode::SBufferAtomicOr},
        {0xf5a402b0u, 0x04000000u, RdnaOpcode::SBufferAtomicOrX2},
        {0xf5180142u, 0xca000000u, RdnaOpcode::SBufferAtomicSmax},
        {0xf5981902u, 0x04000000u, RdnaOpcode::SBufferAtomicSmaxX2},
        {0xf5100142u, 0xfa000064u, RdnaOpcode::SBufferAtomicSmin},
        {0xf5900302u, 0x04000000u, RdnaOpcode::SBufferAtomicSminX2},
        {0xf58c0282u, 0x04000000u, RdnaOpcode::SBufferAtomicSubX2},
        {0xf5000142u, 0x04000000u, RdnaOpcode::SBufferAtomicSwap},
        {0xf51c0142u, 0x04000000u, RdnaOpcode::SBufferAtomicUmax},
        {0xf5140142u, 0x04000000u, RdnaOpcode::SBufferAtomicUmin},
        {0xf5280142u, 0x04000000u, RdnaOpcode::SBufferAtomicXor},
    };
    static_assert(std::size(forms) == 45u);
    for (const auto& form : forms) {
        const std::array words{form.word0, form.word1};
        const auto inst = DecodeRdnaInstruction(0u, words, 0u);
        Check(inst.op == form.opcode && inst.unsupportedReason.empty(), "LLVM SMEM encoding fixture");
    }
}

void CheckExistingForms() {
    const std::array load{0xf4000141u, 0u};
    const auto scalarLoad = DecodeRdnaInstruction(0u, load, 0u);
    Check(scalarLoad.op == RdnaOpcode::SLoadDword && scalarLoad.destination.reg == 5u && scalarLoad.source0.reg == 2u && scalarLoad.sourceCount == 2u, "existing scalar load");
    const std::array ds{0xd8000004u, 0x00000201u};
    const auto dsAdd = DecodeRdnaInstruction(0u, ds, 0u);
    Check(dsAdd.op == RdnaOpcode::DsAddU32 && dsAdd.sourceCount == 2u && dsAdd.source0.reg == 1u && dsAdd.source1.reg == 2u, "existing DS atomic");
    const std::array src2{0xda007fffu, 9u};
    const auto dsSrc2 = DecodeRdnaInstruction(0u, src2, 0u);
    Check(dsSrc2.op == RdnaOpcode::DsAddSrc2U32 && dsSrc2.dataDwordCount == 1u && dsSrc2.sourceCount == 1u && dsSrc2.secondaryOffset == 0xfffffffcu, "existing DS SRC2");
    const std::array vccStore{0xf4400000u | (106u << 6u) | 53u, 125u << 25u};
    const auto store = DecodeRdnaInstruction(0u, vccStore, 0u);
    Check(store.source0.kind == RdnaOperandKind::VccLo && store.source2.kind == RdnaOperandKind::VccLo, "SMEM VCC operands");
    for (auto opcode : {0x0du, 0x13u, 0x14u, 0x1bu, 0x4du, 0x6du, 0x8du, 0xadu}) {
        const std::array reserved{0xf4000000u | (opcode << 18u), 125u << 25u};
        RequireThrows([&] { DecodeRdnaInstruction(0u, reserved, 0u); }, "SMEM opcode is not supported");
    }
    for (auto opcode : {0xccu, 0xceu, 0xcfu, 0xd0u, 0xd1u, 0xd4u, 0xd5u}) {
        const std::array reserved{0xd8000000u | (opcode << 18u), 0u};
        RequireThrows([&] { DecodeRdnaInstruction(0u, reserved, 0u); }, "DS opcode is not supported");
    }
    const std::array badDescriptor{0xf5200000u | (20u << 6u) | 54u, 125u << 25u};
    RequireThrows([&] { DecodeRdnaInstruction(0u, badDescriptor, 0u); }, "SMEM base register range overflow");
}

}

int main() {
    try {
        CheckDsForms();
        CheckSmemForms();
        CheckGoldenForms();
        CheckExistingForms();
        std::puts("76 gfx1013 reference forms: operands, widths, offsets, flags, truncation and reserved encodings passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
