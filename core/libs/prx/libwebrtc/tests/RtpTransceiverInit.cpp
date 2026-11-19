#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RtpTransceiverInit check failed at line %u\n", location.line());
    std::abort();
}

void TestDefaultConstructor() {
    WebRtcRtpTransceiverInit init;
    std::memset(&init, 0xA5, sizeof(init));
    _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(&init);
    Require(init.direction == WebRtcRtpTransceiverDirectionSendRecv);
    Require(!init.streamIds.begin && !init.streamIds.end && !init.streamIds.capacityEnd);
    Require(!init.sendEncodings.begin && !init.sendEncodings.end && !init.sendEncodings.capacityEnd);
}

void TestNullObjectThrows() {
    try {
        _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestDefaultConstructor();
    TestNullObjectThrows();
    std::puts("libwebrtc RtpTransceiverInit tests passed");
    return 0;
}
