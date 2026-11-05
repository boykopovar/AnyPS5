#include <codegen/x86/WideStoreLowering.hpp>

namespace Codegen {

namespace {

void _emitHalf(StubBodyBuilder& body, const WideStoreHalf& half) {
    if (half.RipDisplacementOffset)
        body.RipRelative(half.Bytes, *half.RipDisplacementOffset, half.RipTarget);
    else
        body.Raw(half.Bytes);
}

}

void WideStoreLowering::EmitOutOfLine(StubBodyBuilder& body, const WideStoreOperands& operands) const {
    _emitHalf(body, operands.Low);
    _emitHalf(body, operands.High);
}

}
