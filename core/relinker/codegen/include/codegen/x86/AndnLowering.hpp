#ifndef CODEGEN_X86_ANDNLOWERING_HPP
#define CODEGEN_X86_ANDNLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

struct AndnOperands {
    std::uint8_t Destination;
    std::uint8_t InvertedSource;
    std::uint8_t Source;
    bool Wide;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<AndnOperands> DecodeAndn(const std::uint8_t* data, std::size_t length);
void EmitAndn(StubBodyBuilder& body, const AndnOperands& operands);

}

#endif
