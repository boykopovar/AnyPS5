#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

class FakeWindow final : public IHostFullscreenWindow {
public:
    bool fullscreen = false;
    bool restoresSize = true;
    bool failTransition = false;
    HostWindowSize size{1280, 720};
    HostWindowSize desktop{1920, 1080};
    HostWindowSize beforeFullscreen{};
    int fullscreenCalls = 0;
    int sizeCalls = 0;

    bool IsFullscreen() const override { return fullscreen; }
    void SetFullscreen(bool value) override {
        if (failTransition) throw std::runtime_error("transition refused");
        fullscreenCalls++;
        if (value) {
            beforeFullscreen = size;
            size = desktop;
        } else if (restoresSize) {
            size = beforeFullscreen;
        }
        fullscreen = value;
    }
    HostWindowSize Size() const override { return size; }
    void SetSize(HostWindowSize value) override {
        sizeCalls++;
        size = value;
    }
};

static void ToggleTest() {
    FakeWindow window;
    HostFullscreenState state(window);
    state.Adopt(HostFullscreenMode::Windowed, window.size);
    Require(window.fullscreenCalls == 0 && window.sizeCalls == 0, "adopting windowed touched the window");
    state.Toggle();
    Require(window.fullscreen && window.size == window.desktop, "toggle did not enter fullscreen");
    state.Toggle();
    Require(!window.fullscreen && window.size == HostWindowSize{1280, 720}, "toggle did not leave fullscreen at the windowed size");
    Require(window.sizeCalls == 0, "a window that restores its own size was resized again");
    window.size = {1000, 562};
    state.Toggle();
    state.Toggle();
    Require(window.size == HostWindowSize{1000, 562}, "a resize between toggles was not remembered");
}

static void RestoreSizeTest() {
    FakeWindow window;
    window.restoresSize = false;
    HostFullscreenState state(window);
    state.Adopt(HostFullscreenMode::Windowed, window.size);
    state.Toggle();
    state.Toggle();
    Require(!window.fullscreen && window.sizeCalls == 1 && window.size == HostWindowSize{1280, 720}, "a window that stays at the desktop size was not restored");
}

static void AdoptTest() {
    FakeWindow created;
    created.fullscreen = true;
    created.size = created.desktop;
    HostFullscreenState started(created);
    started.Adopt(HostFullscreenMode::Borderless, {960, 540});
    Require(created.fullscreenCalls == 0, "adopting a window created fullscreen switched it again");
    started.Toggle();
    Require(!created.fullscreen && created.size == HostWindowSize{960, 540}, "leaving fullscreen from a fullscreen start did not give the creation size");

    FakeWindow plain;
    HostFullscreenState requested(plain);
    requested.Adopt(HostFullscreenMode::Borderless, plain.size);
    Require(plain.fullscreen && plain.fullscreenCalls == 1, "borderless mode did not enter fullscreen");
    requested.Toggle();
    Require(!plain.fullscreen && plain.size == HostWindowSize{1280, 720}, "leaving a borderless start lost the windowed size");
}

static void FailureTest() {
    FakeWindow window;
    HostFullscreenState state(window);
    state.Adopt(HostFullscreenMode::Windowed, window.size);
    window.failTransition = true;
    bool thrown = false;
    try {
        state.Toggle();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown && !window.fullscreen, "a refused transition was swallowed");
    window.failTransition = false;
    state.Toggle();
    Require(window.fullscreen, "toggle did not work after a refused transition");
}

static void ParseTest() {
    Require(ParseHostFullscreenMode(nullptr) == HostFullscreenMode::Windowed, "unset did not stay windowed");
    Require(ParseHostFullscreenMode("") == HostFullscreenMode::Windowed, "empty did not stay windowed");
    Require(ParseHostFullscreenMode("0") == HostFullscreenMode::Windowed, "0 was not windowed");
    Require(ParseHostFullscreenMode("windowed") == HostFullscreenMode::Windowed, "windowed was not windowed");
    Require(ParseHostFullscreenMode("1") == HostFullscreenMode::Borderless, "1 was not borderless");
    Require(ParseHostFullscreenMode("borderless") == HostFullscreenMode::Borderless, "borderless was not borderless");
    for (const char* invalid : {"2", "true", "false", "off", "exclusive", "Borderless", "00", " 1", "1 ", "-1"}) {
        bool thrown = false;
        try {
            ParseHostFullscreenMode(invalid);
        } catch (const std::runtime_error& error) {
            thrown = std::strstr(error.what(), "ANYPS5_FULLSCREEN") != nullptr;
        }
        Require(thrown, (std::string("invalid value did not throw naming the variable: ") + invalid).c_str());
    }
}

int main() {
    ParseTest();
    ToggleTest();
    RestoreSizeTest();
    AdoptTest();
    FailureTest();
    return 0;
}
