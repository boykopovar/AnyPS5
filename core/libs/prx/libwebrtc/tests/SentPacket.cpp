#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtc::SentPacket constructor check failed at line %u\n", location.line());
    std::abort();
}

WebRtcPacketInfo SeedInfo(std::uint8_t seed) {
    WebRtcPacketInfo info;
    std::memset(&info, static_cast<int>(seed), sizeof(info));
    return info;
}

}

int main() {
    unsigned char object[sizeof(WebRtcSentPacket)];
    std::memset(object, 0xA5, sizeof(object));

    _ZN3rtc10SentPacketC1Ell_nid_postfix(reinterpret_cast<WebRtcSentPacket*>(object), 42, 1234);
    const WebRtcSentPacket& packet = *reinterpret_cast<const WebRtcSentPacket*>(object);
    Require(packet.packetId == 42);
    Require(packet.sendTimeMs == 1234);
    Require(packet.info.includedInFeedback == false);
    Require(packet.info.includedInAllocation == false);
    Require(packet.info.isMedia == false);
    Require(packet.info.packetType == WebRtcPacketType::kUnknown);
    Require(packet.info.protocol == WebRtcPacketInfoProtocolType::kUnknown);
    Require(packet.info.networkId.engaged == false);
    Require(packet.info.networkId.storage.nullState == static_cast<char>(0xA5));
    Require(packet.info.packetSizeBytes == 0);
    Require(packet.info.turnOverheadBytes == 0);
    Require(packet.info.ipOverheadBytes == 0);

    const WebRtcPacketInfo info = SeedInfo(0x5A);
    _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(reinterpret_cast<WebRtcSentPacket*>(object), -7, -9, &info);
    Require(packet.packetId == -7);
    Require(packet.sendTimeMs == -9);
    Require(std::memcmp(&packet.info, &info, sizeof(info)) == 0);

    bool threw = false;
    try {
        _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(reinterpret_cast<WebRtcSentPacket*>(object), 1, 2, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN3rtc10SentPacketC1Ell_nid_postfix(nullptr, 1, 2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
