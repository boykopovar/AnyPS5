#ifndef CODEGEN_X86_COUNTLOWERING_HPP
#define CODEGEN_X86_COUNTLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

enum class CountOperation : std::uint8_t {
    TrailingZeros,
    LeadingZeros,
};

struct CountOperands {
    CountOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Width;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] const char* CountName(CountOperation operation);
[[nodiscard]] std::optional<CountOperands> DecodeCount(const std::uint8_t* data, std::size_t length);
void EmitCount(StubBodyBuilder& body, const CountOperands& operands);

}

#endif
