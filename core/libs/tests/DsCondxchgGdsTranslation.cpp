#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("ds_condxchg32: " + message);
}

void CheckTranslation(bool gds) {
    constexpr std::uint32_t destination = 4u;
    constexpr std::uint32_t addressReg = 2u;
    constexpr std::uint32_t dataReg = 8u;
    constexpr std::uint32_t offset0 = 8u;
    constexpr std::uint32_t offset1 = 0u;
    constexpr std::uint32_t combinedOffset = offset0 | (offset1 << 8u);
    constexpr std::uint32_t opcode = 0x7eu;

    const std::array<std::uint32_t, 2> code{
        (0x36u << 26u) | (opcode << 18u) | (static_cast<std::uint32_t>(gds) << 17u) | (offset1 << 8u) | offset0,
        (destination << 24u) | (dataReg << 8u) | addressReg
    };

    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsCondxchg32RtnB64, "instruction does not decode to DsCondxchg32RtnB64");
    Require(instruction.gds == gds, "instruction gds flag mismatch");
    Require(instruction.destination.reg == destination, "destination register index mismatch");
    Require(instruction.source0.reg == addressReg, "address register index mismatch");
    Require(instruction.source1.reg == dataReg, "data register index mismatch");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    std::vector<const IrValue*> atomicOperations;
    const IrValue* writeLow = nullptr;
    const IrValue* writeHigh = nullptr;

    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::SharedAtomicMskor32) {
            atomicOperations.push_back(value);
        }
        if (value->Opcode() == IrOpcode::SetVectorRegister) {
            if (value->Argument(0)->Register().index == destination) {
                writeLow = value;
            } else if (value->Argument(0)->Register().index == destination + 1u) {
                writeHigh = value;
            }
        }
    }

    Require(atomicOperations.size() == 2u, "expected two SharedAtomicMskor32 operations");
    Require(writeLow != nullptr, "low destination register was not written");
    Require(writeHigh != nullptr, "high destination register was not written");

    const ResourceKind expectedKind = gds ? ResourceKind::Gds : ResourceKind::Lds;
    for (std::size_t i = 0u; i < 2u; ++i) {
        const auto memoryIndex = atomicOperations[i]->Flags<MemoryFlags>().index;
        Require(memoryIndex < program.Resources().memoryInfo.size(), "memory index out of bounds");
        const auto& memory = program.Resources().memoryInfo[memoryIndex];
        Require(memory.kind == expectedKind, "resource kind mismatch for atomic operation");
        Require(memory.offset == combinedOffset + static_cast<std::uint32_t>(i * 4u), "memory offset mismatch for atomic operation");
    }
}

}

int main() {
    CheckTranslation(false);
    CheckTranslation(true);
    return 0;
}
