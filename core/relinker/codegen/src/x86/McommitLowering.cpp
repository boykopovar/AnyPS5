#include <codegen/x86/McommitLowering.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <array>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

constexpr std::array<std::uint8_t, 3> kSfence = {0x0F, 0xAE, 0xF8};
constexpr std::array<std::uint8_t, 1> kPushfq = {0x9C};
constexpr std::array<std::uint8_t, 7> kAndFlagsArithmetic = {0x81, 0x24, 0x24, 0x2A, 0xF7, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 4> kOrFlagsCarry = {0x83, 0x0C, 0x24, 0x01};
constexpr std::array<std::uint8_t, 1> kPopfq = {0x9D};

}

void McommitLowering::EmitOutOfLine(StubBodyBuilder& body) const {
    body.Raw(std::span<const std::uint8_t>(kLeaRspBelowRedZone.Bytes, kLeaRspBelowRedZone.Size));
    body.Raw(kSfence);
    body.Raw(kPushfq);
    body.Raw(kAndFlagsArithmetic);
    body.Raw(kOrFlagsCarry);
    body.Raw(kPopfq);
    body.Raw(std::span<const std::uint8_t>(kLeaRspRestore.Bytes, kLeaRspRestore.Size));
}

LoweredBody McommitLowering::LowerOutOfLine() const {
    StubBodyBuilder body;
    EmitOutOfLine(body);
    return body.Finish();
}

}
