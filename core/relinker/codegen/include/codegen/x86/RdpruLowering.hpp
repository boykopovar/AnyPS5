#ifndef CODEGEN_X86_RDPRULOWERING_HPP
#define CODEGEN_X86_RDPRULOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>

namespace Codegen {

class RdpruLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body) const;
    [[nodiscard]] LoweredBody LowerOutOfLine() const;
};

}

#endif
