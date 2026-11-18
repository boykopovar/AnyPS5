#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("ds src2 64: " + message);
}

struct Form {
    std::uint32_t encoding;
    RdnaOpcode decoded;
    IrOpcode atomic;
    const char* name;
};

constexpr std::array<Form, 5> Forms{{
    {0xc5u, RdnaOpcode::DsMinSrc2I64, IrOpcode::SharedAtomicSMin64, "ds_min_src2_i64"},
    {0xc6u, RdnaOpcode::DsMaxSrc2I64, IrOpcode::SharedAtomicSMax64, "ds_max_src2_i64"},
    {0xc7u, RdnaOpcode::DsMinSrc2U64, IrOpcode::SharedAtomicUMin64, "ds_min_src2_u64"},
    {0xc8u, RdnaOpcode::DsMaxSrc2U64, IrOpcode::SharedAtomicUMax64, "ds_max_src2_u64"},
    {0xc9u, RdnaOpcode::DsAndSrc2B64, IrOpcode::SharedAtomicAnd64, "ds_and_src2_b64"},
}};

std::array<std::uint32_t, 2> Code(std::uint32_t opcode, std::uint32_t offset, bool gds) {
    return {
        (0x36u << 26u) | (opcode << 18u) | (static_cast<std::uint32_t>(gds) << 17u) | offset,
        7u
    };
}

void CheckTranslation(const Form& form) {
    constexpr std::uint32_t offset0 = 6u;
    constexpr std::uint32_t offset1 = 0u;
    constexpr std::uint32_t combinedOffset = offset0 | (offset1 << 8u);
    const auto code = Code(form.encoding, combinedOffset, false);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == form.decoded, std::string(form.name) + " decoded to the wrong opcode");
    Require(instruction.opcodeId == form.encoding, std::string(form.name) + " decoded from the wrong opcode id");
    Require(instruction.dataDwordCount == 2u, std::string(form.name) + " is not a qword operation");
    Require(instruction.secondaryOffset == combinedOffset * 4u, std::string(form.name) + " has the wrong source offset");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* atomic = nullptr;
    const IrValue* load = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == form.atomic) atomic = value;
        if (value->Opcode() == IrOpcode::LoadSharedU32x2) load = value;
    }
    Require(atomic != nullptr, std::string(form.name) + " did not emit its 64-bit atomic");
    Require(load != nullptr, std::string(form.name) + " did not load the qword at the second address");

    const auto atomicIndex = atomic->Flags<MemoryFlags>().index;
    Require(atomicIndex < program.Resources().memoryInfo.size(), std::string(form.name) + " atomic memory index is out of bounds");
    const auto& target = program.Resources().memoryInfo[atomicIndex];
    Require(target.kind == ResourceKind::Lds, std::string(form.name) + " atomic does not target LDS");
    Require(target.offset == 0u, std::string(form.name) + " atomic does not update the first address");
    Require(target.dataDwords == 2u, std::string(form.name) + " atomic is not a qword update");

    const auto loadIndex = load->Flags<MemoryFlags>().index;
    Require(loadIndex < program.Resources().memoryInfo.size(), std::string(form.name) + " load memory index is out of bounds");
    const auto& source = program.Resources().memoryInfo[loadIndex];
    Require(source.kind == ResourceKind::Lds, std::string(form.name) + " source is not read from LDS");
    Require(source.offset == combinedOffset * 4u, std::string(form.name) + " source is not read from the second address");
}

void CheckRejections(const Form& form) {
    const auto gdsCode = Code(form.encoding, 0u, true);
    const RdnaInstruction gdsInstruction = DecodeRdnaDs(0u, gdsCode, 0u);
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool gdsThrew = false;
    try {
        context.TranslateInstruction(gdsInstruction);
    } catch (const std::exception&) {
        gdsThrew = true;
    }
    Require(gdsThrew, std::string(form.name) + " accepted the GDS form");

    const auto addressCode = Code(form.encoding, 0x8000u, false);
    bool addressThrew = false;
    try {
        (void)DecodeRdnaDs(0u, addressCode, 0u);
    } catch (const std::exception&) {
        addressThrew = true;
    }
    Require(addressThrew, std::string(form.name) + " accepted an offset taken from the address");
}

}

int main() {
    try {
        for (const auto& form : Forms) {
            CheckTranslation(form);
            CheckRejections(form);
        }
        std::puts("ds src2 64 translation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
