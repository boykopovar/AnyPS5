#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCError check failed at line %u\n", location.line());
    std::abort();
}

WebRtcRtcError Error() {
    WebRtcRtcError error;
    std::memset(&error, 0xA5, sizeof(error));
    error.type = 2;
    return error;
}

void TestInlineMessage() {
    WebRtcRtcError error = Error();
    std::memcpy(error.message.buffer, "Invalid SDP", 12);
    error.message.size = 11;
    error.message.capacity = 15;
    Require(_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error) == error.message.buffer);
    Require(std::strcmp(_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error), "Invalid SDP") == 0);

    std::memcpy(error.message.buffer, "fifteen chars!!", 16);
    error.message.size = 15;
    Require(_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error) == error.message.buffer);
}

void TestEmptyMessage() {
    WebRtcRtcError error = Error();
    error.message.buffer[0] = '\0';
    error.message.size = 0;
    error.message.capacity = 15;
    Require(*_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error) == '\0');
}

void TestHeapMessage() {
    char text[] = "Failed to set remote answer sdp: Called in wrong state";
    WebRtcRtcError error = Error();
    error.message.pointer = text;
    error.message.size = sizeof(text) - 1;
    error.message.capacity = 63;
    Require(_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error) == text);
    error.message.capacity = 16;
    Require(_ZNK6webrtc8RTCError7messageEv_nid_postfix(&error) == text);
}

void TestNullErrorThrows() {
    try {
        _ZNK6webrtc8RTCError7messageEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestInlineMessage();
    TestEmptyMessage();
    TestHeapMessage();
    TestNullErrorThrows();
    return 0;
}
