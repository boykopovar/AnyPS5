#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include "SDL.h"
#ifdef _WIN32
#undef main
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

int main(int argc, char** argv) {
    Require(argc == 2, "usage: guest_video_out_info_flag_tests none|info|override");
    const std::string mode = argv[1];
    Require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
    SDL_DisplayMode display{};
    Require(SDL_GetCurrentDisplayMode(0, &display) == 0, SDL_GetError());
    const std::uint32_t measured = (display.w >= 3840 || display.h >= 2160) ? 2u : 1u;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (mode == "override") SetEnvironment("APS5_OUTPUT_RESOLUTION", "1920x1080");
    VideoOutTestEnvironment environment;
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA99008","localizedParameters":{"en-US":{"titleName":"Video-out info test"}},"attribute3":)"
              << (mode == "info" ? 4 : 16) << "}";
        Require(static_cast<bool>(param), "cannot write the param.json fixture");
    }
    int handle;
    try {
        handle = sceVideoOutOpen(255, 0, 0, nullptr);
    } catch (const std::runtime_error& error) {
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    VideoOutOutputStatus status{};
    Require(sceVideoOutGetOutputStatus(handle, &status) == 0, "output status failed");
    if (mode == "none") Require(status.resolution == 2, "a title without the video-out info bit was not given 2160p");
    else if (mode == "info") Require(status.resolution == measured, "a title with the video-out info bit did not get the display's resolution");
    else Require(status.resolution == 1, "APS5_OUTPUT_RESOLUTION did not take precedence over the title's attribute3");
    Require(sceVideoOutClose(handle) == 0, "close failed");
    return 0;
}
