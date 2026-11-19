#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrame dimensions check failed at line %u\n", location.line());
    std::abort();
}

const void* expectedBuffer;
int widthCalls;
int heightCalls;

int APS5_VABI Width(const void* buffer) {
    Require(buffer == expectedBuffer);
    ++widthCalls;
    return 853;
}

int APS5_VABI Height(const void* buffer) {
    Require(buffer == expectedBuffer);
    ++heightCalls;
    return 479;
}

void TestDimensions(const void* buffer, int width, int height) {
    alignas(8) std::array<unsigned char, 256> storage;
    storage.fill(0xA5);
    std::memcpy(storage.data() + 8, &buffer, sizeof(buffer));
    const auto original = storage;
    const auto* frame = reinterpret_cast<const WebRtcVideoFrameDimensions*>(storage.data());
    Require(_ZNK6webrtc10VideoFrame5widthEv_nid_postfix(frame) == width);
    Require(_ZNK6webrtc10VideoFrame6heightEv_nid_postfix(frame) == height);
    Require(storage == original);
}

}

int main() {
    TestDimensions(nullptr, 0, 0);
    const void* vtable[] = {nullptr, nullptr, nullptr, nullptr, nullptr,
        reinterpret_cast<const void*>(&Width), reinterpret_cast<const void*>(&Height)};
    const void* const* buffer = vtable;
    expectedBuffer = &buffer;
    TestDimensions(&buffer, 853, 479);
    Require(widthCalls == 1 && heightCalls == 1);
    Require(buffer == vtable);
    bool widthThrew = false;
    bool heightThrew = false;
    try {
        _ZNK6webrtc10VideoFrame5widthEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        widthThrew = true;
    }
    try {
        _ZNK6webrtc10VideoFrame6heightEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        heightThrew = true;
    }
    Require(widthThrew && heightThrew);
    return 0;
}
