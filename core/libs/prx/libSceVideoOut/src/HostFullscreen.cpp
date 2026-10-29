#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstring>

HostFullscreenMode ParseHostFullscreenMode(const char* value, bool* unrecognized) {
    if (unrecognized != nullptr) *unrecognized = false;
    if (value == nullptr || *value == '\0') return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "0") == 0 || std::strcmp(value, "windowed") == 0) return HostFullscreenMode::Windowed;
    if (std::strcmp(value, "1") != 0 && std::strcmp(value, "borderless") != 0 && unrecognized != nullptr) *unrecognized = true;
    return HostFullscreenMode::Borderless;
}

std::string HostFullscreenWarning(const char* value) {
    return std::string("DisplayWindow: ANYPS5_FULLSCREEN='") + value + "' is not 0, 1, windowed or borderless; treating it as borderless";
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
