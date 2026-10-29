#include "prx/libSceVideoOut/include/HostPauseSettings.hpp"

#include "SDL_keyboard.h"
#include <stdexcept>
#include <string>

namespace {

SDL_Scancode readKey(const char* value) {
    if (value == nullptr) return SDL_SCANCODE_UNKNOWN;
    const auto key = *value == '\0' ? SDL_SCANCODE_UNKNOWN : SDL_GetScancodeFromName(value);
    if (key == SDL_SCANCODE_UNKNOWN) {
        throw std::runtime_error(std::string("VideoOut: ANYPS5_PAUSE_KEY must be an SDL key name such as F9 or Pause, got \"") + value + "\"");
    }
    return key;
}

bool readOnFocusLoss(const char* value) {
    if (value == nullptr) return false;
    const std::string text = value;
    if (text == "0") return false;
    if (text == "1") return true;
    throw std::runtime_error("VideoOut: ANYPS5_PAUSE_ON_FOCUS_LOSS must be 0 or 1, got \"" + text + "\"");
}

}

HostPauseSettings ReadHostPauseSettings(const char* key, const char* onFocusLoss) {
    HostPauseSettings settings;
    settings.key = readKey(key);
    settings.onFocusLoss = readOnFocusLoss(onFocusLoss);
    return settings;
}
