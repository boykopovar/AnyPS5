#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr std::uint64_t StrictColorimetryOption = 8;

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

void TestGammaBounds() {
    VideoOutColorSettings settings{};

    Require(Throws([] {
        return sceVideoOutColorSettingsSetGamma(nullptr, 1.0f);
    }));
    Require(Throws([] {
        return sceVideoOutColorSettingsSetGamma_(nullptr, 1.0f, sizeof(VideoOutColorSettings));
    }));

    for (const std::uint32_t badSize : {0u, 1u, 4u, 8u, 12u, 15u}) {
        Require(Throws([&settings, badSize] {
            return sceVideoOutColorSettingsSetGamma_(&settings, 1.0f, badSize);
        }));
    }

    const float badGammas[] = {
        -100.0f, -1.0f, -0.001f, 0.0f, 0.05f, 0.0999f,
        2.0001f, 2.05f, 2.5f, 5.0f, 100.0f,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };

    for (const float badGamma : badGammas) {
        Require(Throws([&settings, badGamma] {
            return sceVideoOutColorSettingsSetGamma(&settings, badGamma);
        }));
        Require(Throws([&settings, badGamma] {
            return sceVideoOutColorSettingsSetGamma_(&settings, badGamma, sizeof(VideoOutColorSettings));
        }));
    }

    const float validGammas[] = {0.1f, 0.2f, 0.5f, 0.8f, 1.0f, 1.2f, 1.5f, 1.8f, 2.0f};
    for (const float validGamma : validGammas) {
        Require(sceVideoOutColorSettingsSetGamma(&settings, validGamma) == 0);
        Require(settings.gamma == validGamma);

        Require(sceVideoOutColorSettingsSetGamma_(&settings, validGamma, sizeof(VideoOutColorSettings)) == 0);
        Require(settings.gamma == validGamma);

        Require(sceVideoOutColorSettingsSetGamma_(&settings, validGamma, sizeof(VideoOutColorSettings) + 16) == 0);
        Require(settings.gamma == validGamma);
    }
}

void TestColorSpaceConversionBounds() {
    VideoOutColorSpaceConversionSettings settings{};

    Require(Throws([] {
        return sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr);
    }));
    Require(Throws([] {
        return sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(nullptr, sizeof(VideoOutColorSpaceConversionSettings));
    }));

    for (const std::uint32_t badSize : {0u, 1u, 4u, 8u, 16u, 24u, 31u}) {
        Require(Throws([&settings, badSize] {
            return sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, badSize);
        }));
        Require(Throws([&settings, badSize] {
            return sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, 1.0f, 0, badSize);
        }));
    }

    Require(Throws([] {
        return sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0);
    }));
    Require(Throws([] {
        return sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(nullptr, 1.0f, 0, sizeof(VideoOutColorSpaceConversionSettings));
    }));

    std::memset(&settings, 0x5A, sizeof(settings));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&settings) == 0);
    for (const auto field : settings.internalData) {
        Require(field == 0);
    }

    std::memset(&settings, 0xA5, sizeof(settings));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings)) == 0);
    for (const auto field : settings.internalData) {
        Require(field == 0);
    }

    std::memset(&settings, 0x3C, sizeof(settings));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&settings, sizeof(settings) + 32) == 0);
    for (const auto field : settings.internalData) {
        Require(field == 0);
    }

    for (const float lumaScale : {-10.0f, -1.0f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 100.0f}) {
        for (const std::uint32_t option : {0u, 1u, 2u, 100u, 0xFFFFFFFFu}) {
            Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&settings, lumaScale, option) == 0);
            Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, lumaScale, option, sizeof(settings)) == 0);
            Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&settings, lumaScale, option, sizeof(settings) + 16) == 0);
        }
    }
}

void TestBufferAttributeColorimetryBounds() {
    Require(Throws([] {
        sceVideoOutAddBufferAttributeOption(nullptr, StrictColorimetryOption);
    }));
    Require(Throws([] {
        sceVideoOutRemoveBufferAttributeOption(nullptr, StrictColorimetryOption);
    }));

    VideoOutBufferAttribute2 attribute{};
    attribute.option = 0;

    sceVideoOutAddBufferAttributeOption(&attribute, StrictColorimetryOption);
    Require(attribute.option == StrictColorimetryOption);

    sceVideoOutAddBufferAttributeOption(&attribute, StrictColorimetryOption);
    Require(attribute.option == StrictColorimetryOption);

    sceVideoOutAddBufferAttributeOption(&attribute, 0x20);
    Require(attribute.option == (StrictColorimetryOption | 0x20));

    sceVideoOutRemoveBufferAttributeOption(&attribute, StrictColorimetryOption);
    Require(attribute.option == 0x20);

    sceVideoOutRemoveBufferAttributeOption(&attribute, StrictColorimetryOption);
    Require(attribute.option == 0x20);

    sceVideoOutRemoveBufferAttributeOption(&attribute, 0x20);
    Require(attribute.option == 0);
}

void TestUnopenedHandleBounds() {
    VideoOutColorSettings colorSettings{};
    VideoOutColorSpaceConversionSettings cscSettings{};

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 10, 100, 9999, 0x7FFFFFFF}) {
        Require(Throws([badHandle] {
            return sceVideoOutAdjustColor(badHandle, nullptr);
        }));
        Require(Throws([badHandle, &colorSettings] {
            return sceVideoOutAdjustColor(badHandle, &colorSettings);
        }));

        for (const std::uint32_t badSize : {0u, 1u, 4u, 8u, 15u}) {
            Require(Throws([badHandle, &colorSettings, badSize] {
                return sceVideoOutAdjustColor_(badHandle, &colorSettings, badSize);
            }));
        }

        Require(Throws([badHandle, &colorSettings] {
            return sceVideoOutAdjustColor_(badHandle, &colorSettings, sizeof(VideoOutColorSettings));
        }));

        Require(Throws([badHandle] {
            return sceVideoOutAdjustColorSpaceConversion(badHandle, nullptr);
        }));
        Require(Throws([badHandle, &cscSettings] {
            return sceVideoOutAdjustColorSpaceConversion(badHandle, &cscSettings);
        }));

        for (const std::uint32_t badSize : {0u, 1u, 8u, 16u, 31u}) {
            Require(Throws([badHandle, &cscSettings, badSize] {
                return sceVideoOutAdjustColorSpaceConversion_(badHandle, &cscSettings, badSize);
            }));
        }

        Require(Throws([badHandle, &cscSettings] {
            return sceVideoOutAdjustColorSpaceConversion_(badHandle, &cscSettings, sizeof(VideoOutColorSpaceConversionSettings));
        }));

        for (const std::uint32_t space : {0u, 1u, 2u, 3u, 10u}) {
            for (const std::uint32_t option : {0u, 1u, 0xFFFFFFFFu}) {
                Require(Throws([badHandle, space, option] {
                    return sceVideoOutSetGlobalBlendSpace(badHandle, space, option);
                }));
            }
        }
    }
}

void TestOpenHandleOperations(int handle) {
    VideoOutColorSettings colorSettings{};
    VideoOutColorSpaceConversionSettings cscSettings{};

    Require(Throws([handle] {
        return sceVideoOutAdjustColor(handle, nullptr);
    }));
    Require(Throws([handle] {
        return sceVideoOutAdjustColor_(handle, nullptr, sizeof(VideoOutColorSettings));
    }));

    for (const std::uint32_t badSize : {0u, 1u, 4u, 8u, 15u}) {
        Require(Throws([handle, &colorSettings, badSize] {
            return sceVideoOutAdjustColor_(handle, &colorSettings, badSize);
        }));
    }

    Require(sceVideoOutAdjustColor(handle, &colorSettings) == 0);
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(VideoOutColorSettings)) == 0);
    Require(sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(VideoOutColorSettings) + 16) == 0);

    Require(Throws([handle] {
        return sceVideoOutAdjustColorSpaceConversion(handle, nullptr);
    }));
    Require(Throws([handle] {
        return sceVideoOutAdjustColorSpaceConversion_(handle, nullptr, sizeof(VideoOutColorSpaceConversionSettings));
    }));

    for (const std::uint32_t badSize : {0u, 1u, 8u, 16u, 31u}) {
        Require(Throws([handle, &cscSettings, badSize] {
            return sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, badSize);
        }));
    }

    Require(sceVideoOutAdjustColorSpaceConversion(handle, &cscSettings) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(VideoOutColorSpaceConversionSettings)) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(VideoOutColorSpaceConversionSettings) + 32) == 0);

    for (const std::uint32_t space : {0u, 1u, 2u, 3u, 10u}) {
        for (const std::uint32_t option : {0u, 1u, 100u}) {
            Require(sceVideoOutSetGlobalBlendSpace(handle, space, option) == 0);
        }
    }
}

void TestClosedHandleBounds(int handle) {
    VideoOutColorSettings colorSettings{};
    VideoOutColorSpaceConversionSettings cscSettings{};

    Require(Throws([handle, &colorSettings] {
        return sceVideoOutAdjustColor(handle, &colorSettings);
    }));
    Require(Throws([handle, &colorSettings] {
        return sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(VideoOutColorSettings));
    }));

    Require(Throws([handle, &cscSettings] {
        return sceVideoOutAdjustColorSpaceConversion(handle, &cscSettings);
    }));
    Require(Throws([handle, &cscSettings] {
        return sceVideoOutAdjustColorSpaceConversion_(handle, &cscSettings, sizeof(VideoOutColorSpaceConversionSettings));
    }));

    for (const std::uint32_t space : {0u, 1u, 2u}) {
        Require(Throws([handle, space] {
            return sceVideoOutSetGlobalBlendSpace(handle, space, 0);
        }));
    }
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestGammaBounds();
    TestColorSpaceConversionBounds();
    TestBufferAttributeColorimetryBounds();
    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
