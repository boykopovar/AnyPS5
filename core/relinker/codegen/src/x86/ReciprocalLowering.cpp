#include <codegen/x86/ReciprocalLowering.hpp>
#include <array>
#include <cstring>

namespace Codegen {

namespace {

constexpr std::uint8_t kSqrtps = 0x51;
constexpr std::uint8_t kDivps = 0x5E;
constexpr std::uint8_t kMovaps = 0x28;
constexpr float kOne = 1.0f;

StubConstant _ones() {
    StubConstant constant{};
    for (std::size_t lane = 0; lane < 4; ++lane)
        std::memcpy(constant.data() + lane * sizeof(kOne), &kOne, sizeof(kOne));
    return constant;
}

std::uint8_t _scratch(const ReciprocalOperands& operands) {
    std::uint8_t reg = 0;
    while (reg == operands.Destination || reg == operands.Source)
        ++reg;
    return reg;
}

void _vexMovaps(StubBodyBuilder& body, const std::uint8_t dst, const std::uint8_t src) {
    const std::array<std::uint8_t, 5> bytes{
        0xC4,
        static_cast<std::uint8_t>(((dst & 8) != 0 ? 0x00 : 0x80) | 0x40 | ((src & 8) != 0 ? 0x00 : 0x20) | 0x01),
        0x78,
        kMovaps,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))
    };
    body.Raw(bytes);
}

}

void ReciprocalLowering::EmitOutOfLine(StubBodyBuilder& body, const ReciprocalOperands& operands) const {
    const auto scratch = _scratch(operands);
    const auto dst = operands.Destination;
    body.Spill(scratch);
    if (operands.Operation == ReciprocalOperation::ReciprocalSquareRoot) {
        body.SsePlain({0x0F, kSqrtps}, scratch, operands.Source);
        body.RipOperand({0x0F, kMovaps}, dst, _ones());
        body.SsePlain({0x0F, kDivps}, dst, scratch);
        _vexMovaps(body, dst, dst);
    } else {
        body.RipOperand({0x0F, kMovaps}, scratch, _ones());
        body.SsePlain({0x0F, kDivps}, scratch, operands.Source);
        _vexMovaps(body, dst, scratch);
    }
    body.Restore(scratch);
}

LoweredBody ReciprocalLowering::LowerOutOfLine(const ReciprocalOperands& operands, std::span<const std::uint8_t> trailing) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    body.Raw(trailing);
    return body.Finish();
}

}
