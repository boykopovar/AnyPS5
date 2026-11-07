#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::TransportFeedback check failed at line %u\n", location.line());
    std::abort();
}

void TestBaseSequence() {
    unsigned char storage[96];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* feedback = reinterpret_cast<WebRtcRtcpTransportFeedback*>(storage);
    for (std::uint16_t value : {std::uint16_t {0}, std::uint16_t {1}, std::uint16_t {0x1234}, std::uint16_t {0xFFFF}}) {
        feedback->baseSequenceNumber = value;
        Require(_ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback) == value);
    }
    feedback->baseSequenceNumber = 0x0102;
    feedback->sequenceNumberCount = 0xBEEF;
    feedback->senderSsrc = 0x11111111u;
    feedback->mediaSsrc = 0x22222222u;
    Require(_ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback) == 0x0102);
    unsigned char before[sizeof(storage)];
    std::memcpy(before, storage, sizeof(storage));
    _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback);
    Require(std::memcmp(before, storage, sizeof(storage)) == 0);
}

void TestNullPacketThrows() {
    try {
        _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestBaseSequence();
    TestNullPacketThrows();
    return 0;
}
