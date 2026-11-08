#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;

static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int INVALID_EVENT = static_cast<int>(0x8029000D);
static constexpr int RESOURCE_BUSY = static_cast<int>(0x80290009);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TFunc>
static bool Throws(TFunc&& func) {
    try {
        func();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    for (int invalidHandle : {-1, 100, 0, 0x7FFFFFFF}) {
        Require(sceVideoOutAddFlipEvent(0, invalidHandle, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutDeleteFlipEvent(0, invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutAddVblankEvent(0, invalidHandle, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutDeleteVblankEvent(0, invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutAddPreVblankStartEvent(0, invalidHandle, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutDeletePreVblankStartEvent(0, invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutAddOutputModeEvent(0, invalidHandle, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutDeleteOutputModeEvent(0, invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutAddVrrActiveStatusEvent(0, invalidHandle, nullptr) == INVALID_HANDLE);
        Require(sceVideoOutDeleteVrrActiveStatusEvent(0, invalidHandle) == INVALID_HANDLE);

        VideoOutFlipStatus flipStatus{};
        Require(sceVideoOutGetFlipStatus(invalidHandle, &flipStatus) == INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(invalidHandle) == INVALID_HANDLE);

        std::array<std::byte, 96> vrrStatus{};
        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, vrrStatus.data()) == INVALID_HANDLE);

        std::uint64_t cap = 0;
        Require(sceVideoOutGetDeviceCapabilityInfo_(invalidHandle, &cap, sizeof(cap)) == INVALID_HANDLE);

        std::uintptr_t labelAddress = 0;
        Require(sceVideoOutGetBufferLabelAddress(invalidHandle, &labelAddress) == INVALID_HANDLE);

        VideoOutBufferAttribute attr{};
        Require(sceVideoOutSubmitChangeBufferAttribute(invalidHandle, 0, &attr) == INVALID_HANDLE);

        std::uint32_t token = 0;
        Require(sceVideoOutSubmitEopFlip(invalidHandle, 0, 1, 0, &token) == INVALID_HANDLE);

        Require(sceVideoOutSetFlipMaster(invalidHandle, 0) == INVALID_HANDLE);
        Require(sceVideoOutClearFlipMaster(invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutClose(invalidHandle) == INVALID_HANDLE);

        VideoOutOutputStatus outStatus{};
        Require(Throws([&] { sceVideoOutGetOutputStatus(invalidHandle, &outStatus); }));

        VideoOutResolutionStatus resStatus{};
        Require(Throws([&] { sceVideoOutGetResolutionStatus(invalidHandle, &resStatus); }));

        VideoOutVblankStatus vblankStatus{};
        Require(Throws([&] { sceVideoOutGetVblankStatus(invalidHandle, &vblankStatus); }));

        Require(Throws([&] { sceVideoOutSetFlipRate(invalidHandle, 0); }));

        VideoOutBufferAttribute2 attr2{};
        Require(Throws([&] { sceVideoOutSubmitChangeBufferAttribute2(invalidHandle, 0, &attr2, nullptr); }));
        Require(Throws([&] { sceVideoOutUnregisterBuffers(invalidHandle, 0); }));
        Require(Throws([&] { sceVideoOutSubmitFlip(invalidHandle, 0, 1, 0); }));
    }

    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);
    Require(Throws([] { sceVideoOutOpen(1, MAIN_BUS, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(-1, MAIN_BUS, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, -1, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, 3, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, 100, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 1, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, -1, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 100, nullptr); }));

    VideoOutBufferAttribute bufferAttr{};
    sceVideoOutSetBufferAttribute(&bufferAttr, 0x80000000u, 0, 0, 1920, 1080, 0);
    Require(bufferAttr.pixel_format == 0x80000000u);
    Require(bufferAttr.tiling_mode == 0);
    Require(bufferAttr.aspect_ratio == 0);
    Require(bufferAttr.width == 1920);
    Require(bufferAttr.height == 1080);
    Require(bufferAttr.pitch_in_pixel == 0);

    VideoOutBufferAttribute2 bufferAttr2{};
    sceVideoOutSetBufferAttribute2(&bufferAttr2, 0x8000000000000000ull, 1, 1920, 1080, 0, 0x208, 0x1234);
    Require(bufferAttr2.pixel_format == 0x8000000000000000ull);
    Require(bufferAttr2.tiling_mode == 1);
    Require(bufferAttr2.width == 1920);
    Require(bufferAttr2.height == 1080);
    Require(bufferAttr2.dcc_control == 0x208);
    Require(bufferAttr2.dcc_cb_register_clear_color == 0x1234);

    sceVideoOutAddBufferAttributeOption(&bufferAttr2, 8);
    Require(bufferAttr2.option == 8);
    sceVideoOutRemoveBufferAttributeOption(&bufferAttr2, 8);
    Require(bufferAttr2.option == 0);

    Require(Throws([] { sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 0, 0, 1920, 1080, 0); }));
    Require(Throws([] { sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0); }));
    Require(Throws([] { sceVideoOutAddBufferAttributeOption(nullptr, 8); }));
    Require(Throws([] { sceVideoOutRemoveBufferAttributeOption(nullptr, 8); }));

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "video_out_event_queue") == 0);
    Require(eq != 0);

    Require(Throws([&] { sceVideoOutAddFlipEvent(0, handle, nullptr); }));
    Require(Throws([&] { sceVideoOutDeleteFlipEvent(0, handle); }));
    Require(Throws([&] { sceVideoOutAddFlipEvent(0x7FFFFFFF, handle, nullptr); }));
    Require(Throws([&] { sceVideoOutDeleteFlipEvent(0x7FFFFFFF, handle); }));
    Require(Throws([&] { sceVideoOutAddVblankEvent(0, handle, nullptr); }));
    Require(Throws([&] { sceVideoOutDeleteVblankEvent(0, handle); }));

    Require(sceVideoOutAddFlipEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutAddFlipEvent(eq, handle, reinterpret_cast<void*>(0xCAFE)) == 0);
    Require(sceVideoOutDeleteFlipEvent(eq, handle) == 0);
    Require(sceVideoOutDeleteFlipEvent(eq, handle) == 0);

    Require(sceVideoOutAddVblankEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutDeleteVblankEvent(eq, handle) == 0);

    Require(sceVideoOutAddPreVblankStartEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutDeletePreVblankStartEvent(eq, handle) == 0);

    Require(sceVideoOutAddOutputModeEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutDeleteOutputModeEvent(eq, handle) == 0);

    Require(sceVideoOutAddVrrActiveStatusEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(eq, handle) == 0);

    Require(Throws([] { sceVideoOutGetEventId(nullptr); }));
    std::int64_t dummyData = 0;
    Require(Throws([&] { sceVideoOutGetEventData(nullptr, &dummyData); }));
    KernelEvent dummyEvent{};
    Require(Throws([&] { sceVideoOutGetEventData(&dummyEvent, nullptr); }));
    Require(Throws([] { sceVideoOutGetEventCount(nullptr); }));

    KernelEvent alienEvent{};
    alienEvent.filter = EVFILT_USER;
    Require(sceVideoOutGetEventId(&alienEvent) == INVALID_EVENT);
    Require(sceVideoOutGetEventData(&alienEvent, &dummyData) == INVALID_EVENT);
    Require(sceVideoOutGetEventCount(&alienEvent) == INVALID_EVENT);

    KernelEvent unknownEvent{};
    unknownEvent.filter = EVFILT_VIDEO_OUT;
    unknownEvent.ident = 0x999;
    Require(sceVideoOutGetEventId(&unknownEvent) == INVALID_EVENT);

    KernelEvent simulatedFlip{};
    simulatedFlip.filter = EVFILT_VIDEO_OUT;
    simulatedFlip.ident = 0;
    simulatedFlip.data = static_cast<intptr_t>((0x1234ull << 16u) | (2u << 12u));
    Require(sceVideoOutGetEventId(&simulatedFlip) == 0);
    Require(sceVideoOutGetEventCount(&simulatedFlip) == 2);
    Require(sceVideoOutGetEventData(&simulatedFlip, &dummyData) == 0);
    Require(dummyData == 0x1234);

    VideoOutFlipStatus flipStatus{};
    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == 0);
    Require(sceVideoOutGetFlipStatus(handle, nullptr) == INVALID_ADDRESS);

    VideoOutOutputStatus outStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, nullptr); }));

    VideoOutResolutionStatus resStatus{};
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(Throws([&] { sceVideoOutGetResolutionStatus(handle, nullptr); }));

    VideoOutVblankStatus vblankStatus{};
    Require(sceVideoOutGetVblankStatus(handle, &vblankStatus) == 0);
    Require(Throws([&] { sceVideoOutGetVblankStatus(handle, nullptr); }));

    std::array<std::byte, 96> vrrStatus{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == 0);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == INVALID_ADDRESS);

    std::uintptr_t labelAddress = 0;
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddress) == 16);
    Require(labelAddress != 0);
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == INVALID_ADDRESS);

    Require(sceVideoOutIsFlipPending(handle) == 0);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    Require(sceVideoOutSetFlipRate(handle, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == INVALID_VALUE);

    Require(sceVideoOutSubmitChangeBufferAttribute(handle, 0, nullptr) == INVALID_ADDRESS);
    Require(Throws([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, nullptr, nullptr); }));
    std::uint32_t token = 0;
    Require(sceVideoOutSubmitEopFlip(handle, 0, 1, 0, nullptr) == INVALID_ADDRESS);

    Require(sceKernelDeleteEqueue(eq) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);

    Require(sceVideoOutAddFlipEvent(eq, handle, nullptr) == INVALID_HANDLE);
    Require(sceVideoOutDeleteFlipEvent(eq, handle) == INVALID_HANDLE);
    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == INVALID_HANDLE);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
