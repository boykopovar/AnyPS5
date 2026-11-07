#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int Hmd2MainBus = 0x20;

constexpr int InvalidValue = static_cast<int>(0x80290001);
constexpr int InvalidAddress = static_cast<int>(0x80290002);
constexpr int ResourceBusy = static_cast<int>(0x80290009);
constexpr int InvalidHandle = static_cast<int>(0x8029000B);
constexpr int UnsupportedOutputMode = static_cast<int>(0x80290016);
constexpr int UnavailableOutputMode = static_cast<int>(0x80290019);
constexpr int UnknownOutputMode = static_cast<int>(0x8029001E);

constexpr uint64_t OutputModeDefault = 1;
constexpr uint64_t OutputMode11988Hz = 15;

constexpr uint64_t RefreshRate5994Hz = 3;
constexpr uint64_t RefreshRate11988Hz = 13;

constexpr uint32_t Resolution1080p = 1;
constexpr uint32_t DynamicRangeSdr = 1;
constexpr uint32_t DynamicRangeHdr = 2;

struct VideoOutOpenParam {
    std::uint32_t firstWord;
    std::uint32_t setPriority;
    std::int32_t priority;
    std::uint32_t setAffinity;
    std::uint64_t affinity;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void ExpectFailure(auto&& action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
}

} // namespace

int main(int, char**) {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) {
            throw;
        }
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);
    Require(sceVideoOutOpen(SystemUser, MainBus, 0, nullptr) == ResourceBusy);

    ExpectFailure([] { sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 1, 0, 1920, 1080, 1920); });
    VideoOutBufferAttribute bufferAttr{};
    sceVideoOutSetBufferAttribute(&bufferAttr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(bufferAttr.pixel_format == 0x80000000u);
    Require(bufferAttr.tiling_mode == 1);
    Require(bufferAttr.aspect_ratio == 0);
    Require(bufferAttr.width == 1920);
    Require(bufferAttr.height == 1080);
    Require(bufferAttr.pitch_in_pixel == 1920);

    ExpectFailure([] { sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0); });
    VideoOutBufferAttribute2 bufferAttr2{};
    sceVideoOutSetBufferAttribute2(&bufferAttr2, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    Require(bufferAttr2.pixel_format == 0x8000000000000000ull);
    Require(bufferAttr2.tiling_mode == 1);
    Require(bufferAttr2.width == 1920);
    Require(bufferAttr2.height == 1080);

    ExpectFailure([] { sceVideoOutAddBufferAttributeOption(nullptr, 8); });
    ExpectFailure([] { sceVideoOutRemoveBufferAttributeOption(nullptr, 8); });
    sceVideoOutAddBufferAttributeOption(&bufferAttr2, 8);
    Require(bufferAttr2.option == 8);
    sceVideoOutRemoveBufferAttributeOption(&bufferAttr2, 8);
    Require(bufferAttr2.option == 0);

    Require(sceVideoOutInitializeOutputOptions(nullptr) == InvalidAddress);
    VideoOutOutputOptions options{};
    std::memset(&options, 0xCC, sizeof(options));
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(options.internalData[0] == 0x00FF0000);
    for (std::size_t index = 1; index < std::size(options.internalData); ++index) {
        Require(options.internalData[index] == 0);
    }

    VideoOutColorSettings colorSettings{};
    ExpectFailure([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); });
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 0.05f); });
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 2.05f); });
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, -1.0f); });
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, NAN); });
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 0.1f) == 0);
    Require(colorSettings.gamma == 0.1f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.0f) == 0);
    Require(colorSettings.gamma == 2.0f);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 1.0f) == 0);
    Require(colorSettings.gamma == 1.0f);
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.0f, sizeof(colorSettings) - 1); });
    Require(sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.5f, sizeof(colorSettings)) == 0);
    Require(colorSettings.gamma == 1.5f);

    VideoOutColorSpaceConversionSettings csSettings{};
    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&csSettings) == 0);
    for (std::size_t index = 0; index < std::size(csSettings.internalData); ++index) {
        Require(csSettings.internalData[index] == 0);
    }
    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&csSettings, 1.0f, 0) == 0);
    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csSettings, sizeof(csSettings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csSettings, sizeof(csSettings)) == 0);
    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csSettings, 1.0f, 0, sizeof(csSettings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csSettings, 1.0f, 0, sizeof(csSettings)) == 0);

    Require(sceVideoOutOpen(SystemUser, Hmd2MainBus, 0, nullptr) == InvalidValue);
    Require(sceVideoOutOpen(0, Hmd2MainBus, 0, nullptr) == InvalidValue);
    ExpectFailure([] { sceVideoOutOpen(100, MainBus, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(-1, MainBus, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SystemUser, 3, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SystemUser, -1, 0, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SystemUser, MainBus, 1, nullptr); });
    ExpectFailure([] { sceVideoOutOpen(SystemUser, MainBus, -1, nullptr); });

    VideoOutOpenParam badParam{15, 0, 0, 0, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 2, 0, 0, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 0, 0, 2, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 1, 255, 0, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 1, 768, 0, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 0, 0, 1, 0};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });
    badParam = {16, 0, 0, 1, 0x2000};
    ExpectFailure([&] { sceVideoOutOpen(SystemUser, MainBus, 0, &badParam); });

    for (int unopened : {-1, 0, 999}) {
        ExpectFailure([&] { sceVideoOutClose(unopened); });
        Require(sceVideoOutIsOutputSupported(unopened, OutputModeDefault, &options, nullptr, 0) == InvalidHandle);
        Require(sceVideoOutConfigureOutput(unopened, OutputModeDefault, &options, nullptr, 0) == InvalidHandle);
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, nullptr) == InvalidAddress);
        std::array<std::byte, 96> unopenedVrr{};
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, unopenedVrr.data()) == InvalidHandle);
        VideoOutResolutionStatus dummyRes{};
        ExpectFailure([&] { sceVideoOutGetResolutionStatus(unopened, &dummyRes); });
        VideoOutOutputStatus dummyOut{};
        ExpectFailure([&] { sceVideoOutGetOutputStatus(unopened, &dummyOut); });
        ExpectFailure([&] { sceVideoOutSetFlipRate(unopened, 0); });
        ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(unopened); });
        ExpectFailure([&] { sceVideoOutSetWindowModeMargins(unopened, 0, 0); });
        ExpectFailure([&] { sceVideoOutLatencyControlWaitBeforeInput(unopened); });
        ExpectFailure([&] { sceVideoOutLatencyMeasureSetStartPoint(unopened, 0); });
        ExpectFailure([&] { sceVideoOutVrrUnpegFromFixedRate(unopened); });
        ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(unopened, 0, 0); });
        ExpectFailure([&] { sceVideoOutAdjustColor(unopened, &colorSettings); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(unopened, &csSettings); });
        ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(unopened, 0, 0); });
    }

    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &options, nullptr, 0) == 1);
    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, nullptr, nullptr, 0) == 1);
    const int highFrameRateSupported = sceVideoOutIsOutputSupported(handle, OutputMode11988Hz, &options, nullptr, 0);
    Require(highFrameRateSupported == 1 || highFrameRateSupported == 0);

    for (uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UnsupportedOutputMode);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UnsupportedOutputMode);
    }

    for (uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x100ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UnknownOutputMode);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UnknownOutputMode);
    }

    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &options, &options, 0) == InvalidValue);
    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &options, nullptr, 1) == InvalidValue);
    Require(sceVideoOutConfigureOutput(handle, OutputModeDefault, &options, &options, 0) == InvalidValue);
    Require(sceVideoOutConfigureOutput(handle, OutputModeDefault, &options, nullptr, 1) == InvalidValue);

    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions modified = options;
        modified.internalData[word] ^= 1;
        const int expected = (word == 3) ? 1 : InvalidValue;
        Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &modified, nullptr, 0) == expected);
    }

    VideoOutOutputOptions freeWordOptions = options;
    freeWordOptions.internalData[3] = 0x12345678;
    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &freeWordOptions, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, OutputModeDefault, &options, nullptr, 0) == 0);

    Require(sceVideoOutSetFlipRate(handle, -1) == InvalidValue);
    Require(sceVideoOutSetFlipRate(handle, 3) == InvalidValue);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);

    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);
    Require(sceVideoOutLatencyControlWaitBeforeInput(handle) == 0);
    Require(sceVideoOutLatencyMeasureSetStartPoint(handle, 0) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 1, 0); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 1); });
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);

    ExpectFailure([&] { sceVideoOutAdjustColor(handle, nullptr); });
    Require(sceVideoOutAdjustColor(handle, &colorSettings) == 0);
    ExpectFailure([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings) - 1); });
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, nullptr); });
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &csSettings) == 0);
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings) - 1); });
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings)) == 0);
    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, nullptr); });
    VideoOutResolutionStatus resStatus{};
    std::memset(&resStatus, 0xFF, sizeof(resStatus));
    Require(sceVideoOutGetResolutionStatus(handle, &resStatus) == 0);
    Require(resStatus.fullWidth == 1920 && resStatus.fullHeight == 1080);
    Require(resStatus.paneWidth == 1920 && resStatus.paneHeight == 1080);
    Require(resStatus.screenSizeInInch == 0.0f);
    Require(resStatus.flags == 0 && resStatus.reserved0 == 0);
    Require(resStatus.reserved1[0] == 0 && resStatus.reserved1[1] == 0 && resStatus.reserved1[2] == 0);
    Require(resStatus.refreshRate == RefreshRate5994Hz);

    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, nullptr); });
    VideoOutOutputStatus outStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.resolution == Resolution1080p);
    Require(outStatus.refreshRate == RefreshRate5994Hz);
    Require(outStatus.flags == 0);
    Require(outStatus.dynamicRange == DynamicRangeSdr);

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == InvalidAddress);
    std::array<std::byte, 96> vrrStatus{};
    vrrStatus.fill(std::byte{0xFF});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == 0);
    Require(std::memcmp(vrrStatus.data(), &outStatus, sizeof(outStatus)) == 0);
    for (std::size_t offset = sizeof(outStatus); offset < 80; ++offset) {
        Require(vrrStatus[offset] == std::byte{0});
    }
    for (std::size_t offset = 80; offset < vrrStatus.size(); ++offset) {
        Require(vrrStatus[offset] == std::byte{0xFF});
    }

    Require(sceVideoOutConfigureOutput(handle, OutputMode11988Hz, &options, nullptr, 0) == UnsupportedOutputMode);

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DynamicRangeSdr);

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, 600.0f));
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DynamicRangeSdr);

    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DynamicRangeHdr);
    Require(outStatus.flags == 0);
    Require(outStatus.resolution == Resolution1080p);

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == DynamicRangeSdr);
    AgcDriverSetSurfaceHdr10_nid_postfix(false);

    Require(sceVideoOutClose(handle) == 0);
    ExpectFailure([&] { sceVideoOutClose(handle); });

    ExpectFailure([&] { sceVideoOutGetResolutionStatus(handle, &resStatus); });
    ExpectFailure([&] { sceVideoOutGetOutputStatus(handle, &outStatus); });
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == InvalidHandle);
    Require(sceVideoOutIsOutputSupported(handle, OutputModeDefault, &options, nullptr, 0) == InvalidHandle);
    Require(sceVideoOutConfigureOutput(handle, OutputModeDefault, &options, nullptr, 0) == InvalidHandle);
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 0); });
    ExpectFailure([&] { sceVideoOutAllowOutputResolutionWqhdDetection(handle); });
    ExpectFailure([&] { sceVideoOutSetWindowModeMargins(handle, 0, 0); });
    ExpectFailure([&] { sceVideoOutLatencyControlWaitBeforeInput(handle); });
    ExpectFailure([&] { sceVideoOutLatencyMeasureSetStartPoint(handle, 0); });
    ExpectFailure([&] { sceVideoOutVrrUnpegFromFixedRate(handle); });
    ExpectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0); });
    ExpectFailure([&] { sceVideoOutAdjustColor(handle, &colorSettings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, &csSettings); });
    ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });

    LibcRunShutdown_nid_postfix();
    return 0;
}
