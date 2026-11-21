#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestColorSettingsGammaBounds() {
    Require(Throws([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); }));
    Require(Throws([] { sceVideoOutColorSettingsSetGamma_(nullptr, 1.0f, sizeof(VideoOutColorSettings)); }));

    VideoOutColorSettings settings{};

    for (const uint32_t badSize : {0u, 1u, 4u, 8u, 12u, 15u}) {
        Require(Throws([&] { sceVideoOutColorSettingsSetGamma_(&settings, 1.0f, badSize); }));
    }

    for (const float badGamma : {-1.0f, 0.0f, 0.09f, 2.01f, 3.0f, 100.0f,
                                 std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::infinity(),
                                 -std::numeric_limits<float>::infinity()}) {
        Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&settings, badGamma); }));
        Require(Throws([&] { sceVideoOutColorSettingsSetGamma_(&settings, badGamma, sizeof(VideoOutColorSettings)); }));
    }

    for (const float validGamma : {0.1f, 0.5f, 1.0f, 1.5f, 2.0f}) {
        Require(sceVideoOutColorSettingsSetGamma(&settings, validGamma) == 0);
        Require(settings.gamma == validGamma);

        Require(sceVideoOutColorSettingsSetGamma_(&settings, validGamma, sizeof(VideoOutColorSettings)) == 0);
        Require(settings.gamma == validGamma);
    }
}

void TestColorSpaceConversionBounds() {
    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); }));
    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(nullptr, sizeof(VideoOutColorSpaceConversionSettings)); }));
    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1000.0f, 0); }));
    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(nullptr, 1000.0f, 0, sizeof(VideoOutColorSpaceConversionSettings)); }));

    VideoOutColorSpaceConversionSettings csc{};
    for (const uint32_t badSize : {0u, 1u, 4u, 8u, 16u, 24u, 31u}) {
        Require(Throws([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csc, badSize); }));
        Require(Throws([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csc, 1000.0f, 0, badSize); }));
    }

    std::memset(&csc, 0x5A, sizeof(csc));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&csc) == 0);
    for (const auto val : csc.internalData) Require(val == 0);

    std::memset(&csc, 0x5A, sizeof(csc));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csc, sizeof(csc)) == 0);
    for (const auto val : csc.internalData) Require(val == 0);

    for (const float luma : {0.0f, 100.0f, 350.0f, 1000.0f, 4000.0f, 10000.0f}) {
        for (const uint32_t option : {0u, 1u, 8u}) {
            Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&csc, luma, option) == 0);
            Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csc, luma, option, sizeof(csc)) == 0);
        }
    }
}

void TestMetadataDebtStubs() {
    Require(Throws([] { sceVideoOutSysSetHdrMetadata(); }));
    Require(Throws([] { sceVideoOutSetGamutMetadata_(); }));
}

void TestUnopenedHandleBounds() {
    VideoOutColorSettings color{};
    color.gamma = 1.0f;

    VideoOutColorSpaceConversionSettings csc{};

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 100, 9999, 0x7FFFFFFF}) {
        Require(Throws([badHandle] { return sceVideoOutAdjustColor(badHandle, nullptr); }));
        Require(Throws([badHandle, &color] { return sceVideoOutAdjustColor(badHandle, &color); }));

        Require(Throws([badHandle, &color] { return sceVideoOutAdjustColor_(badHandle, &color, sizeof(color)); }));
        Require(Throws([badHandle, &color] { return sceVideoOutAdjustColor_(badHandle, &color, 0); }));

        Require(Throws([badHandle] { return sceVideoOutAdjustColorSpaceConversion(badHandle, nullptr); }));
        Require(Throws([badHandle, &csc] { return sceVideoOutAdjustColorSpaceConversion(badHandle, &csc); }));

        Require(Throws([badHandle, &csc] { return sceVideoOutAdjustColorSpaceConversion_(badHandle, &csc, sizeof(csc)); }));
        Require(Throws([badHandle, &csc] { return sceVideoOutAdjustColorSpaceConversion_(badHandle, &csc, 0); }));

        Require(Throws([badHandle] { return sceVideoOutSetGlobalBlendSpace(badHandle, 0, 0); }));
        Require(Throws([badHandle] { return sceVideoOutSetGlobalBlendSpace(badHandle, 1, 0); }));
    }
}

void TestOpenHandleOperations(int handle) {
    Require(Throws([handle] { return sceVideoOutAdjustColor(handle, nullptr); }));
    Require(Throws([handle] { return sceVideoOutAdjustColorSpaceConversion(handle, nullptr); }));

    VideoOutColorSettings color{};
    color.gamma = 1.2f;

    for (const uint32_t badSize : {0u, 1u, 4u, 8u, 15u}) {
        Require(Throws([handle, &color, badSize] { return sceVideoOutAdjustColor_(handle, &color, badSize); }));
    }

    Require(sceVideoOutAdjustColor(handle, &color) == 0);
    Require(sceVideoOutAdjustColor_(handle, &color, sizeof(color)) == 0);

    VideoOutColorSpaceConversionSettings csc{};
    for (const uint32_t badSize : {0u, 1u, 4u, 8u, 16u, 31u}) {
        Require(Throws([handle, &csc, badSize] { return sceVideoOutAdjustColorSpaceConversion_(handle, &csc, badSize); }));
    }

    Require(sceVideoOutAdjustColorSpaceConversion(handle, &csc) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &csc, sizeof(csc)) == 0);

    for (const uint32_t space : {0u, 1u, 2u}) {
        for (const uint32_t option : {0u, 1u, 8u}) {
            Require(sceVideoOutSetGlobalBlendSpace(handle, space, option) == 0);
        }
    }
}

void TestClosedHandleBounds(int handle) {
    VideoOutColorSettings color{};
    color.gamma = 1.0f;

    VideoOutColorSpaceConversionSettings csc{};

    Require(Throws([handle] { return sceVideoOutAdjustColor(handle, nullptr); }));
    Require(Throws([handle, &color] { return sceVideoOutAdjustColor(handle, &color); }));
    Require(Throws([handle, &color] { return sceVideoOutAdjustColor_(handle, &color, sizeof(color)); }));

    Require(Throws([handle] { return sceVideoOutAdjustColorSpaceConversion(handle, nullptr); }));
    Require(Throws([handle, &csc] { return sceVideoOutAdjustColorSpaceConversion(handle, &csc); }));
    Require(Throws([handle, &csc] { return sceVideoOutAdjustColorSpaceConversion_(handle, &csc, sizeof(csc)); }));

    Require(Throws([handle] { return sceVideoOutSetGlobalBlendSpace(handle, 0, 0); }));
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestColorSettingsGammaBounds();
    TestColorSpaceConversionBounds();
    TestMetadataDebtStubs();
    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    return 0;
}
