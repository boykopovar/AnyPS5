#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
        std::abort();
    }
}

static std::array<std::uint32_t, 2> MakeSmemWords(std::uint32_t opcode, std::uint32_t sdst, std::uint32_t sbase, std::uint32_t soffset = 0u, std::uint32_t offset = 0u, std::uint32_t glc = 0u) {
    const std::uint32_t word0 = (0x3du << 26u) | ((opcode & 0xffu) << 18u) | ((glc & 1u) << 16u) | ((sdst & 0x7fu) << 6u) | (sbase & 0x3fu);
    const std::uint32_t word1 = ((soffset & 0x7fu) << 25u) | (offset & 0x1fffffu);
    return {word0, word1};
}

static void CheckNullDestinationLoads() {
    constexpr std::uint32_t opcodes[] = {
        0x00u,
        0x01u,
        0x02u,
        0x03u,
        0x04u,
        0x08u,
        0x09u,
        0x0au,
        0x0bu,
        0x0cu,
    };
    for (std::uint32_t op : opcodes) {
        const auto words = MakeSmemWords(op, 125u, 0u);
        const RdnaInstruction instruction = DecodeRdnaSmem(0x1000u, words, 0u);
        Require(instruction.family == RdnaInstructionFamily::SMEM, "SMEM family");
        Require(instruction.destination.kind == RdnaOperandKind::Null, "Null destination kind");

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        context.TranslateInstruction(instruction);

        for (auto* val : block.Instructions()) {
            const IrOpcode code = val->Opcode();
            Require(code != IrOpcode::SetScalarRegister, "No SetScalarRegister for Null destination");
            Require(code != IrOpcode::SetVccLo, "No SetVccLo for Null destination");
            Require(code != IrOpcode::SetVccHi, "No SetVccHi for Null destination");
            Require(code != IrOpcode::SetM0, "No SetM0 for Null destination");
            Require(code != IrOpcode::SetExecLo, "No SetExecLo for Null destination");
            Require(code != IrOpcode::SetExecHi, "No SetExecHi for Null destination");
        }
    }
}

static void CheckInvalidDestinationsThrow() {
    constexpr std::uint32_t invalidDsts[] = {
        124u,
        126u,
        127u,
    };
    for (std::uint32_t dst : invalidDsts) {
        const auto words = MakeSmemWords(0x00u, dst, 0u);
        const RdnaInstruction instruction = DecodeRdnaSmem(0x2000u, words, 0u);
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);

        bool threw = false;
        try {
            context.TranslateInstruction(instruction);
        } catch (const std::runtime_error& err) {
            threw = std::string(err.what()).find("invalid scalar-memory destination") != std::string::npos;
        }
        Require(threw, "Invalid destination must throw in scalar load");
    }
}

static void CheckInvalidBaseOverflow() {
    const auto words = MakeSmemWords(0x00u, 125u, 63u);
    bool threw = false;
    try {
        (void)DecodeRdnaSmem(0x3000u, words, 0u);
    } catch (const std::runtime_error& err) {
        threw = std::string(err.what()).find("SMEM base register range overflow") != std::string::npos;
    }
    Require(threw, "sbase > 104 must throw in decoder");
}

static void CheckValidDestinationsLowerWrites() {
    {
        const auto words = MakeSmemWords(0x00u, 4u, 0u);
        const RdnaInstruction instruction = DecodeRdnaSmem(0x4000u, words, 0u);
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(instruction);

        bool hasSetScalarReg = false;
        for (auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::SetScalarRegister) {
                hasSetScalarReg = true;
                break;
            }
        }
        Require(hasSetScalarReg, "s_load_dword s4 emits SetScalarRegister");
    }
    {
        const auto words = MakeSmemWords(0x00u, 106u, 0u);
        const RdnaInstruction instruction = DecodeRdnaSmem(0x4000u, words, 0u);
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(instruction);

        bool hasSetVccLo = false;
        for (auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::SetVccLo) {
                hasSetVccLo = true;
                break;
            }
        }
        Require(hasSetVccLo, "s_load_dword vcc_lo emits SetVccLo");
    }
}

int main() {
    CheckNullDestinationLoads();
    CheckInvalidDestinationsThrow();
    CheckInvalidBaseOverflow();
    CheckValidDestinationsLowerWrites();
    return 0;
}
