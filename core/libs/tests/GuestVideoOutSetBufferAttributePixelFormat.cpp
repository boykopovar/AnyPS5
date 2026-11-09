#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
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
    VideoOutBufferAttribute2 attribute2{};
    VideoOutBufferAttribute attribute{};

    ExpectFailure([] { sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 0, 1920, 1080, 0, 0, 0); });
    ExpectFailure([] { sceVideoOutAddBufferAttributeOption(nullptr, 0); });
    ExpectFailure([] { sceVideoOutRemoveBufferAttributeOption(nullptr, 0); });

    sceVideoOutSetBufferAttribute2(&attribute2, 0x8000000000000000ull, 0, 1920, 1080, 0, 0, 0);
    Require(attribute2.pixel_format == 0x8000000000000000ull);
    Require(attribute2.tiling_mode == 0);
    Require(attribute2.width == 1920);
    Require(attribute2.height == 1080);
    Require(attribute2.option == 0);

    sceVideoOutAddBufferAttributeOption(&attribute2, 8);
    Require(attribute2.option == 8);
    sceVideoOutRemoveBufferAttributeOption(&attribute2, 8);
    Require(attribute2.option == 0);

    ExpectFailure([] { sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 0, 0, 1920, 1080, 0); });
    sceVideoOutSetBufferAttribute(&attribute, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attribute.pixel_format == 0x80000000u);
    Require(attribute.tiling_mode == 1);
    Require(attribute.width == 1920);
    Require(attribute.height == 1080);
    Require(attribute.pitch_in_pixel == 1920);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    std::vector<void*> buffers(2);
    for (auto& buf : buffers) {
        buf = std::malloc(1920 * 1080 * 4);
        Require(buf != nullptr);
    }

    int reg = sceVideoOutRegisterBuffers(handle, 0, buffers.data(), 2, &attribute);
    Require(reg >= 0);

    ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute(handle, reg, nullptr); });
    
    VideoOutBufferAttribute badAttribute = attribute;
    badAttribute.aspect_ratio = 1;
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, reg, &badAttribute) == VIDEO_OUT_ERROR_INVALID_ASPECT_RATIO);

    badAttribute = attribute;
    badAttribute.tiling_mode = 2;
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, reg, &badAttribute) == VIDEO_OUT_ERROR_INVALID_TILING_MODE);

    badAttribute = attribute;
    badAttribute.width = 2000;
    badAttribute.pitch_in_pixel = 1920;
    Require(sceVideoOutSubmitChangeBufferAttribute(handle, reg, &badAttribute) == VIDEO_OUT_ERROR_INVALID_PITCH);

    Require(sceVideoOutUnregisterBuffers(handle, reg) == 0);

    for (auto buf : buffers) {
        std::free(buf);
    }

    Require(sceVideoOutClose(handle) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
