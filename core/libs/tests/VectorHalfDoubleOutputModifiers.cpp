#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t Destination = 5u;
constexpr std::uint32_t Source0 = 256u + 6u;
constexpr std::uint32_t Source1 = 256u + 7u;
constexpr std::uint32_t Source2 = 256u + 8u;

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("vector output modifiers: " + message);
}

std::array<std::uint32_t, 2> EncodeVop3(std::uint32_t opcode, std::uint32_t omod, std::uint32_t clamp = 0u, bool ternary = false) {
    const std::uint32_t word0 = 0xd4000000u | (opcode << 16u) | (clamp << 15u) | Destination;
    const std::uint32_t word1 = (omod << 27u) | (ternary ? (Source2 << 18u) : 0u) | (Source1 << 9u) | Source0;
    return {word0, word1};
}

const IrBlock& Translate(IrProgram& program, const std::string& name, const std::array<std::uint32_t, 2>& words, RdnaOpcode expectedOpcode, std::uint32_t floatMode, bool ieeeMode = false) {
    const RdnaInstruction instruction = DecodeRdnaVectorOp(std::span<const std::uint32_t>(words), 0u);
    Require(instruction.family == RdnaInstructionFamily::VOP3, name + " is not decoded as VOP3");
    Require(instruction.op == expectedOpcode, name + " decodes to another opcode");
    Require(instruction.wordCount == 2u, name + " has the wrong word count");
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.SetFloatMode(ShaderFloatMode{floatMode, true, ieeeMode, false});
    context.TranslateInstruction(instruction);
    return block;
}

void CheckScaledF16(const std::string& name, std::uint32_t opcode, RdnaOpcode expectedOpcode, std::uint32_t omod, bool ternary = false, std::uint32_t clamp = 0u) {
    const auto words = EncodeVop3(opcode, omod, clamp, ternary);
    IrProgram program;
    const auto& block = Translate(program, name, words, expectedOpcode, 0x40u, false);

    std::uint32_t stepInstructions = 0u;
    std::uint32_t denormalChecks = 0u;
    std::uint32_t specialChecks = 0u;
    std::uint32_t clampOperations = 0u;

    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::FPMin32) ++clampOperations;
        if (value->Opcode() == IrOpcode::IAdd32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 1u && step == 0x00800000u) ++stepInstructions;
            if (omod == 2u && step == 0x01000000u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ISub32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 3u && step == 0x00800000u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ULessThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x38800000u) ++denormalChecks;
        }
        if (value->Opcode() == IrOpcode::UGreaterThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x7f7fffffu) ++specialChecks;
        }
    }

    const std::uint32_t expectedDenormalChecks = opcode == 0x132u ? 2u : 1u;
    Require(stepInstructions == 1u, name + " emitted " + std::to_string(stepInstructions) + " step instructions, expected 1");
    Require(denormalChecks == expectedDenormalChecks, name + " emitted " + std::to_string(denormalChecks) + " denormal checks, expected " + std::to_string(expectedDenormalChecks));
    Require(specialChecks == 1u, name + " emitted " + std::to_string(specialChecks) + " special checks, expected 1");

    bool denormalSelectUsesSign = false;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SelectU32 && value->Argument(0)->Opcode() == IrOpcode::ULessThan32) {
            const auto* cmp = value->Argument(0);
            if (cmp->Argument(1)->HasImmediate() && cmp->Argument(1)->ImmediateU32() == 0x38800000u) {
                if (value->Argument(1)->Opcode() == IrOpcode::BitwiseAnd32 && value->Argument(1)->Argument(1)->HasImmediate() && value->Argument(1)->Argument(1)->ImmediateU32() == 0x80000000u) {
                    denormalSelectUsesSign = true;
                }
            }
        }
    }
    Require(denormalSelectUsesSign, name + " did not preserve sign on denormal flush");

    if (clamp != 0u) {
        Require(clampOperations == 1u, name + " emitted " + std::to_string(clampOperations) + " clamp operations, expected 1");
    }
}

void CheckScaledF64(const std::string& name, std::uint32_t opcode, RdnaOpcode expectedOpcode, std::uint32_t omod, bool ternary = false, std::uint32_t clamp = 0u) {
    const auto words = EncodeVop3(opcode, omod, clamp, ternary);
    IrProgram program;
    const auto& block = Translate(program, name, words, expectedOpcode, 0x40u, false);

    std::uint32_t stepInstructions = 0u;
    std::uint32_t denormalChecks = 0u;
    std::uint32_t specialChecks = 0u;
    std::uint32_t constructU64 = 0u;
    std::uint32_t clampOperations = 0u;

    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::FPSaturate64) ++clampOperations;
        if (value->Opcode() == IrOpcode::CompositeConstructU64) ++constructU64;
        if (value->Opcode() == IrOpcode::IAdd32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 1u && step == 0x00100000u) ++stepInstructions;
            if (omod == 2u && step == 0x00200000u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ISub32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 3u && step == 0x00100000u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ULessThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x00100000u) ++denormalChecks;
        }
        if (value->Opcode() == IrOpcode::UGreaterThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x7fefffffu) ++specialChecks;
        }
    }

    Require(stepInstructions == 1u, name + " emitted " + std::to_string(stepInstructions) + " step instructions, expected 1");
    Require(denormalChecks == 1u, name + " emitted " + std::to_string(denormalChecks) + " denormal checks, expected 1");
    Require(specialChecks == 1u, name + " emitted " + std::to_string(specialChecks) + " special checks, expected 1");
    Require(constructU64 >= 1u, name + " emitted no ConstructU64");

    bool denormalHighSelectUsesSign = false;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SelectU32 && value->Argument(0)->Opcode() == IrOpcode::ULessThan32) {
            const auto* cmp = value->Argument(0);
            if (cmp->Argument(1)->HasImmediate() && cmp->Argument(1)->ImmediateU32() == 0x00100000u) {
                if (value->Argument(1)->Opcode() == IrOpcode::BitwiseAnd32 && value->Argument(1)->Argument(1)->HasImmediate() && value->Argument(1)->Argument(1)->ImmediateU32() == 0x80000000u) {
                    denormalHighSelectUsesSign = true;
                }
            }
        }
    }
    Require(denormalHighSelectUsesSign, name + " did not preserve sign on denormal flush");

    if (clamp != 0u) {
        Require(clampOperations == 1u, name + " emitted " + std::to_string(clampOperations) + " clamp operations, expected 1");
    }
}

void CheckScaledF16Bits(const std::string& name, std::uint32_t opcode, RdnaOpcode expectedOpcode, std::uint32_t omod, bool ternary = false) {
    const auto words = EncodeVop3(opcode, omod, 0u, ternary);
    IrProgram program;
    const auto& block = Translate(program, name, words, expectedOpcode, 0x40u, false);

    std::uint32_t stepInstructions = 0u;
    std::uint32_t denormalChecks = 0u;
    std::uint32_t specialChecks = 0u;

    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::IAdd32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 1u && step == 0x0400u) ++stepInstructions;
            if (omod == 2u && step == 0x0800u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ISub32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (omod == 3u && step == 0x0400u) ++stepInstructions;
        }
        if (value->Opcode() == IrOpcode::ULessThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x0400u) ++denormalChecks;
        }
        if (value->Opcode() == IrOpcode::UGreaterThan32 && value->Argument(1)->HasImmediate()) {
            if (value->Argument(1)->ImmediateU32() == 0x7bffu) ++specialChecks;
        }
    }

    Require(stepInstructions == 1u, name + " emitted " + std::to_string(stepInstructions) + " step instructions, expected 1");
    Require(denormalChecks >= 1u, name + " emitted " + std::to_string(denormalChecks) + " denormal checks, expected >= 1");
    Require(specialChecks == 1u, name + " emitted " + std::to_string(specialChecks) + " special checks, expected 1");

    bool denormalSelectUsesSign = false;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SelectU32 && value->Argument(0)->Opcode() == IrOpcode::ULessThan32) {
            const auto* cmp = value->Argument(0);
            if (cmp->Argument(1)->HasImmediate() && cmp->Argument(1)->ImmediateU32() == 0x0400u) {
                if (value->Argument(1)->Opcode() == IrOpcode::BitwiseAnd32 && value->Argument(1)->Argument(1)->HasImmediate() && value->Argument(1)->Argument(1)->ImmediateU32() == 0x8000u) {
                    denormalSelectUsesSign = true;
                }
            }
        }
    }
    Require(denormalSelectUsesSign, name + " did not preserve sign on denormal flush");
}

void CheckIgnoredModifiers(const std::string& name, std::uint32_t opcode, RdnaOpcode expectedOpcode, std::uint32_t floatMode, bool ieeeMode, bool ternary = false) {
    const auto words = EncodeVop3(opcode, 1u, 0u, ternary);
    IrProgram program;
    const auto& block = Translate(program, name, words, expectedOpcode, floatMode, ieeeMode);

    std::uint32_t stepInstructions = 0u;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::IAdd32 && value->Argument(1)->HasImmediate()) {
            const std::uint32_t step = value->Argument(1)->ImmediateU32();
            if (step == 0x00800000u || step == 0x01000000u || step == 0x00100000u || step == 0x00200000u || step == 0x0400u || step == 0x0800u) {
                ++stepInstructions;
            }
        }
    }
    Require(stepInstructions == 0u, name + " applied output modifier when it should be ignored");
}

}

int main() {
    try {
        CheckIgnoredModifiers("v_cvt_f32_f64", 0x18fu, RdnaOpcode::VCvtF32F64, 0xe0u, false);

        CheckScaledF16("v_add_f16 mul:2", 0x132u, RdnaOpcode::VAddF16, 1u);
        CheckScaledF16("v_add_f16 mul:4", 0x132u, RdnaOpcode::VAddF16, 2u);
        CheckScaledF16("v_add_f16 div:2", 0x132u, RdnaOpcode::VAddF16, 3u);
        CheckScaledF16("v_add_f16 mul:2 clamp", 0x132u, RdnaOpcode::VAddF16, 1u, false, 1u);

        CheckScaledF16("v_mul_f16 mul:2", 0x135u, RdnaOpcode::VMulF16, 1u);
        CheckScaledF16("v_mul_f16 mul:4", 0x135u, RdnaOpcode::VMulF16, 2u);
        CheckScaledF16("v_mul_f16 div:2", 0x135u, RdnaOpcode::VMulF16, 3u);

        CheckScaledF16("v_fma_f16 mul:2", 0x34bu, RdnaOpcode::VFmaF16, 1u, true);
        CheckScaledF16("v_fma_f16 mul:4", 0x34bu, RdnaOpcode::VFmaF16, 2u, true);
        CheckScaledF16("v_fma_f16 div:2", 0x34bu, RdnaOpcode::VFmaF16, 3u, true);

        CheckScaledF64("v_add_f64 mul:2", 0x164u, RdnaOpcode::VAddF64, 1u);
        CheckScaledF64("v_add_f64 mul:4", 0x164u, RdnaOpcode::VAddF64, 2u);
        CheckScaledF64("v_add_f64 div:2", 0x164u, RdnaOpcode::VAddF64, 3u);
        CheckScaledF64("v_add_f64 mul:2 clamp", 0x164u, RdnaOpcode::VAddF64, 1u, false, 1u);

        CheckScaledF64("v_mul_f64 mul:2", 0x165u, RdnaOpcode::VMulF64, 1u);
        CheckScaledF64("v_mul_f64 mul:4", 0x165u, RdnaOpcode::VMulF64, 2u);
        CheckScaledF64("v_mul_f64 div:2", 0x165u, RdnaOpcode::VMulF64, 3u);

        CheckScaledF64("v_fma_f64 mul:2", 0x14cu, RdnaOpcode::VFmaF64, 1u, true);
        CheckScaledF64("v_fma_f64 mul:4", 0x14cu, RdnaOpcode::VFmaF64, 2u, true);
        CheckScaledF64("v_fma_f64 div:2", 0x14cu, RdnaOpcode::VFmaF64, 3u, true);

        CheckScaledF16Bits("v_div_fixup_f16 mul:2", 0x35fu, RdnaOpcode::VDivFixupF16, 1u, true);
        CheckScaledF16Bits("v_div_fixup_f16 mul:4", 0x35fu, RdnaOpcode::VDivFixupF16, 2u, true);
        CheckScaledF16Bits("v_div_fixup_f16 div:2", 0x35fu, RdnaOpcode::VDivFixupF16, 3u, true);

        CheckIgnoredModifiers("v_add_f16 with denormals kept", 0x132u, RdnaOpcode::VAddF16, 0xf0u, false);
        CheckIgnoredModifiers("v_add_f16 with ieee mode on", 0x132u, RdnaOpcode::VAddF16, 0x40u, true);
        CheckIgnoredModifiers("v_add_f64 with denormals kept", 0x164u, RdnaOpcode::VAddF64, 0xf0u, false);
        CheckIgnoredModifiers("v_add_f64 with ieee mode on", 0x164u, RdnaOpcode::VAddF64, 0x40u, true);
        CheckIgnoredModifiers("v_fma_f16 with denormals kept", 0x34bu, RdnaOpcode::VFmaF16, 0xf0u, false, true);
        CheckIgnoredModifiers("v_fma_f64 with denormals kept", 0x14cu, RdnaOpcode::VFmaF64, 0xf0u, false, true);
        CheckIgnoredModifiers("v_div_fixup_f16 with denormals kept", 0x35fu, RdnaOpcode::VDivFixupF16, 0xf0u, false, true);

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
