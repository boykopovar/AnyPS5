#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
        std::abort();
    }
}

void TestColorSettingsGammaBounds() {
    VideoOutColorSettings settings{};

    for (const float validGamma : {0.1f, 0.5f, 1.0f, 1.5f, 2.0f}) {
        Require(sceVideoOutColorSettingsSetGamma(&settings, validGamma) == 0);
        Require(settings.gamma == validGamma);

        Require(sceVideoOutColorSettingsSetGamma_(&settings, validGamma, sizeof(VideoOutColorSettings)) == 0);
        Require(settings.gamma == validGamma);
    }
}

void TestColorSpaceConversionBounds() {
    VideoOutColorSpaceConversionSettings csc{};

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

void TestOpenHandleOperations(int handle) {
    VideoOutColorSettings color{};
    color.gamma = 1.2f;

    Require(sceVideoOutAdjustColor(handle, &color) == 0);
    Require(sceVideoOutAdjustColor_(handle, &color, sizeof(color)) == 0);

    VideoOutColorSpaceConversionSettings csc{};
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &csc) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &csc, sizeof(csc)) == 0);

    for (const uint32_t space : {0u, 1u, 2u}) {
        for (const uint32_t option : {0u, 1u, 8u}) {
            Require(sceVideoOutSetGlobalBlendSpace(handle, space, option) == 0);
        }
    }
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestColorSettingsGammaBounds();
    TestColorSpaceConversionBounds();

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

    return 0;
}
