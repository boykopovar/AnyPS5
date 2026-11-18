#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("scalar atomic: " + message);
}

struct Case {
    const char* name;
    std::uint32_t opcode;
    RdnaOpcode rdna;
    IrOpcode ir;
};

constexpr std::array<Case, 7> Cases{{
    {"swap", 0x80u, RdnaOpcode::SAtomicSwap, IrOpcode::AddressAtomicSwap32},
    {"cmpswap", 0x81u, RdnaOpcode::SAtomicCmpswap, IrOpcode::AddressAtomicCmpSwap32},
    {"add", 0x82u, RdnaOpcode::SAtomicAdd, IrOpcode::AddressAtomicIAdd32},
    {"sub", 0x83u, RdnaOpcode::SAtomicSub, IrOpcode::AddressAtomicISub32},
    {"smin", 0x84u, RdnaOpcode::SAtomicSmin, IrOpcode::AddressAtomicSMin32},
    {"umin", 0x85u, RdnaOpcode::SAtomicUmin, IrOpcode::AddressAtomicUMin32},
    {"smax", 0x86u, RdnaOpcode::SAtomicSmax, IrOpcode::AddressAtomicSMax32},
}};

void Check(const Case& test, bool glc) {
    constexpr std::uint32_t sdst = 10u;
    constexpr std::uint32_t sbase = 1u;
    const std::array<std::uint32_t, 2> code{
        (0x3du << 26u) | (test.opcode << 18u) | (static_cast<std::uint32_t>(glc) << 16u) | (sdst << 6u) | sbase,
        0x00000000u
    };
    const RdnaInstruction instruction = DecodeRdnaSmem(0u, code, 0u);
    Require(instruction.op == test.rdna, std::string(test.name) + " did not decode to the expected opcode");
    Require(instruction.opcodeId == test.opcode, std::string(test.name) + " opcode id mismatch");
    Require(instruction.glc == glc, std::string(test.name) + " glc flag mismatch");
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister && instruction.source0.reg == sbase * 2u, std::string(test.name) + " base register pair mismatch");
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister && instruction.destination.reg == sdst, std::string(test.name) + " data register mismatch");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 32);
    context.TranslateInstruction(instruction);

    const IrValue* atomicOperation = nullptr;
    const IrValue* writeOperation = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == test.ir) atomicOperation = value;
        if (value->Opcode() == IrOpcode::SetScalarRegister && value->Argument(0) != nullptr && value->Argument(0)->Register().index == sdst) {
            writeOperation = value;
        }
    }
    Require(atomicOperation != nullptr, std::string(test.name) + " did not emit the expected address atomic");
    const std::uint32_t expectedArguments = test.ir == IrOpcode::AddressAtomicCmpSwap32 ? 6u : 5u;
    Require(atomicOperation->ArgumentCount() == expectedArguments, std::string(test.name) + " emitted an address atomic with the wrong operand count");
    if (glc) {
        Require(writeOperation != nullptr, std::string(test.name) + " did not write the returned value when glc is set");
    } else {
        Require(writeOperation == nullptr, std::string(test.name) + " wrote the data register when glc is clear");
    }
    ValidateProgram(program, false);
}

}

int main() {
    for (const auto& test : Cases) {
        Check(test, false);
        Check(test, true);
    }
    return 0;
}
