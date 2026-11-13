#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

#undef main

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);
static constexpr int UNAVAILABLE_OUTPUT_MODE = static_cast<int>(0x80290019);
static constexpr std::uint64_t TILING_MODE_LINEAR = 0;
static constexpr std::uint64_t PIXEL_FORMAT = 0x8000000022000000ull;

static void Require(bool value) { if (!value) std::abort(); }

static void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0);
#else
    Require(setenv(name, value, 1) == 0);
#endif
}

static void UnsetEnvironment(const char* name) {
#ifdef _WIN32
    Require(_putenv_s(name, "") == 0);
#else
    Require(unsetenv(name) == 0);
#endif
}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? std::string_view(argv[1]) : std::string_view{};
    const bool heightOnly = mode == "height-only";
    const bool widthOnly = mode == "width-only";
    const bool fourK = mode == "four-k";
    const bool malformed = mode == "malformed";
    const bool disabled = mode == "disabled";
    if (disabled || heightOnly) {
        UnsetEnvironment("ANYPS5_VIDEO_OUT_MAX_WIDTH");
    } else if (fourK) {
        SetEnvironment("ANYPS5_VIDEO_OUT_MAX_WIDTH", "3840");
    } else if (malformed) {
        SetEnvironment("ANYPS5_VIDEO_OUT_MAX_WIDTH", "1920px");
    } else {
        SetEnvironment("ANYPS5_VIDEO_OUT_MAX_WIDTH", "1920");
    }
    if (disabled || widthOnly) {
        UnsetEnvironment("ANYPS5_VIDEO_OUT_MAX_HEIGHT");
    } else if (fourK) {
        SetEnvironment("ANYPS5_VIDEO_OUT_MAX_HEIGHT", "2160");
    } else {
        SetEnvironment("ANYPS5_VIDEO_OUT_MAX_HEIGHT", "1080");
    }

    VideoOutTestEnvironment environment;
    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(VideoOutOutputSupport_nid_no_patch(1, &options, 60) == 1);
    Require(VideoOutOutputSupport_nid_no_patch(15, &options, 59) == 0);
    Require(VideoOutOutputSupport_nid_no_patch(15, &options, 120) == 1);
    Require(VideoOutOutputSupport_nid_no_patch(0x4ull, &options, 120) == UNSUPPORTED_OUTPUT_MODE);
    Require(VideoOutOutputSupport_nid_no_patch(0xD000000Aull, &options, 120) == UNKNOWN_OUTPUT_MODE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == 0);
    for (std::uint64_t unknownMode : {0xD000000Aull, 0x8000000000000001ull, ~0ull}) {
        if (malformed) continue;
        if (disabled || fourK) {
            Require(sceVideoOutIsOutputSupported(handle, unknownMode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
            Require(sceVideoOutConfigureOutput(handle, unknownMode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        } else {
            Require(sceVideoOutIsOutputSupported(handle, unknownMode, &options, nullptr, 0) == 0);
            Require(sceVideoOutConfigureOutput(handle, unknownMode, &options, nullptr, 0) == UNAVAILABLE_OUTPUT_MODE);
        }
    }
    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0x10ull, 0x11ull, 0x13ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }

    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, PIXEL_FORMAT, TILING_MODE_LINEAR, 3840, 2160, 0, 0, 0);
    Require(attribute.width == 3840);
    Require(attribute.height == 2160);
    sceVideoOutSetBufferAttribute2(&attribute, PIXEL_FORMAT, TILING_MODE_LINEAR, 1280, 720, 0, 0, 0);
    Require(attribute.width == 1280);
    Require(attribute.height == 720);

    VideoOutBufferAttribute ps4{};
    sceVideoOutSetBufferAttribute(&ps4, 0x80002200u, 1, 0, 3840, 2160, 0);
    Require(ps4.width == 3840);
    Require(ps4.height == 2160);

    std::vector<std::byte> allocation(64u * 1024u * 1024u);
    const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
    const auto aligned = (address + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffer{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 direct{};
    direct.width = 3840;
    direct.height = 2160;
    direct.pixel_format = PIXEL_FORMAT;
    direct.tiling_mode = TILING_MODE_LINEAR;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &direct, 0, nullptr) == 0);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    Require(cfg != nullptr);
    {
        std::lock_guard lock(cfg->mutex);
        Require(cfg->groups[0].attribute.width == 3840);
        Require(cfg->groups[0].attribute.height == 2160);
        Require(cfg->width == 3840);
        Require(cfg->height == 2160);
    }

    VideoOutBufferAttribute2 changed{};
    changed.width = 3840;
    changed.height = 2160;
    changed.pixel_format = PIXEL_FORMAT;
    changed.tiling_mode = TILING_MODE_LINEAR;
    Require(sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &changed, nullptr) == 0);
    {
        std::lock_guard lock(cfg->mutex);
        Require(cfg->groups[0].attribute.width == 3840);
        Require(cfg->groups[0].attribute.height == 2160);
    }

    if (malformed) {
        bool supportedThrew = false;
        try {
            (void)sceVideoOutIsOutputSupported(handle, 0xD000000Aull, &options, nullptr, 0);
        } catch (const std::runtime_error& error) {
            supportedThrew = std::string_view(error.what()).find("ANYPS5_VIDEO_OUT_MAX_WIDTH") != std::string_view::npos;
        }
        Require(supportedThrew);
        bool configureThrew = false;
        try {
            (void)sceVideoOutConfigureOutput(handle, 0xD000000Aull, &options, nullptr, 0);
        } catch (const std::runtime_error& error) {
            configureThrew = std::string_view(error.what()).find("ANYPS5_VIDEO_OUT_MAX_WIDTH") != std::string_view::npos;
        }
        Require(configureThrew);
    }

    Require(sceVideoOutClose(handle) == 0);
    LibcRunShutdown_nid_postfix();
}
