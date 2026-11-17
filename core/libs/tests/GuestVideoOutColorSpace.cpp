#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include "Testing.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;

int main() {
    VideoOutTestEnvironment environment;
    VideoOutColorSpaceConversionSettings settings{};
    std::memset(&settings, 0x5A, sizeof(settings));

    ExpectErr<std::runtime_error>([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); });
    errno = 71;
    ExpectErr<std::runtime_error>([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(reinterpret_cast<VideoOutColorSpaceConversionSettings*>(0x10)); });
    Require(errno == 71);
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);
    for (std::size_t index = 0; index < 8; ++index) {
        Require(settings.internalData[index] == 0);
    }

    ExpectErr<std::runtime_error>([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); });
    errno = 71;
    ExpectErr<std::runtime_error>([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(reinterpret_cast<VideoOutColorSpaceConversionSettings*>(0x10), 1.0f, 0); });
    Require(errno == 71);
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&settings, 1.0f, 0) == 0);

    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(0, nullptr); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(-1, &settings); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(0, &settings); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(1, &settings); });

    ExpectErr<std::runtime_error>([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings)) == 0);

    ExpectErr<std::runtime_error>([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings) - 1); });
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, sizeof(settings)) == 0);

    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion_(1, &settings, sizeof(settings) - 1); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion_(1, &settings, sizeof(settings)); });

    ExpectErr<std::runtime_error>([] { sceVideoOutSetGlobalBlendSpace(-1, 0, 0); });
    ExpectErr<std::runtime_error>([] { sceVideoOutSetGlobalBlendSpace(0, 0, 0); });
    ExpectErr<std::runtime_error>([] { sceVideoOutSetGlobalBlendSpace(1, 0, 0); });

    VideoOutColorSettings colorSettings{};
    ExpectErr<std::runtime_error>([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 0.05f); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 2.5f); });
    errno = 71;
    ExpectErr<std::runtime_error>([] { sceVideoOutColorSettingsSetGamma(reinterpret_cast<VideoOutColorSettings*>(0x10), 1.0f); });
    Require(errno == 71);
    Require(sceVideoOutColorSettingsSetGamma(&colorSettings, 1.2f) == 0);
    Require(colorSettings.gamma == 1.2f);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    VideoOutOutputStatus colorOut{};
    Require(sceVideoOutGetOutputStatus(handle, &colorOut) == 0);
    Require(colorOut.refreshRate == 3 || colorOut.refreshRate == 13);
    Require(colorOut.resolution == 1 || colorOut.resolution == 2);
    Require(colorOut.dynamicRange == 1 || colorOut.dynamicRange == 2);

    Require(sceVideoOutAdjustColorSpaceConversion(handle, &settings) == 0);
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(handle, reinterpret_cast<const VideoOutColorSpaceConversionSettings*>(0x10)); });
    Require(errno == 71);
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColor(handle, reinterpret_cast<const VideoOutColorSettings*>(0x10)); });
    Require(errno == 71);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)) == 0);
    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    Require(sceVideoOutClose(handle) == 0);
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion(handle, &settings); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &settings, sizeof(settings)); });
    ExpectErr<std::runtime_error>([&] { sceVideoOutSetGlobalBlendSpace(handle, 0, 0); });

    LibcRunShutdown_nid_postfix();
    return 0;
}
