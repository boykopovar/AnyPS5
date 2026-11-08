#ifndef CODEGEN_X86_PERMUTELOWERING_HPP
#define CODEGEN_X86_PERMUTELOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

struct PermuteOperands {
    const char* Name;
    bool Variable;
    std::uint8_t Destination;
    std::uint8_t Index;
    std::uint8_t Source;
    std::uint8_t Immediate;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] std::optional<PermuteOperands> DecodePermute(const std::uint8_t* data, std::size_t length);
void EmitPermute(StubBodyBuilder& body, const PermuteOperands& operands);

}

#endif
