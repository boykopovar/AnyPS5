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

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const WebRtcRtcpPli* pli);
using CreateFunction = bool (APS5_VABI*)(const WebRtcRtcpPli* pli, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

std::vector<std::uint8_t> sent;

void APS5_VABI Send(void* function, const std::uint8_t* data, std::size_t size) {
    Require(function == &sent);
    sent.assign(data, data + size);
}

void* Slot(const WebRtcRtcpPli& pli, int slot) {
    return static_cast<void* const*>(pli.vtable)[slot];
}

void TestConstructor() {
    WebRtcRtcpPli pli = Filled();
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    Require(pli.vtable != nullptr && pli.senderSsrc == 0 && pli.mediaSsrc == 0);
    Require(Slot(pli, 0) == reinterpret_cast<void*>(&_ZN6webrtc4rtcp3PliD1Ev_nid_postfix));
    Require(Slot(pli, 1) != nullptr);
    Require(reinterpret_cast<BlockLengthFunction>(Slot(pli, 2))(&pli) == 12);
    RequireInvalidArgument([] { _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(nullptr); });
}

void TestCreate() {
    WebRtcRtcpPli pli = Filled();
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    pli.senderSsrc = 0x12345678;
    pli.mediaSsrc = 0x9ABCDEF0;
    const auto create = reinterpret_cast<CreateFunction>(Slot(pli, 3));
    std::uint8_t packet[24] = {};
    std::size_t index = 0;
    Require(create(&pli, packet, &index, sizeof(packet), nullptr, nullptr));
    const std::uint8_t expected[12] = {0x81, 206, 0, 2, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    Require(index == 12 && std::memcmp(packet, expected, sizeof(expected)) == 0);

    index = 0;
    Require(!create(&pli, packet, &index, 11, nullptr, nullptr) && index == 0);

    index = 12;
    sent.clear();
    Require(create(&pli, packet, &index, 20, &sent, &Send));
    Require(sent.size() == 12 && std::memcmp(sent.data(), expected, sizeof(expected)) == 0);
    Require(index == 12 && std::memcmp(packet, expected, sizeof(expected)) == 0);

    index = 12;
    RequireInvalidArgument([&] { create(&pli, packet, &index, 20, nullptr, nullptr); });
}

int main() {
    TestPli();
    TestLongerPayloadReadsTheCommonFeedbackOnly();
    TestShortPayloadLeavesThePacketUntouched();
    TestNullArgumentsThrow();
    TestConstructor();
    TestCreate();
    return 0;
}
