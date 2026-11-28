#ifndef CODEGEN_X86_MOVBELOWERING_HPP
#define CODEGEN_X86_MOVBELOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <optional>

namespace Codegen {

struct MovbeOperands {
    bool Store;
    std::uint8_t Register;
    std::uint8_t Width;
    MemoryOperand Memory;
};

[[nodiscard]] std::optional<MovbeOperands> DecodeMovbe(const std::uint8_t* data, std::size_t length);
void EmitMovbe(StubBodyBuilder& body, const MovbeOperands& operands);

}

#endif
