#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceVideoOut/include/KeyboardInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"
#include "SDL.h"
#include <cstdio>
#include <cstdlib>
#undef main

static void Require(bool condition, const char* reason) {
    if (!condition) { std::fprintf(stderr, "physical input: %s\n", reason); std::abort(); }
}

int main() {
    Require(std::getenv("APS5_NO_PHYSICAL_INPUT") != nullptr, "gate must be enabled before startup");
    Require(Keyboard::Initialize() == KEYBOARD_OK, "keyboard init");
    Require(Keyboard::Open(1, 0, 0) == KEYBOARD_HANDLE, "keyboard open");
    Require(Mouse::Initialize() == MOUSE_OK, "mouse init");
    Require(Mouse::Open(1, 0, 0, nullptr) == MOUSE_HANDLE, "mouse open");
    Pad::Initialize();
    PadInputState stale{};
    stale.buttons = 0x8000;
    stale.sticks[0] = 0;
    PadPublishInput_nid_postfix(stale);
    PadInput pad;
    MouseInput mouse;
    KeyboardInput keyboard;
    DisplayWindow window;
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.windowID = 7;
    event.key.keysym.scancode = SDL_SCANCODE_A;
    keyboard.HandleEvent(event, 7);
    pad.HandleEvent(event, window);
    event = {};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 7;
    event.motion.xrel = 100;
    mouse.HandleEvent(event, 7);
    pad.HandleEvent(event, window);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_LEFT;
    mouse.HandleEvent(event, 7);
    pad.HandleEvent(event, window);
    event = {};
    event.type = SDL_CONTROLLERDEVICEADDED;
    event.cdevice.which = 0;
    pad.HandleEvent(event, window);
    event.type = SDL_CONTROLLERAXISMOTION;
    event.caxis.value = 32767;
    pad.HandleEvent(event, window);
    pad.Update(window);
    const auto state = Pad::ReadState();
    Require(state.buttons == 0x4000, "physical state not neutral or scripted Cross lost");
    Require(state.left_stick_x == 128, "initial stale stick was not cleared");
    KeyboardData keys{};
    Require(Keyboard::ReadState(KEYBOARD_HANDLE, &keys) == KEYBOARD_OK && keys.length == 1 &&
        keys.key_code[0] == 0 && keys.modifier_key == 0, "physical keyboard leaked");
    MouseData mice[MOUSE_MAX_DATA_NUM]{};
    const auto count = Mouse::Read(MOUSE_HANDLE, mice, MOUSE_MAX_DATA_NUM);
    Require(count >= 0, "mouse read");
    for (int i = 0; i < count; ++i)
        Require(mice[i].buttons == 0 && mice[i].x_axis == 0 && mice[i].y_axis == 0, "physical mouse leaked");
    Require(!mouse.CaptureWanted(true), "physical mouse capture still enabled");
    event = {};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = 7;
    event.window.event = SDL_WINDOWEVENT_CLOSE;
    keyboard.HandleEvent(event, 7);
    mouse.HandleEvent(event, 7);
    pad.HandleEvent(event, window);
    Require(Keyboard::ReadState(KEYBOARD_HANDLE, &keys) == KEYBOARD_OK && !keys.connected, "close lifecycle suppressed");
    const auto closed = Mouse::Read(MOUSE_HANDLE, mice, MOUSE_MAX_DATA_NUM);
    Require(closed > 0 && !mice[closed - 1].connected, "mouse close lifecycle suppressed");
    std::puts("physical input suppressed; script, neutral reset and close lifecycle verified");
}
