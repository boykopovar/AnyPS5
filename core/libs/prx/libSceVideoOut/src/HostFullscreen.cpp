#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstring>
#include <stdexcept>

HostFullscreenMode ParseHostFullscreenMode(const char* value) {
    if (value == nullptr || *value == '\0') return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "0") == 0 || std::strcmp(value, "windowed") == 0) return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "1") == 0 || std::strcmp(value, "borderless") == 0) return HostFullscreenMode::Borderless;
    throw std::runtime_error("DisplayWindow: ANYPS5_FULLSCREEN must be 0, 1, windowed or borderless");
}
