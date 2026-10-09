#ifndef CODEGEN_X86_RECIPROCALLOWERING_HPP
#define CODEGEN_X86_RECIPROCALLOWERING_HPP

#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>

namespace Codegen {

class ReciprocalLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const ReciprocalOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const ReciprocalOperands& operands) const;
};

}

#endif
