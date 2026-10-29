#include "prx/libSceVideoOut/include/HostPauseSettings.hpp"
#include "SDL_keyboard.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#undef main

namespace {

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "host pause: %s\n", reason);
        std::abort();
    }
}

std::string Refusal(const char* key, const char* onFocusLoss) {
    try {
        static_cast<void>(ReadHostPauseSettings(key, onFocusLoss));
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

void SettingsDefault() {
    const auto settings = ReadHostPauseSettings(nullptr, nullptr);
    Require(!settings.Enabled(), "unset variables enable the pause");
    Require(settings.key == SDL_SCANCODE_UNKNOWN && !settings.onFocusLoss, "unset variables set a key or the focus rule");
}

void SettingsKey() {
    Require(ReadHostPauseSettings("F9", nullptr).key == SDL_SCANCODE_F9, "F9 is not the F9 scancode");
    Require(ReadHostPauseSettings("Pause", nullptr).key == SDL_SCANCODE_PAUSE, "Pause is not the Pause scancode");
    Require(ReadHostPauseSettings("F9", nullptr).Enabled(), "a key alone does not enable the pause");
    Require(!ReadHostPauseSettings("F9", nullptr).onFocusLoss, "a key alone enables the focus rule");
    for (const char* bad : {"", "NoSuchKey", "f9 ", "F9,F10"}) {
        const auto message = Refusal(bad, nullptr);
        Require(message.find("ANYPS5_PAUSE_KEY") != std::string::npos, "an invalid key does not name ANYPS5_PAUSE_KEY");
        Require(bad[0] == '\0' || message.find(bad) != std::string::npos, "an invalid key is not quoted back");
    }
}

void SettingsFocus() {
    Require(!ReadHostPauseSettings(nullptr, "0").Enabled(), "0 enables the pause");
    const auto focus = ReadHostPauseSettings(nullptr, "1");
    Require(focus.onFocusLoss && focus.Enabled() && focus.key == SDL_SCANCODE_UNKNOWN, "1 does not enable only the focus rule");
    const auto both = ReadHostPauseSettings("F9", "1");
    Require(both.key == SDL_SCANCODE_F9 && both.onFocusLoss, "a key and the focus rule do not combine");
    for (const char* bad : {"", "2", "true", " 1", "01", "on"}) {
        const auto message = Refusal(nullptr, bad);
        Require(message.find("ANYPS5_PAUSE_ON_FOCUS_LOSS") != std::string::npos, "an invalid focus value does not name ANYPS5_PAUSE_ON_FOCUS_LOSS");
    }
    Require(Refusal("NoSuchKey", "1").find("ANYPS5_PAUSE_KEY") != std::string::npos, "a bad key next to a good focus value is accepted");
    Require(Refusal("F9", "x").find("ANYPS5_PAUSE_ON_FOCUS_LOSS") != std::string::npos, "a bad focus value next to a good key is accepted");
}

}

int main() {
    SettingsDefault();
    SettingsKey();
    SettingsFocus();
    std::puts("host pause: all checks passed");
    return 0;
}
