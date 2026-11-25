#include <codegen/x86/WideStoreLowering.hpp>
#include <array>

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
    if (!operands.AddressCalculation.empty()) {
        // Preserve the caller's red zone and RAX while materializing the original effective address.
        constexpr std::array<std::uint8_t, 6> enterFrame = {0x48, 0x8D, 0x64, 0x24, 0x80, 0x50};
        constexpr std::array<std::uint8_t, 9> leaveFrame = {0x58, 0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00};
        body.Raw(enterFrame);
        body.Raw(operands.AddressCalculation);
        _emitHalf(body, operands.Low);
        _emitHalf(body, operands.High);
        body.Raw(leaveFrame);
        return;
    }
    _emitHalf(body, operands.Low);
    _emitHalf(body, operands.High);
}

}
