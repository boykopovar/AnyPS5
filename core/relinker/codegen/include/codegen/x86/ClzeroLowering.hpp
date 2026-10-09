#ifndef CODEGEN_X86_CLZEROLOWERING_HPP
#define CODEGEN_X86_CLZEROLOWERING_HPP

#include <codegen/x86/ClzeroOperands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>

namespace Codegen {

class ClzeroLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const ClzeroOperands& operands) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(const ClzeroOperands& operands) const;
};

}

#endif
