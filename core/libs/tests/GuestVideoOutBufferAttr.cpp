#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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
    VideoOutBufferAttribute attr{};
    std::memset(&attr, 0x5A, sizeof(attr));

    ExpectFailure([] { sceVideoOutSetBufferAttribute(nullptr, 0, 0, 0, 1920, 1080, 0); });

    sceVideoOutSetBufferAttribute(&attr, 1, 0, 0, 1920, 1080, 0);
    Require(attr.pixel_format == 1);
    Require(attr.tiling_mode == 0);
    Require(attr.aspect_ratio == 0);
    Require(attr.width == 1920);
    Require(attr.height == 1080);
    Require(attr.pitch_in_pixel == 0);

    sceVideoOutSetBufferAttribute(&attr, 2, 1, 0, 3840, 2160, 3840);
    Require(attr.pixel_format == 2);
    Require(attr.tiling_mode == 1);
    Require(attr.width == 3840);
    Require(attr.height == 2160);
    Require(attr.pitch_in_pixel == 3840);

    VideoOutBufferAttribute2 attr2{};
    std::memset(&attr2, 0x5A, sizeof(attr2));

    ExpectFailure([] { sceVideoOutSetBufferAttribute2(nullptr, 1, 0, 1920, 1080, 0, 0, 0); });

    sceVideoOutSetBufferAttribute2(&attr2, 1, 0, 1920, 1080, 0, 0, 0);
    Require(attr2.pixel_format == 1);
    Require(attr2.tiling_mode == 0);
    Require(attr2.width == 1920);
    Require(attr2.height == 1080);
    Require(attr2.option == 0);

    ExpectFailure([] { sceVideoOutAddBufferAttributeOption(nullptr, 1); });
    sceVideoOutAddBufferAttributeOption(&attr2, 1);
    Require(attr2.option == 1);

    ExpectFailure([] { sceVideoOutRemoveBufferAttributeOption(nullptr, 1); });
    sceVideoOutRemoveBufferAttributeOption(&attr2, 1);
    Require(attr2.option == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
