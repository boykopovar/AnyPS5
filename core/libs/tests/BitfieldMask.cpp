#include "Translation/TranslationContext.hpp"
#include "Optimization/ConstantFolder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

std::uint32_t expectedMask(std::uint32_t width, std::uint32_t offset) {
    width &= 31u;
    offset &= 31u;
    std::uint32_t mask = 0u;
    for (std::uint32_t bit = offset; bit < 32u && bit - offset < width; ++bit) {
        mask |= 1u << bit;
    }
    return mask;
}

void check(RdnaOpcode opcode, std::uint32_t width, std::uint32_t offset) {
    const bool scalar = opcode == RdnaOpcode::SBfmB32;
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    RdnaInstruction instruction{};
    instruction.op = opcode;
    instruction.family = scalar ? RdnaInstructionFamily::SOP2 : RdnaInstructionFamily::VOP3;
    instruction.sourceCount = 2u;
    instruction.source0.kind = RdnaOperandKind::LiteralConstant;
    instruction.source0.value = width;
    instruction.source1.kind = RdnaOperandKind::LiteralConstant;
    instruction.source1.value = offset;
    instruction.destination.kind = scalar ? RdnaOperandKind::ScalarRegister : RdnaOperandKind::VectorRegister;
    instruction.destination.reg = 10u;
    TranslationContext context(program, block, 256u);
    context.TranslateInstruction(instruction);
    ConstantFolder{}.Fold(program);
    const IrValue* result = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SetScc) {
            throw std::runtime_error("bitfield mask changed SCC");
        }
        if (value->Opcode() != (scalar ? IrOpcode::SetScalarRegister : IrOpcode::SetVectorRegister)) continue;
        result = value->Argument(1u)->Resolve();
    }
    if (result == nullptr) throw std::runtime_error("bitfield mask did not write its destination");
    if (!scalar) {
        if (result->Opcode() != IrOpcode::SelectU32) throw std::runtime_error("bitfield mask lost EXEC masking");
        result = result->Argument(1u)->Resolve();
    }
    if (!result->HasImmediate() || result->ImmediateU32() != expectedMask(width, offset)) {
        throw std::runtime_error(std::string(scalar ? "s_bfm_b32" : "v_bfm_b32") +
            " width=" + std::to_string(width) + " offset=" + std::to_string(offset) + " did not fold to the expected mask");
    }
}

}

int main() {
    try {
        const std::array<std::uint32_t, 4> upperBits{0u, 32u, 0x80000000u, 0xffffffe0u};
        for (const auto opcode : {RdnaOpcode::SBfmB32, RdnaOpcode::VBfmB32}) {
            for (std::uint32_t width = 0u; width < 32u; ++width) {
                for (std::uint32_t offset = 0u; offset < 32u; ++offset) {
                    for (const auto widthBits : upperBits) {
                        for (const auto offsetBits : upperBits) {
                            check(opcode, width | widthBits, offset | offsetBits);
                        }
                    }
                }
            }
        }
        std::puts("32768 bitfield mask translation cases passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
