#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

struct ApertureSource {
    std::uint32_t code;
    RdnaOperandKind kind;
    const char* name;
};

const std::array<ApertureSource, 4> ApertureSources{{
    {235u, RdnaOperandKind::SrcSharedBase, "src_shared_base"},
    {236u, RdnaOperandKind::SrcSharedLimit, "src_shared_limit"},
    {237u, RdnaOperandKind::SrcPrivateBase, "src_private_base"},
    {238u, RdnaOperandKind::SrcPrivateLimit, "src_private_limit"},
}};

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

// The refusal must name the source it refused: a bare "not modelled" cannot be told from a decode gap.
template <typename Fn>
void ExpectRefusal(const char* source, Fn translate) {
    try {
        translate();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(source) == std::string::npos) {
            throw std::runtime_error(std::string("the refusal does not name ") + source + ": " + error.what());
        }
        return;
    }
    throw std::runtime_error(std::string("reading ") + source + " was accepted");
}

// s_mov_b32 s0, <source>: SOP1, format 0x7d, opcode 0x03, the source code in the low byte.
RdnaInstruction ScalarMove(std::uint32_t source) {
    const std::array<std::uint32_t, 2> words{0x80000000u | (0x7du << 23u) | (0x03u << 8u) | source, 0u};
    return DecodeRdnaSop1(0u, words, 0u);
}

void Translate(const RdnaInstruction& instruction) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder() = {&block};
    program.Metadata().blockInfo.resize(1);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);
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

// Translating the move reads the source as a value, which is the path a title that carries the
// aperture into an address does not take and one that reads it does.
void CheckReadRefuses() {
    for (const auto& source : ApertureSources) {
        const RdnaInstruction move = ScalarMove(source.code);
        Require(move.op == RdnaOpcode::SMovB32, "the scalar move decoded to another opcode");
        Require(move.source0.kind == source.kind, "the scalar move decodes source0 as the wrong operand kind");
        ExpectRefusal(source.name, [&] { Translate(move); });
    }
}

}

int main() {
    CheckDecode();
    CheckReadRefuses();
    std::puts("aperture sources tests passed");
    return 0;
}
