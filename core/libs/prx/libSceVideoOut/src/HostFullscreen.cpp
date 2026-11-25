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
    std::string sanitized;
    if (value != nullptr) {
        for (const char* p = value; *p != '\0'; ++p) {
            const unsigned char ch = static_cast<unsigned char>(*p);
            if (ch == '\r') {
                sanitized += "\\r";
            } else if (ch == '\n') {
                sanitized += "\\n";
            } else if (ch == '\t') {
                sanitized += "\\t";
            } else if (ch < 0x20 || ch == 0x7f) {
                static constexpr char hex[] = "0123456789abcdef";
                sanitized += "\\x";
                sanitized += hex[ch >> 4];
                sanitized += hex[ch & 0x0f];
            } else {
                sanitized += *p;
            }
        }
    }
    return "DisplayWindow: ANYPS5_FULLSCREEN='" + sanitized + "' is not 0, 1, windowed or borderless; treating it as borderless";
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
    const HostWindowSize current = window.Size();
    window.SetFullscreen(true);
    if (current.width > 0 && current.height > 0) windowedSize = current;
}

void HostFullscreenState::Refresh(bool titleReadsMouse) {
    const bool hide = window.IsFullscreen() && !titleReadsMouse;
    if (hide == cursorHidden) return;
    window.ShowCursor(!hide);
    cursorHidden = hide;
}

void HostFullscreenState::Release() noexcept {
    if (!cursorHidden) return;
    cursorHidden = false;
    try {
        window.ShowCursor(true);
    } catch (...) {
    }
}
