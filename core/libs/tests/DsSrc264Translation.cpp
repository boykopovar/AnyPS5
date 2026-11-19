#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("ds_src2_64: " + message);
}

std::array<std::uint32_t, 2> Encode(std::uint8_t opcode, std::uint32_t offset0, std::uint32_t offset1,
    std::uint32_t destination, std::uint32_t data0, std::uint32_t data1, std::uint32_t address) {
    return {
        (0x36u << 26u) | (static_cast<std::uint32_t>(opcode) << 18u) | (offset1 << 8u) | offset0,
        (destination << 24u) | (data1 << 16u) | (data0 << 8u) | address
    };
}

struct Form {
    std::uint8_t Encoding;
    RdnaOpcode Opcode;
};

constexpr Form kForms[] = {
    {0xc0u, RdnaOpcode::DsAddSrc2U64},
    {0xc1u, RdnaOpcode::DsSubSrc2U64},
    {0xc2u, RdnaOpcode::DsRsubSrc2U64},
    {0xc3u, RdnaOpcode::DsIncSrc2U64},
    {0xc4u, RdnaOpcode::DsDecSrc2U64},
    {0xc5u, RdnaOpcode::DsMinSrc2I64},
    {0xc6u, RdnaOpcode::DsMaxSrc2I64},
    {0xc7u, RdnaOpcode::DsMinSrc2U64},
    {0xc8u, RdnaOpcode::DsMaxSrc2U64},
    {0xc9u, RdnaOpcode::DsAndSrc2B64},
    {0xcau, RdnaOpcode::DsOrSrc2B64},
    {0xcbu, RdnaOpcode::DsXorSrc2B64},
    {0xcdu, RdnaOpcode::DsWriteSrc2B64},
    {0xd2u, RdnaOpcode::DsMinSrc2F64},
    {0xd3u, RdnaOpcode::DsMaxSrc2F64},
};

void CheckDecode() {
    constexpr std::uint32_t offset0 = 8u;
    constexpr std::uint32_t offset1 = 0x10u;
    constexpr std::uint32_t sourceOffset = ((offset1 << 8u) | offset0) * 4u;
    for (const auto& form : kForms) {
        const auto code = Encode(form.Encoding, offset0, offset1, 12u, 16u, 20u, 4u);
        const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
        Require(instruction.op == form.Opcode, "encoding " + std::to_string(form.Encoding) + " decodes to the wrong opcode");
        Require(instruction.dataDwordCount == 2u, "encoding " + std::to_string(form.Encoding) + " does not read two dwords");
        Require(instruction.memoryOffset == 0u, "encoding " + std::to_string(form.Encoding) + " does not address offset 0");
        Require(instruction.secondaryOffset == sourceOffset, "encoding " + std::to_string(form.Encoding) + " reads its src2 at the wrong offset");
        Require(instruction.source0.reg == 4u, "encoding " + std::to_string(form.Encoding) + " has the wrong address register");
        Require(!instruction.gds, "encoding " + std::to_string(form.Encoding) + " reports gds");
    }

    auto gdsCode = Encode(0xc0u, 0u, 0u, 0u, 0u, 0u, 0u);
    gdsCode[0] |= 1u << 17u;
    const RdnaInstruction gds = DecodeRdnaDs(0u, gdsCode, 0u);
    Require(gds.gds, "a gds src2 instruction does not report gds");
}

const MemoryInfo& MemoryOf(const IrProgram& program, const IrValue* value, const char* what) {
    const auto index = value->Flags<MemoryFlags>().index;
    Require(index < program.Resources().memoryInfo.size(), std::string(what) + " has no memory info");
    return program.Resources().memoryInfo[index];
}

void CheckAtomicTranslation() {
    constexpr std::uint32_t offset1 = 0x10u;
    const auto code = Encode(0xc0u, 8u, offset1, 12u, 16u, 20u, 4u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsAddSrc2U64, "ds_add_src2_u64 did not decode");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* load = nullptr;
    const IrValue* construct = nullptr;
    const IrValue* atomic = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::LoadSharedU32x2) load = value;
        if (value->Opcode() == IrOpcode::CompositeConstructU64) construct = value;
        if (value->Opcode() == IrOpcode::SharedAtomicIAdd64) atomic = value;
    }
    Require(load != nullptr, "no two-dword shared load for the src2 operand");
    Require(construct != nullptr, "the two loaded dwords are not joined into a 64-bit value");
    Require(atomic != nullptr, "no SharedAtomicIAdd64");

    const MemoryInfo& source = MemoryOf(program, load, "the src2 load");
    Require(source.kind == ResourceKind::Lds, "the src2 load does not read LDS");
    Require(source.offset == (offset1 << 8u | 8u) * 4u, "the src2 load reads the wrong offset");
    const MemoryInfo& target = MemoryOf(program, atomic, "the atomic");
    Require(target.kind == ResourceKind::Lds, "the atomic does not target LDS");
    Require(target.offset == 0u, "the atomic does not target offset 0");
    Require(atomic->Argument(1) == construct, "the atomic does not take the joined 64-bit value");
}

void CheckWriteTranslation() {
    const auto code = Encode(0xcdu, 0x20u, 0x10u, 0u, 0u, 0u, 4u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsWriteSrc2B64, "ds_write_src2_b64 did not decode");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* load = nullptr;
    const IrValue* write = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::LoadSharedU32x2) load = value;
        if (value->Opcode() == IrOpcode::WriteSharedU32x2) write = value;
    }
    Require(load != nullptr, "no two-dword shared load for the src2 operand");
    Require(write != nullptr, "no WriteSharedU32x2");
    Require(MemoryOf(program, load, "the src2 load").offset == (0x10u << 8u | 0x20u) * 4u, "the src2 load reads the wrong offset");
    Require(MemoryOf(program, write, "the store").offset == 0u, "the store does not write offset 0");
}

}

int main() {
    CheckDecode();
    CheckAtomicTranslation();
    CheckWriteTranslation();
    return 0;
}
