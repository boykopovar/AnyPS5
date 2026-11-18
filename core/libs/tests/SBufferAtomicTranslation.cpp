#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("s_buffer atomic: " + message);
}

struct Form {
    std::uint32_t opcode;
    RdnaOpcode decoded;
    IrOpcode translated;
};

constexpr std::array<Form, 7u> Forms{{
    {0x40u, RdnaOpcode::SBufferAtomicSwap, IrOpcode::BufferAtomicSwap32},
    {0x41u, RdnaOpcode::SBufferAtomicCmpswap, IrOpcode::BufferAtomicCmpSwap32},
    {0x42u, RdnaOpcode::SBufferAtomicAdd, IrOpcode::BufferAtomicIAdd32},
    {0x43u, RdnaOpcode::SBufferAtomicSub, IrOpcode::BufferAtomicISub32},
    {0x44u, RdnaOpcode::SBufferAtomicSmin, IrOpcode::BufferAtomicSMin32},
    {0x45u, RdnaOpcode::SBufferAtomicUmin, IrOpcode::BufferAtomicUMin32},
    {0x46u, RdnaOpcode::SBufferAtomicSmax, IrOpcode::BufferAtomicSMax32},
}};

constexpr std::uint32_t Descriptor = 4u;
constexpr std::uint32_t Data = 6u;

RdnaInstruction Decode(std::uint32_t opcode, bool glc) {
    const std::array<std::uint32_t, 2u> code{
        (0x3du << 26u) | (opcode << 18u) | (static_cast<std::uint32_t>(glc) << 16u) | (Data << 6u) | (Descriptor / 2u),
        (125u << 25u) | 0x10u
    };
    return DecodeRdnaSmem(0u, code, 0u);
}

void Check(const Form& form, bool glc) {
    const RdnaInstruction instruction = Decode(form.opcode, glc);
    Require(instruction.op == form.decoded, "instruction did not decode to the expected opcode");
    Require(instruction.family == RdnaInstructionFamily::SMEM, "instruction is not SMEM");
    Require(instruction.glc == glc, "glc flag mismatch");
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister, "data register is not scalar");
    Require(instruction.destination.reg == Data, "data register index mismatch");
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister, "descriptor base is not scalar");
    Require(instruction.source0.reg == Descriptor, "descriptor base register mismatch");
    Require(instruction.memoryOffset == 0x10u, "offset mismatch");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* atomic = nullptr;
    bool wroteData = false;
    bool readComparator = false;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == form.translated) {
            atomic = value;
        }
        if (value->Opcode() == IrOpcode::SetScalarRegister && value->Argument(0u)->Register().index == Data) {
            wroteData = true;
        }
        if (value->Opcode() == IrOpcode::GetScalarRegister && value->Argument(0u)->Register().index == Data + 1u) {
            readComparator = true;
        }
    }
    Require(atomic != nullptr, "expected buffer atomic operation was not emitted");
    if (glc) {
        Require(wroteData, "data register was not written when glc is set");
    } else {
        Require(!wroteData, "data register was written when glc is clear");
    }
    if (form.opcode == 0x41u) {
        Require(readComparator, "cmpswap did not read the comparator from the second data register");
    }
}

}

int main() {
    for (const auto& form : Forms) {
        Check(form, false);
        Check(form, true);
    }
    return 0;
}
