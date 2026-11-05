#ifndef CODEGEN_X86_SSE4ALOWERING_HPP
#define CODEGEN_X86_SSE4ALOWERING_HPP

#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Codegen {

class Sse4aLowering {
public:
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> LowerInPlace(const Sse4aOperands& operands, std::size_t originalLength) const;
    void EmitOutOfLine(StubBodyBuilder& body, const Sse4aOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const Sse4aOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(std::span<const Sse4aOperands> sequence) const;
};

}

#endif
