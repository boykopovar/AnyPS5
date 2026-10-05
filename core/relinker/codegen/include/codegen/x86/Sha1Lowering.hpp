#ifndef CODEGEN_X86_SHA1LOWERING_HPP
#define CODEGEN_X86_SHA1LOWERING_HPP

#include <codegen/x86/Sha1Operands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <span>

namespace Codegen {

class Sha1Lowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const Sha1Operands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const Sha1Operands& operands, std::span<const std::uint8_t> trailing = {}) const;
};

}

#endif
