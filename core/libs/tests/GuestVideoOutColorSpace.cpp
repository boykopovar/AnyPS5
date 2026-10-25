#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

static void Require(bool value) {
    if (!value) std::abort();
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
    VideoOutColorSpaceConversionSettings settings{};
    std::memset(&settings, 0x5A, sizeof(settings));

    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);
    for (std::size_t index = 0; index < 8; ++index) {
        Require(settings.internalData[index] == 0);
    }

    ExpectFailure([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&settings, 1.0f, 0) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(0, nullptr); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(-1, &settings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(0, &settings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(1, &settings); });

    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings)) == 0);

    ExpectFailure([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings)) == 0);

    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(1, &settings, sizeof(settings) - 1); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(1, &settings, sizeof(settings)); });

    ExpectFailure([] { sceVideoOutSetGlobalBlendSpace(-1, 0, 0); });
    ExpectFailure([] { sceVideoOutSetGlobalBlendSpace(0, 0, 0); });
    ExpectFailure([] { sceVideoOutSetGlobalBlendSpace(1, 0, 0); });

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutAdjustColorSpaceConversion(handle, &settings) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)) == 0);
    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion(handle, &settings); });
    ExpectFailure([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)); });
    ExpectFailure([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });

    LibcRunShutdown_nid_postfix();
    return 0;
}
