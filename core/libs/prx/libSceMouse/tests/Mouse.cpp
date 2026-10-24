#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "SDL_events.h"
#include "SDL_mouse.h"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
bool MouseIsOpen_nid_no_patch();
int APS5_VABI sceMouseInit();
int APS5_VABI sceMouseOpen(int, std::int32_t, std::int32_t, const void*);
int APS5_VABI sceMouseClose(std::int32_t);
int APS5_VABI sceMouseRead(std::int32_t, MouseData*, std::int32_t);
int APS5_VABI sceMouseResetAbsolutePosition(std::int32_t, std::uint16_t, std::uint16_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    MouseData data[64]{};
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_NOT_INITIALIZED);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseInit() == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseOpen(1, 0, 1, nullptr) == MOUSE_ERROR_INVALID_ARG);
    MouseOpenParam unsupported{};
    unsupported.behaviorFlag = 2;
    Require(sceMouseOpen(1, 0, 0, &unsupported) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_HANDLE);
    Require(MouseIsOpen_nid_no_patch());
    Require(sceMouseOpen(1, 0, 0, nullptr) == MOUSE_ERROR_ALREADY_OPENED);
    Require(sceMouseResetAbsolutePosition(42, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1919, 1079) == MOUSE_OK);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1920, 1079) == MOUSE_ERROR_INVALID_ARG);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 1919, 1080) == MOUSE_ERROR_INVALID_ARG);
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

    for (int i = 0; i < 300; ++i) {
        MouseInputEvent motion{};
        motion.x = -1;
        motion.y = i % 2;
        MousePublishInput_nid_no_patch(motion);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1);
    Require(data[0].x_axis == -300 && data[0].y_axis == 150 && data[0].buttons == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInputEvent still{};
    MousePublishInput_nid_no_patch(still);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    for (int i = 0; i < 300; ++i) {
        MouseInputEvent alternating{};
        alternating.x = i % 2;
        MousePublishInput_nid_no_patch(alternating);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1);
    Require(data[0].x_axis == 150 && data[0].y_axis == 0);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);

    MouseInputEvent zero{};
    MouseInputEvent two{};
    two.x = 2;
    MouseInputEvent three{};
    three.x = 3;
    MouseInputEvent down{};
    down.button = 1;
    down.pressed = true;
    MouseInputEvent up{};
    up.button = 1;
    for (const MouseInputEvent& report : {two, zero, down, zero, three, zero, up, zero}) MousePublishInput_nid_no_patch(report);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 4);
    Require(data[0].x_axis == 2 && data[0].buttons == 0);
    Require(data[1].x_axis == 0 && data[1].buttons == 1);
    Require(data[2].x_axis == 3 && data[2].buttons == 1);
    Require(data[3].x_axis == 0 && data[3].buttons == 0);

    MouseInputEvent step{};
    step.x = 3;
    MousePublishInput_nid_no_patch(step);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent press{};
    press.button = 1;
    press.pressed = true;
    MousePublishInput_nid_no_patch(press);
    MousePublishInput_nid_no_patch(step);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent wheel{};
    wheel.wheel = 1;
    MousePublishInput_nid_no_patch(wheel);
    MousePublishInput_nid_no_patch(step);
    MouseInputEvent release{};
    release.button = 1;
    MousePublishInput_nid_no_patch(release);
    MousePublishInput_nid_no_patch(step);
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 7);
    Require(data[0].x_axis == 6 && data[0].buttons == 0);
    Require(data[1].x_axis == 0 && data[1].buttons == 1);
    Require(data[2].x_axis == 6 && data[2].buttons == 1);
    Require(data[3].wheel == 1 && data[3].x_axis == 0 && data[3].buttons == 1);
    Require(data[4].x_axis == 3 && data[4].wheel == 0 && data[4].buttons == 1);
    Require(data[5].x_axis == 0 && data[5].buttons == 0);
    Require(data[6].x_axis == 3 && data[6].buttons == 0);
    Require(data[2].timestamp >= data[1].timestamp && data[6].timestamp >= data[5].timestamp);

    MouseInputEvent large{};
    large.x = INT32_MAX;
    MousePublishInput_nid_no_patch(large);
    MousePublishInput_nid_no_patch(large);
    Require(sceMouseRead(MOUSE_HANDLE, data, 8) == 1 && data[0].x_axis == INT32_MAX);

    for (int i = 0; i < 70; ++i) {
        MouseInputEvent click{};
        click.button = 1;
        click.pressed = (i % 2) == 0;
        MousePublishInput_nid_no_patch(click);
    }
    Require(sceMouseRead(MOUSE_HANDLE, data, 64) == 64);
    Require(data[0].buttons == 1 && data[63].buttons == 0 && data[62].buttons == 1);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_ERROR_INVALID_HANDLE);
    MouseOpenParam merged{};
    merged.behaviorFlag = MOUSE_OPEN_PARAM_MERGED;
    Require(sceMouseOpen(1, 0, 0, &merged) == MOUSE_HANDLE);
    Require(MouseIsOpen_nid_no_patch());
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_OK);
    Require(sceMouseRead(MOUSE_HANDLE, data, 1) == 1 && data[0].buttons == 0);
    Require(sceMouseClose(MOUSE_HANDLE) == MOUSE_OK);
    Require(!MouseIsOpen_nid_no_patch());
    Require(sceMouseResetAbsolutePosition(MOUSE_HANDLE, 959, 539) == MOUSE_ERROR_INVALID_HANDLE);
}
