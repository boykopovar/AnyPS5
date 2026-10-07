#ifndef CODEGEN_X86_SHA256LOWERING_HPP
#define CODEGEN_X86_SHA256LOWERING_HPP

#include <codegen/x86/Sha256Operands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <span>

namespace Codegen {

class Sha256Lowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const Sha256Operands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const Sha256Operands& operands,
                                             std::span<const std::uint8_t> trailing = {}) const;
};

}

#endif
