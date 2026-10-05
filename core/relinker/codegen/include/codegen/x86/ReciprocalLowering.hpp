#ifndef CODEGEN_X86_RECIPROCALLOWERING_HPP
#define CODEGEN_X86_RECIPROCALLOWERING_HPP

#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <span>

namespace Codegen {

class ReciprocalLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const ReciprocalOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const ReciprocalOperands& operands, std::span<const std::uint8_t> trailing = {}) const;
};

}

#endif
