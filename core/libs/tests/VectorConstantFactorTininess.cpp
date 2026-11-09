#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace ShaderRecompiler;

namespace {

enum class Tininess {
    None,
    Rounded,
    ExponentsOnly,
    Full,
};

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("f32 constant factor: " + message);
}

const IrBlock& Translate(IrProgram& program, const std::string& name, const std::vector<std::uint32_t>& words, RdnaOpcode opcode, std::uint32_t floatMode) {
    const RdnaInstruction instruction = DecodeRdnaVectorOp(std::span<const std::uint32_t>(words), 0u);
    Require(instruction.op == opcode, name + " decodes to another opcode");
    Require(instruction.wordCount == words.size(), name + " has the wrong length");
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.SetFloatMode(ShaderFloatMode{floatMode, true, false, false});
    context.TranslateInstruction(instruction);
    return block;
}

void Check(const std::string& name, const std::vector<std::uint32_t>& words, RdnaOpcode opcode, Tininess expected, std::uint32_t floatMode = 0xc0u) {
    IrProgram program;
    const auto& block = Translate(program, name, words, opcode, floatMode);
    std::uint32_t multiplyHigh = 0u;
    std::uint32_t exponentCompares = 0u;
    std::uint32_t roundedCompares = 0u;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::UMulHi) ++multiplyHigh;
        if (value->Opcode() != IrOpcode::ULessThan32 || !value->Argument(1)->HasImmediate()) continue;
        if (value->Argument(1)->ImmediateU32() == 128u) ++exponentCompares;
        if (value->Argument(1)->ImmediateU32() == 0x207fffffu) ++roundedCompares;
    }
    const std::uint32_t expectedExponentCompares = expected == Tininess::ExponentsOnly || expected == Tininess::Full ? 1u : 0u;
    const std::uint32_t expectedRoundedCompares = expected == Tininess::Rounded ? 1u : 0u;
    const std::uint32_t expectedMultiplyHigh = expected == Tininess::Full ? 1u : 0u;
    Require(exponentCompares == expectedExponentCompares, name + " emits " + std::to_string(exponentCompares) + " exponent tiny product checks, expected " + std::to_string(expectedExponentCompares));
    Require(roundedCompares == expectedRoundedCompares, name + " emits " + std::to_string(roundedCompares) + " rounded tiny product checks, expected " + std::to_string(expectedRoundedCompares));
    Require(multiplyHigh == expectedMultiplyHigh, name + " emits " + std::to_string(multiplyHigh) + " significand products, expected " + std::to_string(expectedMultiplyHigh));
}

std::unordered_set<const IrValue*> Cone(const IrValue* root, const std::unordered_set<const IrValue*>& instructions) {
    std::unordered_set<const IrValue*> cone;
    std::vector<const IrValue*> pending{root};
    while (!pending.empty()) {
        const IrValue* value = pending.back();
        pending.pop_back();
        if (!instructions.contains(value) || !cone.insert(value).second) continue;
        for (const IrValue* argument : value->Arguments()) pending.push_back(argument);
    }
    return cone;
}

void CheckFlushSize(const std::string& name, const std::vector<std::uint32_t>& words, RdnaOpcode opcode, std::uint32_t floatMode, std::size_t expected) {
    IrProgram program;
    const auto& block = Translate(program, name, words, opcode, floatMode);
    const std::unordered_set<const IrValue*> instructions(block.Instructions().begin(), block.Instructions().end());
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() != IrOpcode::SelectF32) continue;
        const IrOpcode product = value->Argument(2)->Opcode();
        if (product != IrOpcode::FPMul32 && product != IrOpcode::FPFma32) continue;
        const auto flush = Cone(value, instructions);
        const auto kept = Cone(value->Argument(2), instructions);
        const auto size = static_cast<std::size_t>(std::count_if(flush.begin(), flush.end(), [&](const IrValue* instruction) { return !kept.contains(instruction); }));
        Require(size == expected, name + " flushes a tiny product with " + std::to_string(size) + " instructions, expected " + std::to_string(expected));
        return;
    }
    Require(false, name + " emits no tiny product flush");
}

void CheckFoldedSource(const std::string& name, const std::vector<std::uint32_t>& words, RdnaOpcode opcode, IrOpcode product, std::uint32_t index, std::uint32_t expected) {
    IrProgram program;
    const auto& block = Translate(program, name, words, opcode, 0xc0u);
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() != product) continue;
        const IrValue* source = value->Argument(index);
        Require(source->Opcode() == IrOpcode::BitCastF32U32 && source->Argument(0)->HasImmediate(), name + " source " + std::to_string(index) + " is not a constant");
        Require(source->Argument(0)->ImmediateU32() == expected, name + " source " + std::to_string(index) + " is folded to the wrong constant");
        return;
    }
    Require(false, name + " emits no product");
}

}

int main() {
    try {
        Check("v_mul_f32 v5, v6, v7", {0x100a0f06u}, RdnaOpcode::VMulF32, Tininess::Rounded);
        Check("v_mul_f32 v5, 2.0, v7", {0x100a0ef4u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, -4.0, v7", {0x100a0ef7u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, 1.0, v7", {0x100a0ef2u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, 0x7f800000, v7", {0x100a0effu, 0x7f800000u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, 0x00400000, v7", {0x100a0effu, 0x00400000u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, 1, v7", {0x100a0e81u}, RdnaOpcode::VMulF32, Tininess::None);
        Check("v_mul_f32 v5, 0.5, v7", {0x100a0ef0u}, RdnaOpcode::VMulF32, Tininess::Rounded);
        Check("v_mul_f32 v5, 0x3e800000, v7", {0x100a0effu, 0x3e800000u}, RdnaOpcode::VMulF32, Tininess::Rounded);
        Check("v_mul_f32 v5, 0x3f7fffff, v7", {0x100a0effu, 0x3f7fffffu}, RdnaOpcode::VMulF32, Tininess::Rounded);
        Check("v_mul_f32 v5, 0.15915494, v7", {0x100a0ef8u}, RdnaOpcode::VMulF32, Tininess::Rounded);
        Check("v_mul_legacy_f32 v5, 4.0, v7", {0x0e0a0ef6u}, RdnaOpcode::VMulLegacyF32, Tininess::None);
        Check("v_fmamk_f32 v5, v6, 0x40400000, v7", {0x580a0f06u, 0x40400000u}, RdnaOpcode::VMadmkF32, Tininess::None);
        Check("v_fmaak_f32 v5, v6, v7, 0x40400000", {0x5a0a0f06u, 0x40400000u}, RdnaOpcode::VMadakF32, Tininess::Rounded);
        Check("v_fma_f32 v5, v6, -|2.0|, v7", {0xd54b0205u, 0x441de906u}, RdnaOpcode::VFmaF32, Tininess::None);
        Check("v_fma_f32 v5, v6, -|0.5|, v7", {0xd54b0205u, 0x441de106u}, RdnaOpcode::VFmaF32, Tininess::Rounded);
        Check("v_mul_f32 v5, v6, v7 rounding toward +infinity", {0x100a0f06u}, RdnaOpcode::VMulF32, Tininess::Full, 0xc1u);
        Check("v_mul_f32 v5, 0.5, v7 rounding toward zero", {0x100a0ef0u}, RdnaOpcode::VMulF32, Tininess::ExponentsOnly, 0xc3u);
        Check("v_fma_f32 v5, v6, -|0.5|, v7 rounding toward -infinity", {0xd54b0205u, 0x441de106u}, RdnaOpcode::VFmaF32, Tininess::ExponentsOnly, 0xc2u);
        Check("v_mad_legacy_f32 v5, v6, v7, v8", {0xd5400005u, 0x04220f06u}, RdnaOpcode::VMadLegacyF32, Tininess::Full);
        Check("v_mul_f32 v5, 2.0, v7 with f32 denormals kept", {0x100a0ef4u}, RdnaOpcode::VMulF32, Tininess::None, 0xf0u);
        Check("v_mul_f32 v5, v6, v7 with f32 denormals kept", {0x100a0f06u}, RdnaOpcode::VMulF32, Tininess::None, 0xf0u);
        CheckFlushSize("v_mul_f32 v5, v6, v7", {0x100a0f06u}, RdnaOpcode::VMulF32, 0xc0u, 12u);
        CheckFlushSize("v_mul_f32 v5, v6, v7 rounding toward +infinity", {0x100a0f06u}, RdnaOpcode::VMulF32, 0xc1u, 36u);
        CheckFoldedSource("v_fma_f32 v5, v6, -|2.0|, v7", {0xd54b0205u, 0x441de906u}, RdnaOpcode::VFmaF32, IrOpcode::FPFma32, 1u, 0xc0000000u);
        CheckFoldedSource("v_mul_f32 v5, 0x00400000, v7", {0x100a0effu, 0x00400000u}, RdnaOpcode::VMulF32, IrOpcode::FPMul32, 0u, 0x00000000u);
        CheckFoldedSource("v_mul_f32 v5, 0x80400000, v7", {0x100a0effu, 0x80400000u}, RdnaOpcode::VMulF32, IrOpcode::FPMul32, 0u, 0x80000000u);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
