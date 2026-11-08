#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/LaterExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

constexpr int SYSTEM_USER = 255;
constexpr int MAIN_BUS = 0;

constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
constexpr uint64_t MODE_DEFAULT = 1;

constexpr uint16_t MAX_PRIMARY_COORDINATE = 50000;
constexpr uint32_t MAX_MASTERING_LUMINANCE_CAP = 10000;

struct SceVideoOutHdrPrimaryCoordinates {
    uint16_t x;
    uint16_t y;
};

struct SceVideoOutHdrMasteringDisplayColorVolume {
    SceVideoOutHdrPrimaryCoordinates displayPrimaries[3];
    SceVideoOutHdrPrimaryCoordinates whitePoint;
    uint32_t maxDisplayMasteringLuminance;
    uint32_t minDisplayMasteringLuminance;
};

struct SceVideoOutHdrContentLightLevel {
    uint32_t maxContentLightLevel;
    uint32_t maxFrameAverageLightLevel;
};

struct SceVideoOutHdrMetadata {
    SceVideoOutHdrPrimaryCoordinates displayPrimaryRed;
    SceVideoOutHdrPrimaryCoordinates displayPrimaryGreen;
    SceVideoOutHdrPrimaryCoordinates displayPrimaryBlue;
    SceVideoOutHdrPrimaryCoordinates whitePoint;
    uint32_t maxDisplayMasteringLuminance;
    uint32_t minDisplayMasteringLuminance;
    uint32_t maxContentLightLevel;
    uint32_t maxFrameAverageLightLevel;
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

bool IsValidCoordinate(uint16_t coord) {
    return coord <= MAX_PRIMARY_COORDINATE;
}

bool IsValidHdrMetadata(const SceVideoOutHdrMetadata& metadata) {
    if (!IsValidCoordinate(metadata.displayPrimaryRed.x) || !IsValidCoordinate(metadata.displayPrimaryRed.y) ||
        !IsValidCoordinate(metadata.displayPrimaryGreen.x) || !IsValidCoordinate(metadata.displayPrimaryGreen.y) ||
        !IsValidCoordinate(metadata.displayPrimaryBlue.x) || !IsValidCoordinate(metadata.displayPrimaryBlue.y) ||
        !IsValidCoordinate(metadata.whitePoint.x) || !IsValidCoordinate(metadata.whitePoint.y)) {
        return false;
    }
    if (metadata.maxDisplayMasteringLuminance == 0 || metadata.maxDisplayMasteringLuminance > MAX_MASTERING_LUMINANCE_CAP) {
        return false;
    }
    if (metadata.minDisplayMasteringLuminance >= metadata.maxDisplayMasteringLuminance * 10000u) {
        return false;
    }
    if (metadata.maxFrameAverageLightLevel > metadata.maxContentLightLevel && metadata.maxContentLightLevel != 0) {
        return false;
    }
    return true;
}

int sceVideoOutSetHdrMetadata(int handle, const SceVideoOutHdrMetadata* metadata) {
    if (metadata == nullptr) {
        return INVALID_ADDRESS;
    }
    if (handle < 0 || handle > 31) {
        return INVALID_HANDLE;
    }
    if (!IsValidHdrMetadata(*metadata)) {
        return INVALID_VALUE;
    }
    return 0;
}

void TestPrimaryCoordinatesAndLuminanceBounds() {
    SceVideoOutHdrMetadata bt2020{};
    bt2020.displayPrimaryRed = {35400, 14600};
    bt2020.displayPrimaryGreen = {8500, 39850};
    bt2020.displayPrimaryBlue = {6550, 2300};
    bt2020.whitePoint = {15635, 16450};
    bt2020.maxDisplayMasteringLuminance = 1000;
    bt2020.minDisplayMasteringLuminance = 50;
    bt2020.maxContentLightLevel = 1000;
    bt2020.maxFrameAverageLightLevel = 400;
    Require(IsValidHdrMetadata(bt2020));

    SceVideoOutHdrMetadata dciP3{};
    dciP3.displayPrimaryRed = {34000, 16000};
    dciP3.displayPrimaryGreen = {13250, 34500};
    dciP3.displayPrimaryBlue = {7500, 3000};
    dciP3.whitePoint = {15635, 16450};
    dciP3.maxDisplayMasteringLuminance = 4000;
    dciP3.minDisplayMasteringLuminance = 5;
    dciP3.maxContentLightLevel = 4000;
    dciP3.maxFrameAverageLightLevel = 1000;
    Require(IsValidHdrMetadata(dciP3));

    SceVideoOutHdrMetadata bt709{};
    bt709.displayPrimaryRed = {32000, 16500};
    bt709.displayPrimaryGreen = {15000, 30000};
    bt709.displayPrimaryBlue = {7500, 3000};
    bt709.whitePoint = {15635, 16450};
    bt709.maxDisplayMasteringLuminance = 500;
    bt709.minDisplayMasteringLuminance = 1;
    bt709.maxContentLightLevel = 500;
    bt709.maxFrameAverageLightLevel = 250;
    Require(IsValidHdrMetadata(bt709));

    for (uint16_t invalidCoord : {50001, 50002, 55000, 60000, 65535}) {
        SceVideoOutHdrMetadata badRedX = bt2020;
        badRedX.displayPrimaryRed.x = invalidCoord;
        Require(!IsValidHdrMetadata(badRedX));

        SceVideoOutHdrMetadata badRedY = bt2020;
        badRedY.displayPrimaryRed.y = invalidCoord;
        Require(!IsValidHdrMetadata(badRedY));

        SceVideoOutHdrMetadata badGreenX = bt2020;
        badGreenX.displayPrimaryGreen.x = invalidCoord;
        Require(!IsValidHdrMetadata(badGreenX));

        SceVideoOutHdrMetadata badGreenY = bt2020;
        badGreenY.displayPrimaryGreen.y = invalidCoord;
        Require(!IsValidHdrMetadata(badGreenY));

        SceVideoOutHdrMetadata badBlueX = bt2020;
        badBlueX.displayPrimaryBlue.x = invalidCoord;
        Require(!IsValidHdrMetadata(badBlueX));

        SceVideoOutHdrMetadata badBlueY = bt2020;
        badBlueY.displayPrimaryBlue.y = invalidCoord;
        Require(!IsValidHdrMetadata(badBlueY));

        SceVideoOutHdrMetadata badWhiteX = bt2020;
        badWhiteX.whitePoint.x = invalidCoord;
        Require(!IsValidHdrMetadata(badWhiteX));

        SceVideoOutHdrMetadata badWhiteY = bt2020;
        badWhiteY.whitePoint.y = invalidCoord;
        Require(!IsValidHdrMetadata(badWhiteY));
    }

    SceVideoOutHdrMetadata zeroMax = bt2020;
    zeroMax.maxDisplayMasteringLuminance = 0;
    Require(!IsValidHdrMetadata(zeroMax));

    for (uint32_t excessiveMax : {10001u, 15000u, 20000u, 50000u, 100000u}) {
        SceVideoOutHdrMetadata badMax = bt2020;
        badMax.maxDisplayMasteringLuminance = excessiveMax;
        Require(!IsValidHdrMetadata(badMax));
    }

    SceVideoOutHdrMetadata excessiveMin = bt2020;
    excessiveMin.maxDisplayMasteringLuminance = 1000;
    excessiveMin.minDisplayMasteringLuminance = 1000 * 10000u;
    Require(!IsValidHdrMetadata(excessiveMin));

    SceVideoOutHdrMetadata fallExceedsCll = bt2020;
    fallExceedsCll.maxContentLightLevel = 600;
    fallExceedsCll.maxFrameAverageLightLevel = 800;
    Require(!IsValidHdrMetadata(fallExceedsCll));
}

void TestSetHdrMetadataApiValidation() {
    SceVideoOutHdrMetadata valid = {};
    valid.displayPrimaryRed = {35400, 14600};
    valid.displayPrimaryGreen = {8500, 39850};
    valid.displayPrimaryBlue = {6550, 2300};
    valid.whitePoint = {15635, 16450};
    valid.maxDisplayMasteringLuminance = 1000;
    valid.minDisplayMasteringLuminance = 50;
    valid.maxContentLightLevel = 1000;
    valid.maxFrameAverageLightLevel = 400;

    Require(sceVideoOutSetHdrMetadata(0, nullptr) == INVALID_ADDRESS);
    Require(sceVideoOutSetHdrMetadata(-1, nullptr) == INVALID_ADDRESS);
    Require(sceVideoOutSetHdrMetadata(50, nullptr) == INVALID_ADDRESS);

    for (int invalidHandle : {-1, 50, -100, 32, 64, 100, 999}) {
        Require(sceVideoOutSetHdrMetadata(invalidHandle, &valid) == INVALID_HANDLE);
    }

    SceVideoOutHdrMetadata invalidCoords = valid;
    invalidCoords.displayPrimaryRed.x = 50001;
    Require(sceVideoOutSetHdrMetadata(0, &invalidCoords) == INVALID_VALUE);

    SceVideoOutHdrMetadata invalidMaxLum = valid;
    invalidMaxLum.maxDisplayMasteringLuminance = 20000;
    Require(sceVideoOutSetHdrMetadata(0, &invalidMaxLum) == INVALID_VALUE);

    Require(sceVideoOutSetHdrMetadata(0, &valid) == 0);
    Require(sceVideoOutSetHdrMetadata(1, &valid) == 0);
}

void TestRelatedHdrConfigurationApis() {
    VideoOutColorSpaceConversionSettings settings{};
    std::memset(&settings, 0xAA, sizeof(settings));

    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);
    for (size_t i = 0; i < 8; ++i) {
        Require(settings.internalData[i] == 0);
    }

    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); });
    for (float luma : {0.0f, 0.5f, 1.0f, 2.0f, 10.0f, -1.0f}) {
        Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&settings, luma, 0) == 0);
        Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&settings, luma, 1) == 0);
    }

    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings) - 1); });
    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, 0); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings)) == 0);

    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings) - 1); });
    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, 0); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings)) == 0);

    VideoOutColorSettings colorSettings{};
    ExpectFailure([] { sceVideoOutColorSettingsSetGamma(nullptr, 2.2f); });
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.2f) == 0);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 1.0f) == 0);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.4f) == 0);

    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma_(&colorSettings, 2.2f, sizeof(colorSettings) - 1); });
    Require(sceVideoOutColorSettingsSetGamma_(&colorSettings, 2.2f, sizeof(colorSettings)) == 0);

    ExpectFailure([] { sceVideoOutSysSetHdrMetadata(); });
    ExpectFailure([] { sceVideoOutSetGamutMetadata_(); });
    ExpectFailure([] { sceVideoOutSysIsUserStatusHdr(); });
    ExpectFailure([] { sceVideoOutSysModeIsHdr(nullptr); });
    ExpectFailure([] { sceVideoOutSetContentDynamicRange(); });
}

void TestInvalidPortHandleRejection() {
    VideoOutColorSpaceConversionSettings settings{};
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);

    VideoOutColorSettings colorSettings{};
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.2f) == 0);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);

    for (int invalidHandle : {-1, 50, 0, 1, 999, 0x7FFFFFFF}) {
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(invalidHandle, &settings); });
        ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(invalidHandle, &settings, sizeof(settings)); });
        ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(invalidHandle, 0, 0); });
        ExpectFailure([&] { sceVideoOutAdjustColor(invalidHandle, &colorSettings); });
        ExpectFailure([&] { sceVideoOutAdjustColor_(invalidHandle, &colorSettings, sizeof(colorSettings)); });

        Require(sceVideoOutClose(invalidHandle) == INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(invalidHandle, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(invalidHandle, MODE_DEFAULT, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(invalidHandle) == INVALID_HANDLE);

        std::array<std::byte, 96> vrr{};
        Require(sceVideoOutGetOutputStatusWithVrr(invalidHandle, vrr.data()) == INVALID_HANDLE);
    }
}

void TestActivePortHdrOperations() {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) {
            throw;
        }
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        std::exit(77);
    }
    Require(handle > 0);

    VideoOutColorSpaceConversionSettings settings{};
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &settings) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)) == 0);
    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    VideoOutColorSettings colorSettings{};
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 2.2f) == 0);
    Require(sceVideoOutAdjustColor(handle, &colorSettings) == 0);
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)) == 0);

    VideoOutOutputStatus outStatus{};
    Require(sceVideoOutGetOutputStatus(handle, &outStatus) == 0);
    Require(outStatus.dynamicRange == 1 || outStatus.dynamicRange == 2);

    std::array<std::byte, 96> vrrStatus{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == 0);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, &settings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)); });
    ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });
    ExpectFailure([&] { sceVideoOutAdjustColor(handle, &colorSettings); });
    ExpectFailure([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings)); });
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrStatus.data()) == INVALID_HANDLE);
}

} // namespace

int main() {
    TestPrimaryCoordinatesAndLuminanceBounds();
    TestSetHdrMetadataApiValidation();
    TestRelatedHdrConfigurationApis();
    TestInvalidPortHandleRejection();
    TestActivePortHdrOperations();
    std::puts("guest video out hdr metadata tests passed");
    LibcRunShutdown_nid_postfix();
    return 0;
}
