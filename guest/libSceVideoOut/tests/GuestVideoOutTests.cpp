#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;

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

    ExpectFailure([] {
        sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    });
    ExpectFailure([] {
        sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    });
    ExpectFailure([] {
        sceVideoOutAddBufferAttributeOption(nullptr, 1);
    });
    ExpectFailure([] {
        sceVideoOutRemoveBufferAttributeOption(nullptr, 1);
    });
    ExpectFailure([] {
        sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr);
    });
    ExpectFailure([] {
        sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0);
    });

    Require(sceVideoOutInitializeOutputOptions(nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);

    VideoOutBufferAttribute attribute{};
    sceVideoOutSetBufferAttribute(&attribute, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attribute.pixel_format == 0x80000000u);
    Require(attribute.width == 1920);
    Require(attribute.height == 1080);

    VideoOutBufferAttribute2 attribute2{};
    sceVideoOutSetBufferAttribute2(&attribute2, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    Require(attribute2.pixel_format == 0x8000000000000000ull);
    Require(attribute2.width == 1920);
    Require(attribute2.height == 1080);

    VideoOutColorSpaceConversionSettings csSettings{};
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&csSettings) == 0);

    void* bufferAddress = nullptr;
    std::array<VideoOutBuffers, 2> dummyBuffers{};
    VideoOutFlipStatus flipStatus{};
    VideoOutOutputStatus outStatus{};
    VideoOutResolutionStatus resStatus{};
    VideoOutVblankStatus vblankStatus{};
    std::array<std::byte, 80> vrrStatus{};
    std::uintptr_t labelAddress = 0;

    for (int invalidHandle : {-100, -1, 0, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutClose(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(invalidHandle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(invalidHandle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetFlipStatus(invalidHandle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetBufferLabelAddress(invalidHandle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(invalidHandle, 1) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutClearFlipMaster(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

        ExpectFailure([&] { sceVideoOutGetOutputStatus(invalidHandle, &outStatus); });
        ExpectFailure([&] { sceVideoOutGetResolutionStatus(invalidHandle, &resStatus); });
        ExpectFailure([&] { sceVideoOutGetVblankStatus(invalidHandle, &vblankStatus); });
        ExpectFailure([&] { sceVideoOutSetFlipRate(invalidHandle, 0); });
        ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(invalidHandle); });
        ExpectFailure([&] { sceVideoOutRegisterBuffers(invalidHandle, 0, &bufferAddress, 1, &attribute); });
        ExpectFailure([&] { sceVideoOutRegisterBuffers2(invalidHandle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
        ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(invalidHandle, 0, &attribute2, nullptr); });
        ExpectFailure([&] { sceVideoOutUnregisterBuffers(invalidHandle, 0); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, 0, 0, 0); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(invalidHandle, &csSettings); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(invalidHandle, &csSettings, sizeof(csSettings)); });
        ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(invalidHandle, 0, 0); });
    }

    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutOpen(0, HMD2_MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    ExpectFailure([] { sceVideoOutOpen(1234, MAIN_BUS, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, 9999, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 1, nullptr); });

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error&) {
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_RESOURCE_BUSY);

    Require(sceVideoOutGetFlipStatus(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutSubmitEopFlip(handle, 0, 0, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutGetVblankStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, nullptr, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, nullptr, 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, nullptr, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, nullptr, nullptr); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, nullptr, sizeof(csSettings)); });

    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, 1, nullptr, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &options, 0) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 1) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, &options, 0) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 1) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x3Full, 0x100ull, 0xD000000Aull, 0x8000000000000001ull, ~0ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
    }

    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
    }

    ExpectFailure([&] {
        sceVideoOutIsOutputSupported(handle, 0x12, &options, nullptr, 0);
    });

    VideoOutOutputOptions invalidOption = options;
    invalidOption.internalData[0] ^= 0x1234;
    Require(sceVideoOutIsOutputSupported(handle, 1, &invalidOption, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    Require(sceVideoOutSetFlipRate(handle, -1) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutRegisterBuffers2(handle, 0, 15, dummyBuffers.data(), 2, &attribute2, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, -1, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 16, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 0, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 17, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 10, &bufferAddress, 10, &attribute); });

    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, -1, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 16, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, -1, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 16, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 0, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 17, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 99, nullptr); });

    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, -1, &attribute2, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 16, &attribute2, nullptr); });
    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, -1); });
    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, 16); });

    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -3, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 16, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, -1, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 7, 0); });

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute2, nullptr); });
    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 0, 0); });
    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, &outStatus); });
    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, &resStatus); });
    ExpectFailure([&] { sceVideoOutGetVblankStatus(handle, &vblankStatus); });
    ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); });
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 0); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, &csSettings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings)); });
    ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });

    LibcRunShutdown_nid_postfix();
    return 0;
}
