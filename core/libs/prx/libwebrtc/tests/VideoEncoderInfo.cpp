#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <source_location>
#include <stdexcept>

namespace {

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer)
        return;
    if (live.erase(pointer) != 1)
        std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc EncoderInfo check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Construct(WebRtcVideoEncoderInfo& info) {
    std::memset(&info, 0xcd, sizeof(info));
    _ZN6webrtc12VideoEncoder11EncoderInfoC1Ev_nid_postfix(&info);
}

void TestDefault() {
    WebRtcVideoEncoderInfo info;
    Construct(info);
    Require(live.empty());
    Require(!info.hasQpThresholds && info.minPixelsPerFrame == 320 * 180);
    Require(info.requestedResolutionAlignment == 1);
    Require(!info.applyAlignmentToAllSimulcastLayers && !info.supportsNativeHandle);
    Require(info.implementationName.reserved == 0 && info.implementationName.size == 7 && info.implementationName.capacity == 15);
    Require(std::strcmp(info.implementationName.buffer, "unknown") == 0);
    Require(!info.hasTrustedRateController && info.isHardwareAccelerated);
    Require(info.fpsAllocation[0].metadata == 2 && info.fpsAllocation[0].inlined[0] == 255);
    for (std::size_t index = 1; index < WebRtcMaxSpatialLayers; ++index)
        Require(info.fpsAllocation[index].metadata == 0);
    Require(!info.resolutionBitrateLimitsBegin && !info.resolutionBitrateLimitsEnd && !info.resolutionBitrateLimitsCapacity);
    Require(!info.supportsSimulcast);
    std::int32_t format = 0;
    std::memcpy(&format, info.preferredPixelFormats.inlined, sizeof(format));
    Require(info.preferredPixelFormats.metadata == 2 && format == 1);
    Require(!info.hasIsQpTrusted && !info.hasMinQp);
    _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info);
    Require(live.empty());
}

template<std::size_t TInlineBytes>
void Allocate(WebRtcInlinedVector<TInlineBytes>& vector, std::size_t size, std::size_t capacity, std::size_t elementSize) {
    vector.metadata = (size << 1) | 1;
    vector.allocated.data = GuestStringStorage::Allocate(capacity * elementSize);
    vector.allocated.capacity = capacity;
    std::memset(vector.allocated.data, 7, size * elementSize);
}

void TestDestroyAllocated() {
    WebRtcVideoEncoderInfo info;
    Construct(info);
    const char name[] = "libvpx-vp9-encoder-implementation";
    info.implementationName.capacity = 47;
    info.implementationName.pointer = static_cast<char*>(GuestStringStorage::Allocate(48));
    std::memcpy(info.implementationName.pointer, name, sizeof(name));
    info.implementationName.size = sizeof(name) - 1;
    Allocate(info.fpsAllocation[0], 17, 32, 1);
    info.fpsAllocation[1].metadata = 16 << 1;
    Allocate(info.fpsAllocation[4], 20, 5000, 1);
    auto* limits = static_cast<WebRtcResolutionBitrateLimits*>(GuestStringStorage::Allocate(4 * sizeof(WebRtcResolutionBitrateLimits)));
    info.resolutionBitrateLimitsBegin = limits;
    info.resolutionBitrateLimitsEnd = limits + 3;
    info.resolutionBitrateLimitsCapacity = limits + 4;
    Allocate(info.preferredPixelFormats, 6, 10, sizeof(std::int32_t));
    Require(live.size() == 5);
    void* largeRaw = static_cast<void**>(info.fpsAllocation[4].allocated.data)[-1];
    Require(live.count(largeRaw) == 1);
    _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info);
    Require(live.empty());
}

void TestInvalid() {
    Require(Throws([] { _ZN6webrtc12VideoEncoder11EncoderInfoC1Ev_nid_postfix(nullptr); }));
    Require(Throws([] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(nullptr); }));

    WebRtcVideoEncoderInfo info;
    Construct(info);
    Allocate(info.preferredPixelFormats, 6, 10, sizeof(std::int32_t));
    auto* limits = static_cast<WebRtcResolutionBitrateLimits*>(GuestStringStorage::Allocate(2 * sizeof(WebRtcResolutionBitrateLimits)));
    info.resolutionBitrateLimitsBegin = limits;
    info.resolutionBitrateLimitsEnd = limits + 2;
    info.resolutionBitrateLimitsCapacity = limits + 2;
    const std::set<void*> held = live;

    info.fpsAllocation[2].metadata = 17 << 1;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.fpsAllocation[2].metadata = 0;

    info.fpsAllocation[3].metadata = (3 << 1) | 1;
    info.fpsAllocation[3].allocated.data = nullptr;
    info.fpsAllocation[3].allocated.capacity = 32;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.fpsAllocation[3].allocated.data = limits;
    info.fpsAllocation[3].allocated.capacity = 2;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.fpsAllocation[3].metadata = 0;

    info.resolutionBitrateLimitsEnd = limits + 3;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.resolutionBitrateLimitsEnd = limits + 2;

    const auto preferredSize = info.preferredPixelFormats.metadata;
    info.preferredPixelFormats.metadata = (11 << 1) | 1;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.preferredPixelFormats.metadata = preferredSize;

    info.implementationName.size = 16;
    Require(Throws([&] { _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info); }));
    info.implementationName.size = 7;
    Require(live == held);

    _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(&info);
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDefault();
    TestDestroyAllocated();
    TestInvalid();
    return 0;
}
