#ifndef CODEGEN_X86_MULXLOWERING_HPP
#define CODEGEN_X86_MULXLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

struct MulxOperands {
    std::uint8_t HighDestination;
    std::uint8_t LowDestination;
    std::uint8_t Source;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<MulxOperands> DecodeMulx(const std::uint8_t* data, std::size_t length);
void EmitMulx(StubBodyBuilder& body, const MulxOperands& operands);

}

#endif
