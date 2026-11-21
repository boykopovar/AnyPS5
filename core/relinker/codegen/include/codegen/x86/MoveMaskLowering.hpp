#ifndef CODEGEN_X86_MOVEMASKLOWERING_HPP
#define CODEGEN_X86_MOVEMASKLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>

namespace Codegen {

struct MoveMaskOperands {
    std::uint8_t Destination;
    std::uint8_t Source;
};

[[nodiscard]] std::optional<MoveMaskOperands> DecodeMoveMask(const std::uint8_t* data, std::size_t length);
void EmitMoveMask(StubBodyBuilder& body, const MoveMaskOperands& operands);

}

#endif
