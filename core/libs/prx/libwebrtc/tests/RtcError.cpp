#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address_ != nullptr);
#else
        void* mapped = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        address_ = mapped;
#endif
    }
    ~GuardedPage() {
        if (!address_)
            return;
#ifdef _WIN32
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, 4096);
#endif
    }
    template<typename T>
    T* As() const {
        return static_cast<T*>(address_);
    }

private:
    void* address_ = nullptr;
};

void TestGuardedErrorThrows() {
    GuardedPage guard;
    try {
        _ZNK6webrtc8RTCError7messageEv_nid_postfix(guard.As<const WebRtcRtcError>());
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
    TestGuardedErrorThrows();
    return 0;
}
