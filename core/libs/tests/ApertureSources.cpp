#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

struct ApertureSource {
    std::uint32_t code;
    RdnaOperandKind kind;
    const char* name;
    std::uint32_t low;
    std::uint32_t high;
};

const std::array<ApertureSource, 4> ApertureSources{{
    {235u, RdnaOperandKind::SrcSharedBase, "src_shared_base", 0x00000000u, 0x80000000u},
    {236u, RdnaOperandKind::SrcSharedLimit, "src_shared_limit", 0xffffffffu, 0x80000000u},
    {237u, RdnaOperandKind::SrcPrivateBase, "src_private_base", 0x00000000u, 0x70000000u},
    {238u, RdnaOperandKind::SrcPrivateLimit, "src_private_limit", 0xffffffffu, 0x70000000u},
}};

constexpr std::uint32_t MovB32 = 0x03u;
constexpr std::uint32_t MovB64 = 0x04u;

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

RdnaInstruction ScalarMove(std::uint32_t opcode, std::uint32_t source) {
    const std::array<std::uint32_t, 2> words{0x80000000u | (0x7du << 23u) | (opcode << 8u) | source, 0u};
    return DecodeRdnaSop1(0u, words, 0u);
}

std::map<std::uint32_t, std::uint32_t> TranslateWrites(const RdnaInstruction& instruction) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder() = {&block};
    program.Metadata().blockInfo.resize(1);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
    std::map<std::uint32_t, std::uint32_t> writes;
    for (const IrValue* value : block.Instructions()) {
        if (value->Opcode() != IrOpcode::SetScalarRegister) continue;
        const IrValue* written = value->Argument(1u)->Resolve();
        Require(written->HasImmediate(), "the write of s" + std::to_string(value->Argument(0u)->Register().index) + " is not a constant");
        writes[value->Argument(0u)->Register().index] = written->ImmediateU32();
    }
    return writes;
}

void CheckDecode() {
    for (const auto& source : ApertureSources) {
        const auto operand = DecodeRdnaScalarSource(source.code, 0u);
        Require(operand.kind == source.kind,
            "scalar source " + std::to_string(source.code) + " decoded to the wrong operand kind");
        Require(RdnaOperandToString(operand) == source.name,
            "scalar source " + std::to_string(source.code) + " is named " + RdnaOperandToString(operand));
    }
}

void CheckWord(const std::map<std::uint32_t, std::uint32_t>& writes, std::uint32_t reg, std::uint32_t expected, const std::string& read) {
    const auto found = writes.find(reg);
    Require(found != writes.end(), read + " did not write s" + std::to_string(reg));
    Require(found->second == expected, read + " wrote " + Hex(found->second) + " to s" + std::to_string(reg) + ", expected " + Hex(expected));
}

void CheckReadValues() {
    for (const auto& source : ApertureSources) {
        const RdnaInstruction narrow = ScalarMove(MovB32, source.code);
        Require(narrow.op == RdnaOpcode::SMovB32, "the 32-bit scalar move decoded to another opcode");
        Require(narrow.source0.kind == source.kind, "the 32-bit scalar move decodes source0 as the wrong operand kind");
        const auto narrowWrites = TranslateWrites(narrow);
        CheckWord(narrowWrites, 0u, source.low, std::string("s_mov_b32 of ") + source.name);
        Require(narrowWrites.size() == 1u, std::string("s_mov_b32 of ") + source.name + " wrote more than one register");
        const RdnaInstruction wide = ScalarMove(MovB64, source.code);
        Require(wide.op == RdnaOpcode::SMovB64, "the 64-bit scalar move decoded to another opcode");
        Require(wide.source0.kind == source.kind, "the 64-bit scalar move decodes source0 as the wrong operand kind");
        const auto wideWrites = TranslateWrites(wide);
        CheckWord(wideWrites, 0u, source.low, std::string("s_mov_b64 of ") + source.name);
        CheckWord(wideWrites, 1u, source.high, std::string("s_mov_b64 of ") + source.name);
        Require(wideWrites.size() == 2u, std::string("s_mov_b64 of ") + source.name + " wrote more than two registers");
    }
}

}

int main() {
    CheckDecode();
    CheckReadValues();
    std::puts("aperture sources tests passed");
    return 0;
}
