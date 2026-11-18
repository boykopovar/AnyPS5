#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("s_atomic: " + message);
}

constexpr std::uint32_t DataReg = 7u;
constexpr std::uint32_t Sbase = 8u;
constexpr std::uint32_t Soffset = 20u;
constexpr std::uint32_t Offset = 64u;

std::array<std::uint32_t, 2> Encode(std::uint32_t opcode, bool glc) {
    const std::uint32_t word0 = (0x3du << 26u) | (opcode << 18u) | (1u << 17u) |
        (static_cast<std::uint32_t>(glc) << 16u) | (DataReg << 6u) | Sbase;
    const std::uint32_t word1 = (Soffset << 25u) | Offset;
    return {word0, word1};
}

struct Form {
    std::uint32_t encoding;
    RdnaOpcode decoded;
    IrOpcode ir;
    const char* name;
};

constexpr Form Forms[]{
    {0x87u, RdnaOpcode::SAtomicUmax, IrOpcode::AddressAtomicUMax32, "s_atomic_umax"},
    {0x88u, RdnaOpcode::SAtomicAnd, IrOpcode::AddressAtomicAnd32, "s_atomic_and"},
    {0x89u, RdnaOpcode::SAtomicOr, IrOpcode::AddressAtomicOr32, "s_atomic_or"},
    {0x8au, RdnaOpcode::SAtomicXor, IrOpcode::AddressAtomicXor32, "s_atomic_xor"},
    {0x8bu, RdnaOpcode::SAtomicInc, IrOpcode::AddressAtomicInc32, "s_atomic_inc"},
    {0x8cu, RdnaOpcode::SAtomicDec, IrOpcode::AddressAtomicDec32, "s_atomic_dec"},
};

void Check(const Form& form, bool glc) {
    const auto code = Encode(form.encoding, glc);
    const RdnaInstruction instruction = DecodeRdnaSmem(0u, code, 0u);
    Require(instruction.op == form.decoded, std::string(form.name) + " decoded to the wrong opcode");
    Require(instruction.glc == glc, std::string(form.name) + " glc flag mismatch");
    Require(instruction.destination.reg == DataReg, std::string(form.name) + " data register mismatch");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* atomic = nullptr;
    const IrValue* write = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == form.ir) atomic = value;
        if (value->Opcode() == IrOpcode::SetVectorRegister && value->Argument(0)->Register().index == DataReg) write = value;
    }
    Require(atomic != nullptr, std::string(form.name) + " did not emit its atomic");
    if (glc) {
        Require(write != nullptr, std::string(form.name) + " did not write the returned value");
    } else {
        Require(write == nullptr, std::string(form.name) + " wrote a value without glc");
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
