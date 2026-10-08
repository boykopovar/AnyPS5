#ifndef CODEGEN_X86_MCOMMITLOWERING_HPP
#define CODEGEN_X86_MCOMMITLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>

namespace Codegen {

class McommitLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body) const;
    [[nodiscard]] LoweredBody LowerOutOfLine() const;
};

}

#endif
