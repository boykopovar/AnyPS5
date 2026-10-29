#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    bool cursorShown = true;
    int cursorCalls = 0;
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
    void ShowCursor(bool show) override {
        cursorCalls++;
        cursorShown = show;
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

static void CursorTest() {
    FakeWindow window;
    HostFullscreenState state(window);
    state.Adopt(HostFullscreenMode::Windowed, window.size);
    state.Refresh(false);
    state.Refresh(true);
    Require(window.cursorCalls == 0 && window.cursorShown, "a windowed run touched the cursor");
    state.Toggle();
    state.Refresh(true);
    Require(window.cursorCalls == 0 && window.cursorShown, "the cursor was hidden for a title that reads the mouse");
    state.Refresh(false);
    Require(window.cursorCalls == 1 && !window.cursorShown, "the cursor stayed visible in fullscreen with no mouse in use");
    state.Refresh(false);
    Require(window.cursorCalls == 1, "an unchanged policy touched the cursor again");
    state.Refresh(true);
    Require(window.cursorCalls == 2 && window.cursorShown, "the cursor stayed hidden after the title opened the mouse");
    state.Refresh(false);
    Require(!window.cursorShown, "the cursor was not hidden again");
    state.Toggle();
    state.Refresh(false);
    Require(window.cursorShown, "the cursor stayed hidden after leaving fullscreen");
    const int calls = window.cursorCalls;
    state.Refresh(false);
    state.Release();
    Require(window.cursorCalls == calls, "Release touched a cursor that was already shown");
    state.Toggle();
    state.Refresh(false);
    Require(!window.cursorShown, "the cursor was not hidden on the second fullscreen");
    state.Release();
    Require(window.cursorShown, "Release did not show the cursor it hid");
    state.Release();
    Require(window.cursorCalls == calls + 2, "a second Release touched the cursor");
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

static bool Fullscreen(const DisplayWindow& window) {
    return (SDL_GetWindowFlags(window.Handle()) & SDL_WINDOW_FULLSCREEN) != 0;
}

static bool Open(DisplayWindow& window) {
    try {
        window.Ensure(640, 360);
    } catch (const std::exception& error) {
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return false;
    }
    return true;
}

static int WindowTest() {
    const auto directory = std::filesystem::temp_directory_path() / "anyps5_host_fullscreen_window";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    std::ofstream(directory / "app0" / "sce_sys" / "param.json") << R"({"titleId":"PPSA00000","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Fullscreen Test"}}})";
    std::filesystem::current_path(directory);
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::printf("skipped, no display or Vulkan device: SDL video driver: %s\n", SDL_GetError());
        return 77;
    }
    SDL_setenv("ANYPS5_FULLSCREEN", "exclusive", 1);
    {
        DisplayWindow window;
        bool thrown = false;
        try {
            window.Ensure(640, 360);
        } catch (const std::exception& error) {
            thrown = std::strstr(error.what(), "ANYPS5_FULLSCREEN") != nullptr;
            if (!thrown && std::strstr(error.what(), "ANYPS5") == nullptr) {
                std::printf("skipped, no display or Vulkan device: %s\n", error.what());
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                return 77;
            }
        }
        Require(thrown && window.Handle() == nullptr, "an invalid ANYPS5_FULLSCREEN did not stop window creation");
    }
    SDL_setenv("ANYPS5_FULLSCREEN", "windowed", 1);
    HostWindowSize windowed;
    {
        DisplayWindow window;
        if (!Open(window)) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return 77;
        }
        Require(!Fullscreen(window), "windowed mode opened fullscreen");
        SDL_GetWindowSize(window.Handle(), &windowed.width, &windowed.height);
        window.ToggleFullscreen();
        Require(Fullscreen(window), "F11 did not enter fullscreen");
        Require(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE, "the cursor stayed visible in fullscreen with no mouse open");
        window.UpdateTitle();
        window.ToggleFullscreen();
        Require(!Fullscreen(window), "F11 did not leave fullscreen");
        Require(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE, "the cursor stayed hidden after leaving fullscreen");
        HostWindowSize back;
        SDL_GetWindowSize(window.Handle(), &back.width, &back.height);
        Require(back == windowed, "leaving fullscreen did not restore the windowed size");
    }
    SDL_setenv("ANYPS5_FULLSCREEN", "borderless", 1);
    {
        DisplayWindow window;
        if (!Open(window)) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return 77;
        }
        Require(Fullscreen(window), "borderless mode did not open fullscreen");
        window.ToggleFullscreen();
        HostWindowSize back;
        SDL_GetWindowSize(window.Handle(), &back.width, &back.height);
        Require(!Fullscreen(window) && back == windowed, "leaving a fullscreen start did not give the creation size");
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::filesystem::current_path(directory.parent_path());
    std::filesystem::remove_all(directory);
    return 0;
}

#undef main

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "window") == 0) return WindowTest();
    ParseTest();
    ToggleTest();
    RestoreSizeTest();
    AdoptTest();
    FailureTest();
    CursorTest();
    return 0;
}
