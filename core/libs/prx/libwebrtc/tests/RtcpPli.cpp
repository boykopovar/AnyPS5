#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::Pli check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcRtcpPli Filled() {
    WebRtcRtcpPli pli;
    std::memset(&pli, 0xA5, sizeof(pli));
    return pli;
}

bool Unchanged(const WebRtcRtcpPli& pli) {
    WebRtcRtcpPli filled = Filled();
    return std::memcmp(&pli, &filled, sizeof(pli)) == 0;
}

bool ParsePacket(WebRtcRtcpPli& pli, const std::vector<std::uint8_t>& packet) {
    WebRtcRtcpCommonHeader header;
    Require(_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, packet.data(), packet.size()));
    return _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header);
}

void TestPli() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    WebRtcRtcpPli pli = Filled();
    WebRtcRtcpPli filled = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 0x12345678u);
    Require(pli.mediaSsrc == 0x9ABCDEF0u);
    Require(pli.vtable == filled.vtable);
}

void TestLongerPayloadReadsTheCommonFeedbackOnly() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x55, 0x55, 0x55, 0x55};
    WebRtcRtcpPli pli = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 1);
    Require(pli.mediaSsrc == 0xFFFFFFFEu);
}

void TestShortPayloadLeavesThePacketUntouched() {
    std::vector<std::uint8_t> empty {0x81, 206, 0x00, 0x00};
    WebRtcRtcpPli pli = Filled();
    Require(!ParsePacket(pli, empty));
    Require(Unchanged(pli));

    std::vector<std::uint8_t> padded {0xA1, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0x01};
    Require(!ParsePacket(pli, padded));
    Require(Unchanged(pli));

    WebRtcRtcpCommonHeader header {206, 1, 0, 7, nullptr};
    Require(!_ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header));
    Require(Unchanged(pli));
}

void TestNullArgumentsThrow() {
    std::vector<std::uint8_t> payload(8, 0x11);
    WebRtcRtcpCommonHeader header {206, 1, 0, 8, payload.data()};
    WebRtcRtcpPli pli = Filled();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(nullptr, &header); });
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, nullptr); });
    header.payload = nullptr;
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header); });
    Require(Unchanged(pli));
}

}

int main() {
    TestPli();
    TestLongerPayloadReadsTheCommonFeedbackOnly();
    TestShortPayloadLeavesThePacketUntouched();
    TestNullArgumentsThrow();
    return 0;
}
