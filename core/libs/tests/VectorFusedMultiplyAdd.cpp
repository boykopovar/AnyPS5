#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t Destination = 5u;
constexpr std::uint32_t Source0 = 6u;
constexpr std::uint32_t Source1 = 7u;
constexpr std::uint32_t Literal = 0x3fc00001u;

struct Term {
    bool literal;
    std::uint32_t value;
};

constexpr Term Register(std::uint32_t index) { return {false, index}; }
constexpr Term Constant() { return {true, Literal}; }

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("vop2 multiply-add: " + message);
}

Term Leaf(const IrValue* value, const std::string& name) {
    Require(value->Opcode() == IrOpcode::BitCastF32U32, name + " operand is not a plain f32 read");
    const IrValue* bits = value->Argument(0);
    if (bits->HasImmediate()) {
        return {true, bits->ImmediateU32()};
    }
    Require(bits->Opcode() == IrOpcode::GetVectorRegister, name + " operand is neither a literal nor a vector register");
    return {false, bits->Argument(0)->Register().index};
}

void Check(const std::string& name, std::uint32_t encoding, std::uint32_t wordCount, RdnaOpcode opcode, IrOpcode expected, const std::array<Term, 3>& terms) {
    const std::array<std::uint32_t, 2> words{(encoding << 25u) | (Destination << 17u) | (Source1 << 9u) | (256u + Source0), Literal};
    const RdnaInstruction instruction = DecodeRdnaVectorOp(std::span<const std::uint32_t>(words).first(wordCount), 0u);
    Require(instruction.family == RdnaInstructionFamily::VOP2, name + " is not decoded as VOP2");
    Require(instruction.opcodeId == encoding, name + " lost its encoding");
    Require(instruction.op == opcode, name + " decodes to another opcode");
    Require(instruction.wordCount == wordCount, name + " has the wrong length");
    Require(instruction.sourceCount == (wordCount == 2u ? 3u : 2u), name + " has the wrong source count");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrOpcode rejected = expected == IrOpcode::FPFma32 ? IrOpcode::FPMad32 : IrOpcode::FPFma32;
    const IrValue* result = nullptr;
    for (const auto* value : block.Instructions()) {
        Require(value->Opcode() != rejected, name + (expected == IrOpcode::FPFma32 ? " rounds the product separately" : " is fused"));
        if (value->Opcode() == expected) {
            Require(result == nullptr, name + " emits more than one multiply-add");
            result = value;
        }
    }
    Require(result != nullptr, name + " emits no multiply-add");
    for (std::uint32_t index = 0u; index < terms.size(); ++index) {
        const Term term = Leaf(result->Argument(index), name);
        Require(term.literal == terms[index].literal && term.value == terms[index].value, name + " reads the wrong operand " + std::to_string(index));
    }
}

}

int main() {
    try {
        const std::array<Term, 3> accumulate{Register(Source0), Register(Source1), Register(Destination)};
        const std::array<Term, 3> multiplyLiteral{Register(Source0), Constant(), Register(Source1)};
        const std::array<Term, 3> addLiteral{Register(Source0), Register(Source1), Constant()};
        Check("v_mac_f32", 0x1fu, 1u, RdnaOpcode::VMacF32, IrOpcode::FPMad32, accumulate);
        Check("v_madmk_f32", 0x20u, 2u, RdnaOpcode::VMadmkF32, IrOpcode::FPMad32, multiplyLiteral);
        Check("v_madak_f32", 0x21u, 2u, RdnaOpcode::VMadakF32, IrOpcode::FPMad32, addLiteral);
        Check("v_fmac_f32", 0x2bu, 1u, RdnaOpcode::VMacF32, IrOpcode::FPFma32, accumulate);
        Check("v_fmamk_f32", 0x2cu, 2u, RdnaOpcode::VMadmkF32, IrOpcode::FPFma32, multiplyLiteral);
        Check("v_fmaak_f32", 0x2du, 2u, RdnaOpcode::VMadakF32, IrOpcode::FPFma32, addLiteral);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
