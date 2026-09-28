#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "SDL_events.h"
#include "SDL_mouse.h"

#include <cstdlib>

extern "C" {
int APS5_VABI sceMouseInit();
int APS5_VABI sceMouseOpen(int, std::int32_t, std::int32_t, const void*);
int APS5_VABI sceMouseClose(std::int32_t);
int APS5_VABI sceMouseRead(std::int32_t, MouseData*, std::int32_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    MouseData data[64]{};
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseOpen(1, 0, 1, nullptr) == MOUSE_ERROR_INVALID_ARG);
    MouseOpenParam unsupported{};
    unsupported.behaviorFlag = 2;
    Require(sceMouseOpen(1, 0, 0, &unsupported) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_ALREADY_OPENED);
    Require(sceMouseRead(42, data, 1) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseRead(MOUSE_HANDLE, nullptr, 1) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseRead(MOUSE_HANDLE, data, 65) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].buttons == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInput input;
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 7;
    event.motion.xrel = 9;
    event.motion.yrel = -4;
    input.HandleEvent(event, 8);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    input.HandleEvent(event, 7);

    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_RIGHT;
    input.HandleEvent(event, 7);
    event = {};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = 7;
    event.wheel.x = 2;
    event.wheel.y = -3;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 3) == 3);
    Require(data[0].x_axis == 9 && data[0].y_axis == -4 && data[0].buttons == 0);
    Require(data[1].buttons == 2);
    Require(data[2].buttons == 2 && data[2].wheel == 3 && data[2].tilt == -2);
    Require(data[0].timestamp <= data[2].timestamp);
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 7;
    event.button.button = SDL_BUTTON_X1;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    event = {};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = 7;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1);
    Require(data[0].connected && data[0].buttons == 0);
    event.type = SDL_MOUSEMOTION;
    event.motion.xrel = 5;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    event.window.event = SDL_WINDOWEVENT_CLOSE;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && !data[0].connected);
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    input.HandleEvent(event, 7);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].connected);

    for (int i = 0; i < 70; ++i) {
        MouseInputEvent motion{};
        motion.x = i;
        MousePublishInput_nid_postfix(motion);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 64);
    Require(data[0].x_axis == 6 && data[63].x_axis == 69);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_ERROR_INVALID_HANDLE);
    MouseOpenParam merged{};
    merged.behaviorFlag = MOUSE_OPEN_PARAM_MERGED;
    Require(sceMouseOpen(1, 0, 0, &merged) == MOUSE_HANDLE);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
}
