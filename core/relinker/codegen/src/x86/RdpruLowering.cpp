#include <codegen/x86/RdpruLowering.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <array>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

constexpr std::array<std::uint8_t, 1> kPushfq = {0x9C};
constexpr std::array<std::uint8_t, 1> kPopfq = {0x9D};
constexpr std::array<std::uint8_t, 3> kCmpEcxOne = {0x83, 0xF9, 0x01};
constexpr std::array<std::uint8_t, 2> kJaUnsupported = {0x77, 0x04};
constexpr std::array<std::uint8_t, 2> kRdtsc = {0x0F, 0x31};
constexpr std::array<std::uint8_t, 2> kJmpRestore = {0xEB, 0x04};
constexpr std::array<std::uint8_t, 2> kZeroEax = {0x31, 0xC0};
constexpr std::array<std::uint8_t, 2> kZeroEdx = {0x31, 0xD2};

}

void RdpruLowering::EmitOutOfLine(StubBodyBuilder& body) const {
    body.Raw(std::span<const std::uint8_t>(kLeaRspBelowRedZone.Bytes, kLeaRspBelowRedZone.Size));
    body.Raw(kPushfq);
    body.Raw(kCmpEcxOne);
    body.Raw(kJaUnsupported);
    body.Raw(kRdtsc);
    body.Raw(kJmpRestore);
    body.Raw(kZeroEax);
    body.Raw(kZeroEdx);
    body.Raw(kPopfq);
    body.Raw(std::span<const std::uint8_t>(kLeaRspRestore.Bytes, kLeaRspRestore.Size));
}

LoweredBody RdpruLowering::LowerOutOfLine() const {
    StubBodyBuilder body;
    EmitOutOfLine(body);
    return body.Finish();
}

}
