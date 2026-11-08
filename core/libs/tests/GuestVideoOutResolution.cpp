#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

#undef main

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;

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

static bool RejectsStatus(int handle, VideoOutResolutionStatus* status) {
    try {
        sceVideoOutGetResolutionStatus(handle, status);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? std::string_view(argv[1]) : std::string_view{};
    const bool fourK = mode == "four-k";
    const bool custom = mode == "custom";
    if (fourK) {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", "3840x2160");
    } else if (custom) {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", "1280x720");
    } else {
        SetEnvironment("APS5_OUTPUT_RESOLUTION", "1920x1080");
    }

    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    VideoOutResolutionStatus status;
    std::memset(&status, 0xff, sizeof(status));
    Require(sceVideoOutGetResolutionStatus(handle, &status) == 0);
    if (fourK) {
        Require(status.fullWidth == 3840 && status.fullHeight == 2160);
    } else if (custom) {
        Require(status.fullWidth == 1280 && status.fullHeight == 720);
    } else {
        Require(status.fullWidth == 1920 && status.fullHeight == 1080);
    }
    Require(status.paneWidth == status.fullWidth && status.paneHeight == status.fullHeight);
    Require(status.screenSizeInInch == 0.0f && status.flags == 0 && status.reserved0 == 0);
    Require(status.reserved1[0] == 0 && status.reserved1[1] == 0 && status.reserved1[2] == 0);

    VideoOutOutputStatus output{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(status.refreshRate == output.refreshRate);

    Require(RejectsStatus(handle, nullptr));
    Require(RejectsStatus(-1, nullptr));
    Require(RejectsStatus(0, &status));
    Require(RejectsStatus(-1, &status));
    Require(RejectsStatus(NEVER_OPENED_HANDLE, &status));

    Require(sceVideoOutClose(handle) == 0);
    Require(RejectsStatus(handle, &status));
    LibcRunShutdown_nid_postfix();
}
