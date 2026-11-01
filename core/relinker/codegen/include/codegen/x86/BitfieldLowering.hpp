#ifndef CODEGEN_X86_BITFIELDLOWERING_HPP
#define CODEGEN_X86_BITFIELDLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class BitfieldOperation : std::uint8_t {
    ZeroHigh,
    Extract,
};

struct BitfieldOperands {
    BitfieldOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Control;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* BitfieldName(BitfieldOperation operation);
[[nodiscard]] std::optional<BitfieldOperands> DecodeBitfield(const std::uint8_t* data, std::size_t length);
void EmitBitfield(StubBodyBuilder& body, const BitfieldOperands& operands);

}

#endif
