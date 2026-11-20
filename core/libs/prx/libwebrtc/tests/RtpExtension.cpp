#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void* lastFreed = nullptr;
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RtpExtension check failed at line %u\n", location.line());
    std::abort();
}

WebRtcRtpExtension Extension(std::int32_t id) {
    WebRtcRtpExtension extension;
    std::memset(&extension, 0, sizeof(extension));
    extension.id = id;
    extension.encrypt = true;
    return extension;
}

void TestInlineUri() {
    WebRtcRtpExtension extension = Extension(3);
    std::memcpy(extension.uri.buffer, "urn:3gpp:video", 15);
    extension.uri.size = 14;
    extension.uri.capacity = 15;
    const int before = frees;
    _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(&extension);
    Require(frees == before);
    Require(std::strcmp(extension.uri.buffer, "urn:3gpp:video") == 0 && extension.id == 3);
}

void TestHeapUri() {
    const char text[] = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
    WebRtcRtpExtension extension = Extension(1);
    extension.uri.pointer = static_cast<char*>(_Znwm_nid_postfix(64));
    std::memcpy(extension.uri.pointer, text, sizeof(text));
    extension.uri.size = sizeof(text) - 1;
    extension.uri.capacity = 63;
    void* storage = extension.uri.pointer;
    const int before = frees;
    _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(&extension);
    Require(frees == before + 1 && lastFreed == storage);
}

void TestLargeHeapUri() {
    constexpr std::size_t bytes = 0x1000;
    auto* raw = static_cast<unsigned char*>(_Znwm_nid_postfix(bytes + 0x27));
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + 0x27) & ~std::uintptr_t {0x1f};
    reinterpret_cast<void**>(aligned)[-1] = raw;
    WebRtcRtpExtension extension = Extension(2);
    extension.uri.pointer = reinterpret_cast<char*>(aligned);
    std::memset(extension.uri.pointer, 'a', bytes - 1);
    extension.uri.pointer[bytes - 1] = '\0';
    extension.uri.size = bytes - 1;
    extension.uri.capacity = bytes - 1;
    const int before = frees;
    _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(&extension);
    Require(frees == before + 1 && lastFreed == raw);
}

void TestNullExtensionThrows() {
    try {
        _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestInlineUri();
    TestHeapUri();
    TestLargeHeapUri();
    TestNullExtensionThrows();
    return 0;
}
