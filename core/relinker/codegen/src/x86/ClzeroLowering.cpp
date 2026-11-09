#include <codegen/x86/ClzeroLowering.hpp>
#include <array>

namespace Codegen {

namespace {

constexpr std::uint8_t kPrefixPacked = 0x66;
constexpr std::uint8_t kScratch = 0;
constexpr std::uint64_t kLineMask = ~std::uint64_t{63};
constexpr std::array<std::uint8_t, 1> kPushRcx = {0x51};
constexpr std::array<std::uint8_t, 1> kPopRcx = {0x59};
constexpr std::array<std::uint8_t, 5> kMovqXmm0Rax = {0x66, 0x48, 0x0F, 0x6E, 0xC0};
constexpr std::array<std::uint8_t, 4> kMovdXmm0Eax = {0x66, 0x0F, 0x6E, 0xC0};
constexpr std::array<std::uint8_t, 5> kMovqRcxXmm0 = {0x66, 0x48, 0x0F, 0x7E, 0xC1};
constexpr std::array<std::uint8_t, 4> kMovntdqRcx = {0x66, 0x0F, 0xE7, 0x01};
constexpr std::array<std::uint8_t, 5> kMovntdqRcx16 = {0x66, 0x0F, 0xE7, 0x41, 0x10};
constexpr std::array<std::uint8_t, 5> kMovntdqRcx32 = {0x66, 0x0F, 0xE7, 0x41, 0x20};
constexpr std::array<std::uint8_t, 5> kMovntdqRcx48 = {0x66, 0x0F, 0xE7, 0x41, 0x30};

}

void ClzeroLowering::EmitOutOfLine(StubBodyBuilder& body, const ClzeroOperands& operands) const {
    StubConstant lineMask{};
    for (std::size_t byte = 0; byte < 8; ++byte)
        lineMask[byte] = static_cast<std::uint8_t>(kLineMask >> (byte * 8));
    body.Spill(kScratch);
    body.Raw(kPushRcx);
    if (operands.AddressSize32)
        body.Raw(kMovdXmm0Eax);
    else
        body.Raw(kMovqXmm0Rax);
    body.RipOperand({0x0F, 0xDB}, kScratch, lineMask);
    body.Raw(kMovqRcxXmm0);
    body.Sse(kPrefixPacked, {0x0F, 0xEF}, kScratch, kScratch);
    body.Raw(kMovntdqRcx);
    body.Raw(kMovntdqRcx16);
    body.Raw(kMovntdqRcx32);
    body.Raw(kMovntdqRcx48);
    body.Raw(kPopRcx);
    body.Restore(kScratch);
}

LoweredBody ClzeroLowering::LowerOutOfLine(const ClzeroOperands& operands) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    return body.Finish();
}

}
