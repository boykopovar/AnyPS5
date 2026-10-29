#include "prx/libSceVideoOut/include/HostPause.hpp"
#include "prx/libSceVideoOut/include/HostPauseSettings.hpp"
#include "SDL_keyboard.h"
#include <chrono>
#include <cstdint>
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

SDL_Event Key(unsigned windowId, SDL_Scancode key, bool down, bool repeat = false) {
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.windowID = windowId;
    event.key.repeat = repeat ? 1 : 0;
    event.key.keysym.scancode = key;
    return event;
}

SDL_Event Window(unsigned windowId, std::uint8_t kind) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = windowId;
    event.window.event = kind;
    return event;
}

struct FakeClock {
    std::chrono::steady_clock::time_point now{std::chrono::seconds(100)};
    HostPause::Clock Get() { return [this] { return now; }; }
};

void KeyToggles() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_F9, false}, clock.Get());
    Require(!pause.Paused(), "a new pause starts paused");
    auto change = pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(change.changed && change.paused && pause.Paused(), "the key does not pause");
    clock.now += std::chrono::milliseconds(2500);
    change = pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(change.changed && !change.paused && !pause.Paused(), "the key does not resume");
    Require(change.pausedFor == std::chrono::milliseconds(2500), "the paused time is not measured with the injected clock");
    clock.now += std::chrono::seconds(7);
    pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    clock.now += std::chrono::milliseconds(40);
    change = pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(change.pausedFor == std::chrono::milliseconds(40), "a second pause is measured from its own start");
}

void KeyIgnored() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_F9, false}, clock.Get());
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, false), 3).changed, "a key release toggled the pause");
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true, true), 3).changed, "a key repeat toggled the pause");
    Require(!pause.HandleEvent(Key(4, SDL_SCANCODE_F9, true), 3).changed, "a key in another window toggled the pause");
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F10, true), 3).changed, "another key toggled the pause");
    Require(!pause.Paused(), "an ignored event left the pause on");
    pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true, true), 3).changed && pause.Paused(), "a key repeat resumed the pause");
    SDL_Event other{};
    other.type = SDL_MOUSEMOTION;
    Require(!pause.HandleEvent(other, 3).changed && pause.Paused(), "a mouse event changed the pause");
}

void NoKeyNoPause() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_UNKNOWN, false}, clock.Get());
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_UNKNOWN, true), 3).changed, "an unset key paused on an unknown scancode");
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3).changed, "an unset key paused on F9");
    Require(!pause.Paused(), "a disabled pause is on");
}

void FocusRule() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_UNKNOWN, true}, clock.Get());
    Require(!pause.Paused(), "a focus rule starts paused");
    auto change = pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3);
    Require(change.changed && change.paused && pause.Paused(), "a focus loss does not pause");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3).changed, "a second focus loss changed the state");
    clock.now += std::chrono::milliseconds(900);
    change = pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3);
    Require(change.changed && !change.paused && !pause.Paused() && change.pausedFor == std::chrono::milliseconds(900), "regaining focus does not resume");
    Require(pause.HandleEvent(Window(3, SDL_WINDOWEVENT_MINIMIZED), 3).paused, "a minimize does not pause");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3).paused, "regaining focus after a minimize does not resume");
}

void FocusIgnored() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_UNKNOWN, true}, clock.Get());
    Require(!pause.HandleEvent(Window(4, SDL_WINDOWEVENT_FOCUS_LOST), 3).changed, "another window's focus loss paused");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_CLOSE), 3).changed, "a close paused");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_SHOWN), 3).changed, "a show paused");
    pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3);
    Require(!pause.HandleEvent(Window(4, SDL_WINDOWEVENT_FOCUS_GAINED), 3).changed && pause.Paused(), "another window's focus gain resumed");
    HostPause off({SDL_SCANCODE_UNKNOWN, false}, clock.Get());
    Require(!off.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3).changed && !off.Paused(), "a focus loss paused with the rule off");
}

void FocusAndKey() {
    FakeClock clock;
    HostPause pause({SDL_SCANCODE_F9, true}, clock.Get());
    pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(pause.Paused(), "the key does not pause beside the focus rule");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3).changed, "a focus loss changed a key pause");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3).changed && pause.Paused(), "regaining focus resumed a key pause");
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3).paused && !pause.Paused(), "the key does not resume after a focus round trip");
    pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3);
    pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3);
    Require(pause.Paused(), "the key press resumed while focus was lost");
    Require(!pause.HandleEvent(Key(3, SDL_SCANCODE_F9, true), 3).changed && pause.Paused(), "a second key press resumed while focus was lost");
    Require(!pause.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3).paused && !pause.Paused(), "regaining focus did not resume once the key was off");
}

}

int main() {
    SettingsDefault();
    SettingsKey();
    SettingsFocus();
    KeyToggles();
    KeyIgnored();
    NoKeyNoPause();
    FocusRule();
    FocusIgnored();
    FocusAndKey();
    std::puts("host pause: all checks passed");
    return 0;
}
