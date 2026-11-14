#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
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
    std::fprintf(stderr, "libwebrtc rtcp::CommonHeader check failed at line %u\n", location.line());
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

WebRtcRtcpCommonHeader Filled() {
    WebRtcRtcpCommonHeader header;
    std::memset(&header, 0xA5, sizeof(header));
    return header;
}

bool Parse(WebRtcRtcpCommonHeader& header, const std::vector<std::uint8_t>& packet, std::size_t size) {
    return _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, packet.data(), size);
}

bool Unchanged(const WebRtcRtcpCommonHeader& header) {
    WebRtcRtcpCommonHeader filled = Filled();
    return header.packetType == filled.packetType && header.countOrFormat == filled.countOrFormat && header.paddingSize == filled.paddingSize &&
        header.payloadSize == filled.payloadSize && header.payload == filled.payload;
}

void TestReceiverReport() {
    std::vector<std::uint8_t> packet {0x81, 201, 0x00, 0x07};
    packet.resize(32, 0x11);
    WebRtcRtcpCommonHeader header = Filled();
    Require(Parse(header, packet, packet.size()));
    Require(header.countOrFormat == 1);
    Require(header.packetType == 201);
    Require(header.payloadSize == 28);
    Require(header.paddingSize == 0);
    Require(header.payload == packet.data() + 4);
}

void TestFeedbackFormatAndEmptyPayload() {
    std::vector<std::uint8_t> packet {0x9F, 206, 0x00, 0x00};
    WebRtcRtcpCommonHeader header = Filled();
    Require(Parse(header, packet, packet.size()));
    Require(header.countOrFormat == 31);
    Require(header.packetType == 206);
    Require(header.payloadSize == 0);
    Require(header.paddingSize == 0);
}

void TestPadding() {
    std::vector<std::uint8_t> packet {0xA0, 200, 0x00, 0x02, 1, 2, 3, 4, 5, 0, 0, 3};
    WebRtcRtcpCommonHeader header = Filled();
    Require(Parse(header, packet, packet.size()));
    Require(header.countOrFormat == 0);
    Require(header.paddingSize == 3);
    Require(header.payloadSize == 5);
    Require(header.payload == packet.data() + 4);

    packet[11] = 8;
    header = Filled();
    Require(Parse(header, packet, packet.size()));
    Require(header.paddingSize == 8);
    Require(header.payloadSize == 0);
}

void TestRejectsBeforeReading() {
    std::vector<std::uint8_t> packet {0x80, 200, 0x00, 0x00};
    WebRtcRtcpCommonHeader header = Filled();
    Require(!Parse(header, packet, 3));
    Require(Unchanged(header));
    Require(!_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, nullptr, 3));
    Require(Unchanged(header));
    for (std::uint8_t first : {0x00, 0x41, 0xC1}) {
        packet[0] = first;
        Require(!Parse(header, packet, packet.size()));
        Require(Unchanged(header));
    }
}

void TestRejectsAfterReading() {
    std::vector<std::uint8_t> packet {0x81, 201, 0x00, 0x02, 1, 2, 3, 4, 5, 6, 7};
    WebRtcRtcpCommonHeader header = Filled();
    Require(!Parse(header, packet, packet.size()));
    Require(header.countOrFormat == 1);
    Require(header.packetType == 201);
    Require(header.payloadSize == 8);
    Require(header.paddingSize == 0);
    Require(header.payload == packet.data() + 4);

    std::vector<std::uint8_t> empty {0xA0, 200, 0x00, 0x00};
    header = Filled();
    Require(!Parse(header, empty, empty.size()));
    Require(header.payloadSize == 0);
    Require(header.paddingSize == 0);

    std::vector<std::uint8_t> zeroPadding {0xA0, 200, 0x00, 0x01, 1, 2, 3, 0};
    header = Filled();
    Require(!Parse(header, zeroPadding, zeroPadding.size()));
    Require(header.payloadSize == 4);
    Require(header.paddingSize == 0);

    std::vector<std::uint8_t> tooMuchPadding {0xA0, 200, 0x00, 0x01, 1, 2, 3, 5};
    header = Filled();
    Require(!Parse(header, tooMuchPadding, tooMuchPadding.size()));
    Require(header.payloadSize == 4);
    Require(header.paddingSize == 5);
}

void TestNullArguments() {
    std::array<std::uint8_t, 4> packet {0x80, 200, 0x00, 0x00};
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(nullptr, packet.data(), packet.size()); });
    WebRtcRtcpCommonHeader header = Filled();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, nullptr, 4); });
    Require(Unchanged(header));
}

}

int main() {
    TestReceiverReport();
    TestFeedbackFormatAndEmptyPayload();
    TestPadding();
    TestRejectsBeforeReading();
    TestRejectsAfterReading();
    TestNullArguments();
    std::puts("libwebrtc rtcp::CommonHeader tests passed");
}
