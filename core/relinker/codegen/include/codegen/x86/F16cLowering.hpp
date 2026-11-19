#ifndef CODEGEN_X86_F16CLOWERING_HPP
#define CODEGEN_X86_F16CLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

enum class F16cOperation : std::uint8_t {
    HalfToSingle,
    SingleToHalf,
};

struct F16cOperands {
    const char* Name;
    F16cOperation Operation;
    bool Vector256;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Immediate;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<F16cOperands> DecodeF16c(const std::uint8_t* data, std::size_t length);
void EmitF16c(StubBodyBuilder& body, const F16cOperands& operands);

}

#endif
