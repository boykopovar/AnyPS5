#ifndef CODEGEN_X86_WIDESTORELOWERING_HPP
#define CODEGEN_X86_WIDESTORELOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/WideStoreOperands.hpp>

namespace Codegen {

class WideStoreLowering {
public:
    void EmitOutOfLine(StubBodyBuilder& body, const WideStoreOperands& operands) const;
};

}

#endif
