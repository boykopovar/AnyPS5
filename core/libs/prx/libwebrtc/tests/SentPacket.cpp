#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SentPacket check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

constexpr std::array<std::int64_t, 5> Values {0, -1, 1234567890123, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()};

void TestDefault() {
    for (auto id : Values) {
        for (auto time : Values) {
            WebRtcSentPacket packet;
            std::memset(&packet, 0xA5, sizeof(packet));
            _ZN3rtc10SentPacketC1Ell_nid_postfix(&packet, id, time);
            Require(packet.packetId == id);
            Require(packet.sendTimeMs == time);
            Require(!packet.info.includedInFeedback);
            Require(!packet.info.includedInAllocation);
            Require(!packet.info.isMedia);
            Require(packet.info.packetType == 0);
            Require(packet.info.protocol == 0);
            Require(!packet.info.hasNetworkId);
            Require(packet.info.packetSizeBytes == 0);
            Require(packet.info.turnOverheadBytes == 0);
            Require(packet.info.ipOverheadBytes == 0);
        }
    }
    RequireInvalidArgument([] { _ZN3rtc10SentPacketC1Ell_nid_postfix(nullptr, 1, 2); });
}

void TestCopy() {
    for (unsigned bits = 0; bits < 16; ++bits) {
        WebRtcPacketInfo info;
        std::memset(&info, 0x5A, sizeof(info));
        info.includedInFeedback = (bits & 1) != 0;
        info.includedInAllocation = (bits & 2) != 0;
        info.isMedia = (bits & 4) != 0;
        info.packetType = bits % 6;
        info.protocol = bits % 5;
        info.networkId = 0xFEDC;
        info.hasNetworkId = (bits & 8) != 0;
        info.packetSizeBytes = 0x123456789ABCDEF0;
        info.turnOverheadBytes = 0xFEDCBA9876543210;
        info.ipOverheadBytes = 0x876543210FEDCBA9;
        for (auto id : Values) {
            for (auto time : Values) {
                WebRtcSentPacket packet;
                std::memset(&packet, 0xA5, sizeof(packet));
                _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(&packet, id, time, &info);
                Require(packet.packetId == id);
                Require(packet.sendTimeMs == time);
                Require(packet.info.includedInFeedback == info.includedInFeedback);
                Require(packet.info.includedInAllocation == info.includedInAllocation);
                Require(packet.info.isMedia == info.isMedia);
                Require(packet.info.packetType == info.packetType);
                Require(packet.info.protocol == info.protocol);
                Require(packet.info.hasNetworkId == info.hasNetworkId);
                if (info.hasNetworkId)
                    Require(packet.info.networkId == 0xFEDC);
                Require(packet.info.packetSizeBytes == info.packetSizeBytes);
                Require(packet.info.turnOverheadBytes == info.turnOverheadBytes);
                Require(packet.info.ipOverheadBytes == info.ipOverheadBytes);
            }
        }
    }
    WebRtcSentPacket packet;
    WebRtcPacketInfo info {};
    RequireInvalidArgument([&] { _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(nullptr, 1, 2, &info); });
    RequireInvalidArgument([&] { _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(&packet, 1, 2, nullptr); });
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "default") == 0)
        TestDefault();
    else {
        Require(std::strcmp(argv[1], "copy") == 0);
        TestCopy();
    }
    return 0;
}
