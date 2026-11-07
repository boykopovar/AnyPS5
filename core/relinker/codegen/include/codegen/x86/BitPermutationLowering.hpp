#ifndef CODEGEN_X86_BITPERMUTATIONLOWERING_HPP
#define CODEGEN_X86_BITPERMUTATIONLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

struct BitPermutationOperands {
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Mask;
    bool Wide;
    bool Deposit;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<BitPermutationOperands> DecodeBitPermutation(const std::uint8_t* data, std::size_t length);
[[nodiscard]] bool CanLowerBitPermutation(const BitPermutationOperands& operands);
void EmitBitPermutation(StubBodyBuilder& body, const BitPermutationOperands& operands);

}

#endif
