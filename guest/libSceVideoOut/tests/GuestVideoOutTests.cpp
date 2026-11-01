#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

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

    VideoOutColorSettings colorSettings{};
    ExpectFailure([] {
        sceVideoOutColorSettingsSetGamma(nullptr, 1.0f);
    });
    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma(&colorSettings, -1.0f);
    });
    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma(&colorSettings, 0.0f);
    });
    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma(&colorSettings, 0.09f);
    });
    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma(&colorSettings, 2.01f);
    });
    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma(&colorSettings, 10.0f);
    });
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 1.0f) == 0);
    Require(colorSettings.gamma == 1.0f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 0.5f) == 0);
    Require(colorSettings.gamma == 0.5f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.0f) == 0);
    Require(colorSettings.gamma == 2.0f);

    ExpectFailure([&] {
        sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.0f, sizeof(colorSettings) - 1);
    });
    Require(sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.2f, sizeof(colorSettings)) == 0);
    Require(colorSettings.gamma == 1.2f);

    VideoOutColorSpaceConversionSettings csSettings{};
    ExpectFailure([] {
        sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr);
    });
    ExpectFailure([] {
        sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0);
    });
    ExpectFailure([&] {
        sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csSettings, sizeof(csSettings) - 1);
    });
    ExpectFailure([&] {
        sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csSettings, 1.0f, 0, sizeof(csSettings) - 1);
    });

    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&csSettings) == 0);
    for (std::size_t i = 0; i < 8; ++i) {
        Require(csSettings.internalData[i] == 0);
    }
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&csSettings, 1.0f, 0) == 0);
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csSettings, sizeof(csSettings)) == 0);
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csSettings, 1.0f, 0, sizeof(csSettings)) == 0);

    Require(sceVideoOutInitializeOutputOptions(nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);

    VideoOutBufferAttribute attribute{};
    sceVideoOutSetBufferAttribute(&attribute, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attribute.pixel_format == 0x80000000u);
    Require(attribute.tiling_mode == 1);
    Require(attribute.aspect_ratio == 0);
    Require(attribute.width == 1920);
    Require(attribute.height == 1080);
    Require(attribute.pitch_in_pixel == 1920);
    Require(attribute.option == 0);
    Require(attribute.reserved0 == 0);
    Require(attribute.reserved1 == 0);

    VideoOutBufferAttribute2 attribute2{};
    sceVideoOutSetBufferAttribute2(&attribute2, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    Require(attribute2.pixel_format == 0x8000000000000000ull);
    Require(attribute2.tiling_mode == 1);
    Require(attribute2.aspect_ratio == 0);
    Require(attribute2.width == 1920);
    Require(attribute2.height == 1080);
    Require(attribute2.pitch_in_pixel == 0);
    Require(attribute2.option == 0);
    Require(attribute2.dcc_control == 0);
    Require(attribute2.dcc_cb_register_clear_color == 0);

    sceVideoOutAddBufferAttributeOption(&attribute2, 0x10);
    Require(attribute2.option == 0x10);
    sceVideoOutRemoveBufferAttributeOption(&attribute2, 0x10);
    Require(attribute2.option == 0);

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);
    Require(sceDbgVideoOutAddOutputModeVr60Privilege() == 0);

    void* bufferAddress = nullptr;
    std::array<VideoOutBuffers, 2> dummyBuffers{};
    VideoOutFlipStatus flipStatus{};
    VideoOutOutputStatus outStatus{};
    VideoOutResolutionStatus resStatus{};
    VideoOutVblankStatus vblankStatus{};
    std::array<std::byte, 96> vrrStatus{};
    std::uintptr_t labelAddress = 0;

    for (int invalidHandle : {-100, -1, 0, 9999, 0x7FFFFFFF}) {
        ExpectFailure([&] { sceVideoOutGetResolutionStatus(invalidHandle, nullptr); });
        ExpectFailure([&] { sceVideoOutGetResolutionStatus(invalidHandle, &resStatus); });

        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

        ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(invalidHandle, 0, 0); });
        ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(invalidHandle, 1, 0); });
        ExpectFailure([&] { sceVideoOutVrrUnpegFromFixedRate(invalidHandle); });

        Require(sceVideoOutClose(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetFlipStatus(invalidHandle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutGetBufferLabelAddress(invalidHandle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutSetFlipMaster(invalidHandle, 1) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutClearFlipMaster(invalidHandle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

        Require(sceVideoOutIsOutputSupported(invalidHandle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(invalidHandle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

        ExpectFailure([&] { sceVideoOutGetOutputStatus(invalidHandle, &outStatus); });
        ExpectFailure([&] { sceVideoOutGetVblankStatus(invalidHandle, &vblankStatus); });
        ExpectFailure([&] { sceVideoOutSetFlipRate(invalidHandle, 0); });
        ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(invalidHandle); });
        ExpectFailure([&] { sceVideoOutSubmitFlip(invalidHandle, 0, 0, 0); });

        ExpectFailure([&] { sceVideoOutRegisterBuffers(invalidHandle, 0, &bufferAddress, 1, &attribute); });
        ExpectFailure([&] { sceVideoOutRegisterBuffers2(invalidHandle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
        ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(invalidHandle, 0, &attribute2, nullptr); });
        ExpectFailure([&] { sceVideoOutUnregisterBuffers(invalidHandle, 0); });

        ExpectFailure([&] { sceVideoOutAdjustColor(invalidHandle, &colorSettings); });
        ExpectFailure([&] { sceVideoOutAdjustColor_(invalidHandle, &colorSettings, sizeof(colorSettings)); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(invalidHandle, &csSettings); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(invalidHandle, &csSettings, sizeof(csSettings)); });
        ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(invalidHandle, 0, 0); });
    }

    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutOpen(0, HMD2_MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    ExpectFailure([] { sceVideoOutOpen(1234, MAIN_BUS, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(-1, MAIN_BUS, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, 9999, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, -1, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 1, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, -1, nullptr); });

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error&) {
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == SCE_VIDEO_OUT_ERROR_RESOURCE_BUSY);

    ExpectFailure([&] {
        sceVideoOutGetResolutionStatus(handle, nullptr);
    });

    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.fullWidth == 1920);
    Require(resStatus.fullHeight == 1080);
    Require(resStatus.paneWidth == 1920);
    Require(resStatus.paneHeight == 1080);
    Require(resStatus.screenSizeInInch == 0.0f);
    Require(resStatus.flags == 0);
    Require(resStatus.reserved0 == 0);
    Require(resStatus.reserved1[0] == 0 && resStatus.reserved1[1] == 0 && resStatus.reserved1[2] == 0);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    vrrStatus.fill(std::byte{0x7F});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == 0);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(std::memcmp(vrrStatus.data(), &outStatus, sizeof(outStatus)) == 0);
    for (std::size_t offset = sizeof(outStatus); offset < 80; ++offset) {
        Require(vrrStatus[offset] == std::byte{0});
    }
    for (std::size_t offset = 80; offset < vrrStatus.size(); ++offset) {
        Require(vrrStatus[offset] == std::byte{0x7F});
    }

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 1, 0); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 1); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 42, 99); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0x100000000ull, 0); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0x100000000ull); });
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColor(handle, nullptr); });
    Require(sceVideoOutAdjustColor(handle, &colorSettings) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings) - 1); });
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, nullptr); });
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &csSettings) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings) - 1); });
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings)) == 0);

    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    Require(sceVideoOutGetFlipStatus(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutSubmitEopFlip(handle, 0, 0, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);

    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutGetVblankStatus(handle, nullptr); });

    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, nullptr, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, -1, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 16, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 0, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 17, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 10, &bufferAddress, 10, &attribute); });

    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, nullptr, 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, nullptr, 0, nullptr); });
    int dummyOpt = 0;
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, &dummyOpt); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, -1, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 16, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, -1, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 16, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 0, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 17, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 99, nullptr); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, -1, nullptr); });

    Require(sceVideoOutRegisterBuffers2(handle, 0, 15, dummyBuffers.data(), 2, &attribute2, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, nullptr, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute2, &dummyOpt); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, -1, &attribute2, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 16, &attribute2, nullptr); });

    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, -1); });
    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, 16); });

    for (std::uint32_t invalidFormat : {0u, 1u, 0x80000001u, 0x80001000u, 0x80002000u, 0x12345678u, 0x7FFFFFFFu, 0xFFFFFFFFu}) {
        VideoOutBufferAttribute invalidAttribute{};
        sceVideoOutSetBufferAttribute(&invalidAttribute, invalidFormat, 1, 0, 64, 64, 64);
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &invalidAttribute);
        });
    }

    {
        VideoOutBufferAttribute invalidOptAttr{};
        sceVideoOutSetBufferAttribute(&invalidOptAttr, 0x80000000u, 1, 0, 64, 64, 64);
        invalidOptAttr.option = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &invalidOptAttr);
        });
        invalidOptAttr.option = 0;
        invalidOptAttr.reserved0 = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &invalidOptAttr);
        });
        invalidOptAttr.reserved0 = 0;
        invalidOptAttr.reserved1 = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &invalidOptAttr);
        });
        invalidOptAttr.reserved1 = 0;
        invalidOptAttr.tiling_mode = 0;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &invalidOptAttr);
        });
    }

    for (std::uint64_t invalidFormat : {
        0ull,
        1ull,
        0x1234ull,
        0x80000000ull,
        0x8000000011000000ull,
        0x8000000033000000ull,
        0x80000000FFFFFFFFull,
        0x8100000011000000ull,
        0xc001000611000000ull,
        0x8100070411000000ull,
        0x7FFFFFFFFFFFFFFFull,
        0xDEADBEEFCAFE0000ull,
        ~0ull
    }) {
        VideoOutBufferAttribute2 invalidAttribute2{};
        sceVideoOutSetBufferAttribute2(&invalidAttribute2, invalidFormat, 1, 64, 64, 0, 0, 0);
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &invalidAttribute2, 0, nullptr);
        });
    }

    for (auto [width, height] : {std::pair{0u, 64u}, std::pair{64u, 0u}, std::pair{16385u, 64u}, std::pair{64u, 16385u}}) {
        VideoOutBufferAttribute2 dimAttr{};
        sceVideoOutSetBufferAttribute2(&dimAttr, 0x8000000000000000ull, 1, width, height, 0, 0, 0);
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &dimAttr, 0, nullptr);
        });
    }

    for (std::uint32_t invalidTiling : {2u, 3u, 99u}) {
        VideoOutBufferAttribute2 tilingAttr{};
        sceVideoOutSetBufferAttribute2(&tilingAttr, 0x8000000000000000ull, invalidTiling, 64, 64, 0, 0, 0);
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &tilingAttr, 0, nullptr);
        });
    }

    {
        VideoOutBufferAttribute2 badAttr{};
        sceVideoOutSetBufferAttribute2(&badAttr, 0x8000000000000000ull, 1, 64, 64, 0, 0, 0);
        badAttr.aspect_ratio = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.aspect_ratio = 0;
        badAttr.option = 0x1000;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.option = 0;
        badAttr.reserved0 = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.reserved0 = 0;
        badAttr.pad0 = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.pad0 = 0;
        badAttr.reserved1[0] = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.reserved1[0] = 0;
        badAttr.reserved1[1] = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
        badAttr.reserved1[1] = 0;
        badAttr.reserved1[2] = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &badAttr, 0, nullptr);
        });
    }

    {
        VideoOutBufferAttribute2 dccAttr{};
        sceVideoOutSetBufferAttribute2(&dccAttr, 0x8000000000000000ull, 0, 64, 64, 0, 1, 0);
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &dccAttr, 0, nullptr);
        });
        dccAttr.dcc_control = 0;
        dccAttr.dcc_cb_register_clear_color = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &dccAttr, 0, nullptr);
        });
        dccAttr.dcc_cb_register_clear_color = 0;
        dccAttr.dcc_control = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &dccAttr, 1, nullptr);
        });
        dccAttr.tiling_mode = 1;
        ExpectFailure([&] {
            sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &dccAttr, 1, nullptr);
        });
    }

    Require(sceVideoOutSetFlipRate(handle, -1) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);

    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);

    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -3, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, -100, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 16, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 99, 0, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, -1, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 7, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 100, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 0, 0); });

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, nullptr); });
    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, &resStatus); });

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0); });
    ExpectFailure([&] { sceVideoOutVrrUnpegFromFixedRate(handle); });

    ExpectFailure([&] { sceVideoOutAdjustColor(handle, &colorSettings); });
    ExpectFailure([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, &csSettings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings)); });
    ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });

    Require(sceVideoOutGetFlipStatus(handle, &flipStatus) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutIsFlipPending(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddress) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutSetFlipMaster(handle, 1) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutClearFlipMaster(handle) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == SCE_VIDEO_OUT_ERROR_INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &attribute); });
    ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr); });
    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute2, nullptr); });
    ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, 0); });
    ExpectFailure([&] { sceVideoOutSubmitFlip(handle, 0, 0, 0); });
    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, &outStatus); });
    ExpectFailure([&] { sceVideoOutGetVblankStatus(handle, &vblankStatus); });
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 0); });
    ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); });

    LibcRunShutdown_nid_postfix();
    return 0;
}
