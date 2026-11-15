#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("global atomic csub: " + message);
}

void CheckTranslation(bool glc) {
    constexpr std::uint32_t destination = 5u;
    constexpr std::uint32_t opcode = 0x34u;
    constexpr std::uint32_t seg = 2u;
    const std::array<std::uint32_t, 2> code{
        (0x37u << 26u) | (opcode << 18u) | (static_cast<std::uint32_t>(glc) << 16u) | (seg << 14u),
        (destination << 24u) | (0x7fu << 16u) | (6u << 8u) | 10u
    };
    const RdnaInstruction instruction = DecodeRdnaFlat(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::GlobalAtomicCsub, "instruction does not decode to GlobalAtomicCsub");
    Require(instruction.glc == glc, "instruction glc flag mismatch");
    if (glc) {
        Require(instruction.destination.reg == destination, "destination register index mismatch");
    } else {
        Require(instruction.destination.kind == RdnaOperandKind::None, "expected no destination when glc is clear");
    }

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* atomicOperation = nullptr;
    const IrValue* writeOperation = nullptr;

    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::AddressAtomicUSubSat32) {
            atomicOperation = value;
        }
        if (value->Opcode() == IrOpcode::SetVectorRegister && value->Argument(0)->Register().index == destination) {
            writeOperation = value;
        }
    }

    Require(atomicOperation != nullptr, "AddressAtomicUSubSat32 was not emitted");
    if (glc) {
        Require(writeOperation != nullptr, "destination register was not written when glc is set");
    } else {
        Require(writeOperation == nullptr, "destination register was written when glc is clear");
    }
}

}

int main() {
    CheckTranslation(false);
    CheckTranslation(true);
    return 0;
}
