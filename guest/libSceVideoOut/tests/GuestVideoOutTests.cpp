#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int SCE_VIDEO_OUT_ERROR_RESOURCE_BUSY = static_cast<int>(0x80290009);
static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_INDEX = static_cast<int>(0x8029000A);
static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int SCE_VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int SCE_VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static void ExpectFailure(auto&& action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
}

int main() {
    VideoOutTestEnvironment environment;

    VideoOutResolutionStatus resStatus{};
    VideoOutFlipStatus flipStatus{};
    VideoOutOutputStatus outStatus{};
    std::array<std::byte, 80> vrrStatus{};
    std::uint32_t token = 0;
    std::uintptr_t labelAddress = 0;

    ExpectFailure([] {
        sceVideoOutGetResolutionStatus(-1, nullptr);
    });
    ExpectFailure([] {
        sceVideoOutGetResolutionStatus(0, nullptr);
    });
    ExpectFailure([] {
        sceVideoOutGetResolutionStatus(9999, nullptr);
    });
    ExpectFailure([] {
        sceVideoOutGetResolutionStatus(0x7FFFFFFF, nullptr);
    });

    for (int invalidHandle : {-100, -1, 0, 9999, 0x7FFFFFFF}) {
        ExpectFailure([&] { sceVideoOutGetResolutionStatus(invalidHandle, &resStatus); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, 0, 0, 0); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, -1, 0, 0); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, -2, 0, 0); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, 0, 1, 0); });
        ExpectFailure([&] { sceVideoOutSetFlipRate(invalidHandle, 0); });
        ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(invalidHandle); });
        ExpectFailure([&] { sceVideoOutGetOutputStatus(invalidHandle, &outStatus); });

        Require(sceVideoOutClose(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetFlipStatus(invalidHandle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetBufferLabelAddress(invalidHandle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(invalidHandle, 1) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutClearFlipMaster(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    }

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error&) {
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    ExpectFailure([&] {
        sceVideoOutGetResolutionStatus(handle, nullptr);
    });

    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.fullWidth == 1920);
    Require(resStatus.fullHeight == 1080);
    Require(resStatus.paneWidth == 1920);
    Require(resStatus.paneHeight == 1080);
    Require(resStatus.refreshRate == 0);

    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, nullptr); });
    Require(sceVideoOutGetFlipStatus(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutSubmitEopFlip(handle, 0, 0, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -3, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -100, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 16, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 99, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, -1, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 7, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 100, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 0, 0); });

    Require(sceVideoOutSetFlipRate(handle, -1) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);

    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, &resStatus); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -1, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -2, 0, 0); });
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 0); });
    ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); });
    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, &outStatus); });

    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutIsFlipPending(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    LibcRunShutdown_nid_postfix();
    return 0;
}
