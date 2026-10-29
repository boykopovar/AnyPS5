#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstring>
#include <stdexcept>

HostFullscreenMode ParseHostFullscreenMode(const char* value) {
    if (value == nullptr || *value == '\0') return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "0") == 0 || std::strcmp(value, "windowed") == 0) return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "1") == 0 || std::strcmp(value, "borderless") == 0) return HostFullscreenMode::Borderless;
    throw std::runtime_error("DisplayWindow: ANYPS5_FULLSCREEN must be 0, 1, windowed or borderless");
}

HostFullscreenState::HostFullscreenState(IHostFullscreenWindow& window) : window(window) {}

void HostFullscreenState::Adopt(HostFullscreenMode mode, HostWindowSize size) {
    windowedSize = size;
    if (mode == HostFullscreenMode::Borderless && !window.IsFullscreen()) window.SetFullscreen(true);
}

void HostFullscreenState::Toggle() {
    if (window.IsFullscreen()) {
        window.SetFullscreen(false);
        if (windowedSize.width > 0 && windowedSize.height > 0 && !(window.Size() == windowedSize)) window.SetSize(windowedSize);
        return;
    }
    windowedSize = window.Size();
    window.SetFullscreen(true);
}

void HostFullscreenState::Refresh(bool titleReadsMouse) {
    const bool hide = window.IsFullscreen() && !titleReadsMouse;
    if (hide == cursorHidden) return;
    window.ShowCursor(!hide);
    cursorHidden = hide;
}

void HostFullscreenState::Release() {
    if (!cursorHidden) return;
    window.ShowCursor(true);
    cursorHidden = false;
}
