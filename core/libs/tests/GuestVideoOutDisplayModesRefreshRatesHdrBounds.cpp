#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

static_assert(sizeof(VideoOutOutputOptions) == 64);
static_assert(sizeof(VideoOutResolutionStatus) == 48);
static_assert(sizeof(VideoOutOutputStatus) == 48);
static_assert(sizeof(VideoOutColorSettings) == 16);
static_assert(sizeof(VideoOutColorSpaceConversionSettings) == 32);

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001u);
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = static_cast<int>(0x80290002u);
static constexpr int VIDEO_OUT_ERROR_RESOURCE_BUSY = static_cast<int>(0x80290009u);
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x8029000Bu);
static constexpr int VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016u);
static constexpr int VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001Eu);
static constexpr uint64_t OUTPUT_MODE_DEFAULT = 1ull;
static constexpr uint64_t OUTPUT_MODE_119_88HZ = 15ull;
static constexpr uint64_t REFRESH_RATE_59_94HZ = 3ull;
static constexpr uint32_t DYNAMIC_RANGE_SDR = 1u;
static constexpr uint32_t DYNAMIC_RANGE_HDR = 2u;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsRuntimeError(auto&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment env;

    VideoOutOutputOptions options{};
    std::memset(&options, 0xCD, sizeof(options));
    Require(sceVideoOutInitializeOutputOptions(nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS);
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(options.internalData[0] == 0x00FF0000u);
    for (std::size_t i = 1; i < 16; ++i) {
        Require(options.internalData[i] == 0u);
    }

    for (int unopened : {-1, 0, 1, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutIsOutputSupported(unopened, OUTPUT_MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(unopened, OUTPUT_MODE_119_88HZ, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, OUTPUT_MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, OUTPUT_MODE_119_88HZ, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
        Require(ThrowsRuntimeError([=] { sceVideoOutSetFlipRate(unopened, 0); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutSetWindowModeMargins(unopened, 0, 0); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutLatencyControlWaitBeforeInput(unopened); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutLatencyMeasureSetStartPoint(unopened, 0); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutSetGlobalBlendSpace(unopened, 0, 0); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutVrrUnpegFromFixedRate(unopened); }));
        Require(ThrowsRuntimeError([=] { sceVideoOutVrrPegToFixedRate(unopened, 0, 0); }));
    }

    VideoOutColorSettings colorSettings{};
    Require(ThrowsRuntimeError([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, std::numeric_limits<float>::quiet_NaN()); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, std::numeric_limits<float>::infinity()); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, -std::numeric_limits<float>::infinity()); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 0.0f); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 0.09f); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, -1.0f); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 2.01f); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 5.0f); }));

    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 0.1f) == 0);
    Require(colorSettings.gamma == 0.1f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.0f) == 0);
    Require(colorSettings.gamma == 2.0f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 1.0f) == 0);
    Require(colorSettings.gamma == 1.0f);

    Require(ThrowsRuntimeError([&] { sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.0f, sizeof(VideoOutColorSettings) - 1); }));
    Require(sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.2f, sizeof(VideoOutColorSettings)) == 0);
    Require(colorSettings.gamma == 1.2f);

    VideoOutColorSpaceConversionSettings cscSettings{};
    std::memset(&cscSettings, 0x7E, sizeof(cscSettings));
    Require(ThrowsRuntimeError([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&cscSettings) == 0);
    for (std::size_t i = 0; i < 8; ++i) {
        Require(cscSettings.internalData[i] == 0u);
    }
    Require(ThrowsRuntimeError([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&cscSettings, 1.5f, 1) == 0);

    Require(ThrowsRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cscSettings, sizeof(cscSettings) - 1); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cscSettings, sizeof(cscSettings)) == 0);
    Require(ThrowsRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cscSettings, 1.0f, 0, sizeof(cscSettings) - 1); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cscSettings, 1.0f, 0, sizeof(cscSettings)) == 0);

    struct alignas(4) VideoOutSysModeBuffer {
        std::uint8_t bytes[64];
    };
    VideoOutSysModeBuffer modeBuf{};
    std::memset(&modeBuf, 0, sizeof(modeBuf));
    sceVideoOutSysInitMode(modeBuf.bytes);
    std::uint32_t initSize = 0;
    std::memcpy(&initSize, modeBuf.bytes, sizeof(initSize));
    Require(initSize == 0x20u);
    for (std::size_t i = 4; i < 32; ++i) {
        Require(modeBuf.bytes[i] == 0xFFu);
    }
    Require(sceVideoOutSysModeIsHdr(modeBuf.bytes) == 0);

    for (std::uint32_t val = 0; val < 256; ++val) {
        modeBuf.bytes[6] = static_cast<std::uint8_t>(val);
        const int expectedHdr = (val >= 12 && val <= 17) ? 1 : 0;
        Require(sceVideoOutSysModeIsHdr(modeBuf.bytes) == expectedHdr);
    }

    modeBuf.bytes[6] = 14;
    Require(sceVideoOutSysModeIsHdr(modeBuf.bytes) == 1);

    std::uint32_t flags = 0;
    std::memcpy(modeBuf.bytes + 0x20, &flags, sizeof(flags));
    Require(sceVideoOutSysModeIsSystemDefault(modeBuf.bytes) == 1);
    Require(sceVideoOutSysModeIsVr(modeBuf.bytes) == 0);
    Require(sceVideoOutSysModeIsVrNative(modeBuf.bytes) == 0);
    Require(sceVideoOutSysModeIsVrLegacy(modeBuf.bytes) == 0);

    flags = 1;
    std::memcpy(modeBuf.bytes + 0x20, &flags, sizeof(flags));
    Require(sceVideoOutSysModeIsSystemDefault(modeBuf.bytes) == 0);

    flags = 2;
    std::memcpy(modeBuf.bytes + 0x20, &flags, sizeof(flags));
    Require(sceVideoOutSysModeIsVr(modeBuf.bytes) == 1);
    Require(sceVideoOutSysModeIsVrNative(modeBuf.bytes) == 1);
    Require(sceVideoOutSysModeIsVrLegacy(modeBuf.bytes) == 0);

    flags = 6;
    std::memcpy(modeBuf.bytes + 0x20, &flags, sizeof(flags));
    Require(sceVideoOutSysModeIsVr(modeBuf.bytes) == 1);
    Require(sceVideoOutSysModeIsVrNative(modeBuf.bytes) == 0);
    Require(sceVideoOutSysModeIsVrLegacy(modeBuf.bytes) == 1);

    Require(ThrowsRuntimeError([] { sceVideoOutSysSetHdrMetadata(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutSysIsUserStatusHdr(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutAddBufferHdrPrivilege(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutConfigureOutputEx(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutConfigureOutputMode_(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutConfigureOutputModeEx_(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutGetCurrentOutputMode_(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutSysGetCurrentOutputMode(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutSysConfigureOutputMode(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutSysConfigureOutput(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutSysIsOutputSupported(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutGetPortStatusInfo_(); }));
    Require(ThrowsRuntimeError([] { sceVideoOutGetPortStatusInfoByBusSpecifier_(); }));

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == VIDEO_OUT_ERROR_RESOURCE_BUSY);

    Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, &options, &options, 0) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, &options, nullptr, 1) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_DEFAULT, &options, &options, 0) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_DEFAULT, &options, nullptr, 1) == VIDEO_OUT_ERROR_INVALID_VALUE);

    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions modified = options;
        modified.internalData[word] ^= 1;
        const int expected = (word == 3 ? 1 : VIDEO_OUT_ERROR_INVALID_VALUE);
        Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, &modified, nullptr, 0) == expected);
    }
    VideoOutOutputOptions freeWordOptions = options;
    freeWordOptions.internalData[3] = 0x12345678u;
    Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, &freeWordOptions, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_DEFAULT, &freeWordOptions, nullptr, 0) == 0);

    Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 0);

    const int mode15Support = sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0);
    Require(mode15Support == 0 || mode15Support == 1);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);

    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE);
    }

    Require(ThrowsRuntimeError([&] { sceVideoOutIsOutputSupported(handle, 0x12ull, &options, nullptr, 0); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutConfigureOutput(handle, 0x12ull, &options, nullptr, 0); }));

    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x3Full, 0x100ull, 0xD000000Aull, 0x20001ull, 0x2000Cull, 0x20010ull, ~0ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE);
    }

    VideoOutResolutionStatus resStatus{};
    std::memset(&resStatus, 0xA5, sizeof(resStatus));
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.fullWidth > 0 && resStatus.fullHeight > 0);
    Require(resStatus.paneWidth == resStatus.fullWidth && resStatus.paneHeight == resStatus.fullHeight);
    Require(resStatus.refreshRate == REFRESH_RATE_59_94HZ);
    Require(resStatus.screenSizeInInch == 0.0f);
    Require(resStatus.flags == 0);
    Require(resStatus.reserved0 == 0);
    Require(resStatus.reserved1[0] == 0 && resStatus.reserved1[1] == 0 && resStatus.reserved1[2] == 0);

    VideoOutOutputStatus outStatus{};
    std::memset(&outStatus, 0x5A, sizeof(outStatus));
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.refreshRate == REFRESH_RATE_59_94HZ);
    Require(outStatus.resolution == 1u || outStatus.resolution == 2u);
    Require(outStatus.flags == 0);
    Require(outStatus.reserved[0] == 0 && outStatus.reserved[1] == 0 && outStatus.reserved[2] == 0);

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);
    Require(sceVideoOutSetFlipRate(handle, -1) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 60) == VIDEO_OUT_ERROR_INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);

    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColor(handle, nullptr); }));
    Require(sceVideoOutAdjustColor(handle, &colorSettings) == 0);
    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings) - 1); }));
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)) == 0);

    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion(handle, nullptr); }));
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &cscSettings) == 0);
    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(cscSettings) - 1); }));
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(cscSettings)) == 0);

    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);
    Require(sceVideoOutLatencyControlWaitBeforeInput(handle) == 0);
    Require(sceVideoOutLatencyMeasureSetStartPoint(handle, 0) == 0);

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(ThrowsRuntimeError([&] { sceVideoOutVrrPegToFixedRate(handle, 1, 0); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 1); }));
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DYNAMIC_RANGE_SDR);

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, 600.0f));
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DYNAMIC_RANGE_SDR);

    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DYNAMIC_RANGE_HDR);

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DYNAMIC_RANGE_SDR);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutIsOutputSupported(handle, OUTPUT_MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, OUTPUT_MODE_DEFAULT, &options, nullptr, 0) == VIDEO_OUT_ERROR_INVALID_HANDLE);
    Require(ThrowsRuntimeError([&] { sceVideoOutSetFlipRate(handle, 0); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColor(handle, &colorSettings); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion(handle, &cscSettings); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutVrrUnpegFromFixedRate(handle); }));
    Require(ThrowsRuntimeError([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0); }));

    LibcRunShutdown_nid_postfix();
    return 0;
}
