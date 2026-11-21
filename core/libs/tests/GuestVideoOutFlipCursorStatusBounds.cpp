#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/LaterExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int InvalidHandle = -1;
constexpr int NeverOpenedHandle = 2;
constexpr int OutOfRangeHandle = 9999;

constexpr int InvalidValue = static_cast<int>(0x80290001u);
constexpr int InvalidAddress = static_cast<int>(0x80290002u);
constexpr int InvalidHandleError = static_cast<int>(0x8029000Bu);
constexpr int InvalidEvent = static_cast<int>(0x8029000Du);
constexpr int InvalidEventQueue = static_cast<int>(0x80290005u);
constexpr int Error0x8029001C = static_cast<int>(0x8029001Cu);

constexpr int VideoOutEventFlip = 0;
constexpr int VideoOutEventVblank = 1;
constexpr int VideoOutEventPreVblankStart = 2;
constexpr int VideoOutEventSetMode = 8;
constexpr int VideoOutEventVrrStatus = 16;

#define Require(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: Require(%s) failed\n", __FILE__, __LINE__, #condition); \
            std::abort(); \
        } \
    } while (false)

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutAddFlipEvent(0, InvalidHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutAddFlipEvent(0, NeverOpenedHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutAddFlipEvent(0, OutOfRangeHandle, nullptr) == InvalidHandleError);

    Require(sceVideoOutDeleteFlipEvent(0, InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutDeleteFlipEvent(0, NeverOpenedHandle) == InvalidHandleError);
    Require(sceVideoOutDeleteFlipEvent(0, OutOfRangeHandle) == InvalidHandleError);

    Require(sceVideoOutAddVblankEvent(0, InvalidHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutDeleteVblankEvent(0, InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutAddPreVblankStartEvent(0, InvalidHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutDeletePreVblankStartEvent(0, InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutAddOutputModeEvent(0, InvalidHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutDeleteOutputModeEvent(0, InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutAddVrrActiveStatusEvent(0, InvalidHandle, nullptr) == InvalidHandleError);
    Require(sceVideoOutDeleteVrrActiveStatusEvent(0, InvalidHandle) == InvalidHandleError);

    Require(sceVideoOutAddFlipEvent(0, handle, nullptr) == InvalidEventQueue);
    Require(sceVideoOutAddFlipEvent(static_cast<KernelEqueue>(0xdeadbeef), handle, nullptr) == InvalidEventQueue);
    Require(sceVideoOutDeleteFlipEvent(0, handle) == InvalidEventQueue);
    Require(sceVideoOutDeleteFlipEvent(static_cast<KernelEqueue>(0xdeadbeef), handle) == InvalidEventQueue);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "videoout_test_eq") == 0);
    Require(eq != 0);

    Require(sceVideoOutAddFlipEvent(eq, handle, nullptr) == 0);
    Require(sceVideoOutAddFlipEvent(eq, handle, reinterpret_cast<void*>(0x1234)) == 0);
    Require(sceVideoOutDeleteFlipEvent(eq, handle) == 0);
    Require(sceVideoOutDeleteFlipEvent(eq, handle) == 0);

    Require(sceKernelDeleteEqueue(eq) == 0);

    Require(sceVideoOutGetEventId(nullptr) == InvalidAddress);

    KernelEvent nonVideoEvent{};
    nonVideoEvent.filter = EVFILT_USER;
    Require(sceVideoOutGetEventId(&nonVideoEvent) == InvalidEvent);

    KernelEvent videoEvent{};
    videoEvent.filter = EVFILT_VIDEO_OUT;
    videoEvent.ident = 0x99;
    Require(sceVideoOutGetEventId(&videoEvent) == InvalidEvent);

    videoEvent.ident = VideoOutEventFlip;
    Require(sceVideoOutGetEventId(&videoEvent) == VideoOutEventFlip);
    videoEvent.ident = VideoOutEventVblank;
    Require(sceVideoOutGetEventId(&videoEvent) == VideoOutEventVblank);
    videoEvent.ident = VideoOutEventPreVblankStart;
    Require(sceVideoOutGetEventId(&videoEvent) == VideoOutEventPreVblankStart);
    videoEvent.ident = VideoOutEventSetMode;
    Require(sceVideoOutGetEventId(&videoEvent) == VideoOutEventSetMode);
    videoEvent.ident = VideoOutEventVrrStatus;
    Require(sceVideoOutGetEventId(&videoEvent) == VideoOutEventVrrStatus);

    int64_t eventData = 0;
    Require(sceVideoOutGetEventData(nullptr, &eventData) == InvalidAddress);
    Require(sceVideoOutGetEventData(&videoEvent, nullptr) == InvalidAddress);
    Require(sceVideoOutGetEventData(&nonVideoEvent, &eventData) == InvalidEvent);

    videoEvent.ident = VideoOutEventFlip;
    videoEvent.data = static_cast<intptr_t>(0x00420000ULL);
    Require(sceVideoOutGetEventData(&videoEvent, &eventData) == 0);
    Require(eventData == 0x42);

    videoEvent.data = static_cast<intptr_t>(0x8000000000000000ULL | (0x0042ULL << 16u));
    Require(sceVideoOutGetEventData(&videoEvent, &eventData) == 0);
    Require((static_cast<uint64_t>(eventData) & 0xffff000000000000ULL) == 0xffff000000000000ULL);

    Require(Throws([] { sceVideoOutGetEventCount(nullptr); }));
    Require(sceVideoOutGetEventCount(&nonVideoEvent) == 0);

    videoEvent.data = static_cast<intptr_t>(0xbu << 12u);
    Require(sceVideoOutGetEventCount(&videoEvent) == 0xb);

    Require(sceVideoOutCursorEnable(InvalidHandle, 0, nullptr) == InvalidHandleError);
    Require(sceVideoOutCursorEnable(NeverOpenedHandle, 0, nullptr) == InvalidHandleError);
    Require(sceVideoOutCursorDisable(InvalidHandle, 0) == InvalidHandleError);
    Require(sceVideoOutCursorDisable(NeverOpenedHandle, 0) == InvalidHandleError);
    Require(sceVideoOutCursorSetImageAddress(InvalidHandle, 0, nullptr) == InvalidHandleError);
    Require(sceVideoOutCursorSetImageAddress(NeverOpenedHandle, 0, nullptr) == InvalidHandleError);
    Require(sceVideoOutCursorSetPosition(InvalidHandle, 0, 100, 200) == InvalidHandleError);
    Require(sceVideoOutCursorSetPosition(NeverOpenedHandle, 0, 100, 200) == InvalidHandleError);
    Require(sceVideoOutCursorSet2xMagnify(InvalidHandle, 0, true) == InvalidHandleError);
    Require(sceVideoOutCursorSet2xMagnify(NeverOpenedHandle, 0, true) == InvalidHandleError);

    Require(Throws([&] { sceVideoOutCursorEnable(handle, 1, nullptr); }));
    Require(Throws([&] { sceVideoOutCursorEnable(handle, -1, nullptr); }));
    Require(Throws([&] { sceVideoOutCursorDisable(handle, 1); }));
    Require(Throws([&] { sceVideoOutCursorDisable(handle, -1); }));
    Require(Throws([&] { sceVideoOutCursorSetImageAddress(handle, 1, nullptr); }));
    Require(Throws([&] { sceVideoOutCursorSetImageAddress(handle, -1, nullptr); }));
    Require(Throws([&] { sceVideoOutCursorSetPosition(handle, 1, 0, 0); }));
    Require(Throws([&] { sceVideoOutCursorSetPosition(handle, -1, 0, 0); }));
    Require(Throws([&] { sceVideoOutCursorSet2xMagnify(handle, 1, false); }));
    Require(Throws([&] { sceVideoOutCursorSet2xMagnify(handle, -1, false); }));

    Require(sceVideoOutCursorEnable(handle, 0, nullptr) == InvalidAddress);
    Require(sceVideoOutCursorSetImageAddress(handle, 0, nullptr) == InvalidAddress);

    uint32_t dummyCursorImage = 0x12345678;
    Require(Throws([&] { sceVideoOutCursorEnable(handle, 0, &dummyCursorImage); }));
    Require(Throws([&] { sceVideoOutCursorSetImageAddress(handle, 0, &dummyCursorImage); }));

    Require(sceVideoOutCursorSetPosition(handle, 0, 0, 0) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 1920, 1080) == 0);
    Require(sceVideoOutCursorSetPosition(handle, 0, 3840, 2160) == 0);

    Require(sceVideoOutCursorSet2xMagnify(handle, 0, true) == 0);
    Require(sceVideoOutCursorSet2xMagnify(handle, 0, false) == 0);

    Require(sceVideoOutCursorDisable(handle, 0) == 0);

    Require(Throws([] { sceVideoOutCursorSetHotSpot(); }));
    Require(Throws([] { sceVideoOutCursorIsUpdatePending(); }));
    Require(Throws([] { sceVideoOutCursorSetPositionStereo(); }));
    Require(Throws([] { sceVideoOutSysCursorSetSize(); }));
    Require(Throws([] { sceVideoOutSysGetCursorInfo(); }));

    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, nullptr); }));

    VideoOutOutputStatus outStatus{};
    Require(Throws([&] { sceVideoOutGetOutputStatus(InvalidHandle, &outStatus); }));
    Require(Throws([&] { sceVideoOutGetOutputStatus(NeverOpenedHandle, &outStatus); }));
    Require(Throws([&] { sceVideoOutGetOutputStatus(OutOfRangeHandle, &outStatus); }));

    std::memset(&outStatus, 0xff, sizeof(outStatus));
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.resolution == 1 || outStatus.resolution == 2);
    Require(outStatus.refreshRate != 0);
    Require(outStatus.dynamicRange == 1 || outStatus.dynamicRange == 2);
    Require(outStatus.flags == 0);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == InvalidAddress);

    std::array<std::byte, 96> vrrBuffer;
    vrrBuffer.fill(std::byte{0xaa});
    Require(sceVideoOutGetOutputStatusWithVrr(InvalidHandle, vrrBuffer.data()) == InvalidHandleError);
    Require(sceVideoOutGetOutputStatusWithVrr(NeverOpenedHandle, vrrBuffer.data()) == InvalidHandleError);
    Require(sceVideoOutGetOutputStatusWithVrr(OutOfRangeHandle, vrrBuffer.data()) == InvalidHandleError);
    for (const auto byte : vrrBuffer) Require(byte == std::byte{0xaa});

    vrrBuffer.fill(std::byte{0xdd});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == Error0x8029001C);
    for (const auto byte : vrrBuffer) Require(byte == std::byte{0xdd});

    Require(sceVideoOutGetFlipStatus(handle, nullptr) == InvalidAddress);
    Require(Throws([] { sceVideoOutGetFlipStatus(InvalidHandle, nullptr); }));
    Require(Throws([] { sceVideoOutGetFlipStatus(NeverOpenedHandle, nullptr); }));

    VideoOutFlipStatus flipStatus{};
    Require(sceVideoOutGetFlipStatus(InvalidHandle, &flipStatus) == InvalidHandleError);
    Require(sceVideoOutGetFlipStatus(NeverOpenedHandle, &flipStatus) == InvalidHandleError);

    std::memset(&flipStatus, 0, sizeof(flipStatus));
    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == 0);
    Require(flipStatus.flipArg == -1);
    Require(flipStatus.currentBuffer == -1);
    Require(flipStatus.flipPendingNum == 0);
    Require(flipStatus.count == 0);
    Require(flipStatus.gcQueueNum == 0);

    Require(sceVideoOutIsFlipPending(InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutIsFlipPending(NeverOpenedHandle) == InvalidHandleError);
    Require(sceVideoOutIsFlipPending(handle) == 0);

    Require(Throws([] { sceVideoOutSetFlipRate(InvalidHandle, 0); }));
    Require(Throws([] { sceVideoOutSetFlipRate(NeverOpenedHandle, 1); }));

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    Require(sceVideoOutSetFlipRate(handle, -1) == InvalidValue);
    Require(sceVideoOutSetFlipRate(handle, 3) == InvalidValue);
    Require(sceVideoOutSetFlipRate(handle, 60) == InvalidValue);

    Require(Throws([] { sceVideoOutSetWindowModeMargins(InvalidHandle, 0, 0); }));
    Require(Throws([] { sceVideoOutSetWindowModeMargins(NeverOpenedHandle, 10, 10); }));
    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 10, 20) == 0);

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);

    Require(sceVideoOutSetFlipMaster(InvalidHandle, 0) == InvalidHandleError);
    Require(sceVideoOutSetFlipMaster(handle, 0) == InvalidHandleError);
    Require(sceVideoOutSetFlipMaster(handle, 1) == InvalidHandleError);
    Require(Throws([&] { sceVideoOutSetFlipMaster(handle, 2); }));

    Require(sceVideoOutClearFlipMaster(InvalidHandle) == InvalidHandleError);
    Require(sceVideoOutClearFlipMaster(handle) == Error0x8029001C);

    Require(sceVideoOutClose(handle) == 0);
    Require(Throws([&] { sceVideoOutClose(handle); }));

    Require(sceVideoOutCursorDisable(handle, 0) == InvalidHandleError);
    Require(sceVideoOutCursorSetPosition(handle, 0, 10, 10) == InvalidHandleError);
    Require(sceVideoOutIsFlipPending(handle) == InvalidHandleError);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == InvalidHandleError);
    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, &outStatus); }));
    Require(Throws([&] { sceVideoOutSetFlipRate(handle, 0); }));
    Require(Throws([&] { sceVideoOutSetWindowModeMargins(handle, 0, 0); }));

    LibcRunShutdown_nid_postfix();
    return 0;
}
