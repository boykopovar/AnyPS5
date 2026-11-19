#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("buffer d16 previous value: " + message);
}

void CheckHalfword(const IrValue& halfword, std::uint32_t registerIndex, bool highHalf) {
    const std::string name = "V" + std::to_string(registerIndex) + (highHalf ? " high half" : " low half");
    Require(halfword.ArgumentCount() == 2u, name + " is not a two-operand expression");
    Require(halfword.Argument(0)->Opcode() == IrOpcode::GetVectorRegister && halfword.Argument(0)->Argument(0)->Register().index == registerIndex,
        name + " does not read V" + std::to_string(registerIndex));
    if (highHalf) {
        Require(halfword.Opcode() == IrOpcode::ShiftRightLogical32 && halfword.Argument(1)->HasImmediate() && halfword.Argument(1)->ImmediateU32() == 16u,
            name + " is not the high half of V" + std::to_string(registerIndex));
    } else {
        Require(halfword.Opcode() == IrOpcode::BitwiseAnd32 && halfword.Argument(1)->HasImmediate() && halfword.Argument(1)->ImmediateU32() == 0xffffu,
            name + " is not the low half of V" + std::to_string(registerIndex));
    }
}

void CheckTranslation(std::uint32_t opcode, std::uint32_t components, std::uint32_t destination) {
    const std::array<std::uint32_t, 2> code{
        (0x38u << 26u) | (opcode << 18u) | (1u << 12u),
        (10u << 24u) | (4u << 16u) | (destination << 8u) | 2u
    };
    const RdnaInstruction instruction = DecodeRdnaMubuf(0u, code, 0u);
    Require(instruction.destination.reg == destination, "the destination register does not decode");

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.TranslateInstruction(instruction);

    const IrValue* load = nullptr;
    for (const auto* value : block.Instructions()) {
        if (value->Opcode() == IrOpcode::LoadBufferU32 || value->Opcode() == IrOpcode::LoadBufferU32x2 ||
            value->Opcode() == IrOpcode::LoadBufferU32x3 || value->Opcode() == IrOpcode::LoadBufferU32x4) {
            load = value;
        }
    }
    Require(load != nullptr, "the load was not emitted");
    Require(load->ArgumentCount() == 6u, "the load does not carry a previous-value operand");
    const IrValue& previous = *load->Argument(4u);
    Require(previous.ArgumentCount() == components, "the previous value does not hold one halfword per loaded component");
    for (std::uint32_t half = 0u; half < components; ++half) {
        CheckHalfword(*previous.Argument(half), destination + half / 2u, half % 2u != 0u);
    }
}

}

int main() {
    CheckTranslation(0x81u, 2u, 5u);
    CheckTranslation(0x82u, 3u, 5u);
    return 0;
}
