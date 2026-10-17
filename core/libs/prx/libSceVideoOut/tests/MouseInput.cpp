#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SDL.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#undef main

extern "C" {
int APS5_VABI sceMouseInit();
int APS5_VABI sceMouseOpen(int userId, std::int32_t type, std::int32_t index, const void* param);
int APS5_VABI sceMouseRead(std::int32_t handle, MouseData* data, std::int32_t num);
int APS5_VABI sceMouseClose(std::int32_t handle);
}

namespace {

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "mouse input: %s\n", reason);
        std::abort();
    }
}

SDL_Event Motion(unsigned windowId, int xrel, int yrel) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = windowId;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    return event;
}

SDL_Event Window(unsigned windowId, std::uint8_t kind) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = windowId;
    event.window.event = kind;
    return event;
}
void CaptureDecision(MouseInput& input, std::int32_t handle) {
    Require(input.CaptureWanted(true), "a focused window with an open mouse does not want the capture");
    Require(!input.CaptureWanted(false), "the pad's mouse-to-stick toggle does not release the capture");
    input.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_LOST), 3);
    Require(!input.CaptureWanted(true), "losing focus does not release the capture");
    input.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3);
    Require(input.CaptureWanted(true), "regaining focus does not restore the capture");
    input.HandleEvent(Window(4, SDL_WINDOWEVENT_FOCUS_LOST), 3);
    Require(input.CaptureWanted(true), "another window's focus loss released the capture");
    Require(sceMouseClose(handle) == 0, "close failed");
    Require(!input.CaptureWanted(true), "a closed mouse keeps the capture");
    Require(sceMouseOpen(255, 0, 0, nullptr) >= 0, "reopen failed");
    Require(input.CaptureWanted(true), "a reopened mouse does not want the capture");
    input.HandleEvent(Window(3, SDL_WINDOWEVENT_CLOSE), 3);
    Require(!input.CaptureWanted(true), "a closed window keeps the capture");
    input.HandleEvent(Window(3, SDL_WINDOWEVENT_FOCUS_GAINED), 3);
}
void CaptureApplied(MouseInput& input) {
    SDL_SetHint("SDL_VIDEODRIVER", "dummy");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "mouse input: SDL video unavailable (%s): capture application not checked\n", SDL_GetError());
        return;
    }
    input.UpdateCapture(true);
    const bool applied = SDL_GetRelativeMouseMode() == SDL_TRUE;
    input.UpdateCapture(false);
    if (applied) {
        Require(SDL_GetRelativeMouseMode() == SDL_TRUE, "mouse-to-stick mode lost relative mouse mode");
        Require(SDL_SetRelativeMouseMode(SDL_FALSE) == 0, "mouse-to-stick mode could not release relative mode");
        input.UpdateCapture(true);
        Require(SDL_GetRelativeMouseMode() == SDL_TRUE, "guest mouse capture was not restored after mouse-to-stick mode");
        input.UpdateCapture(false);
        Require(SDL_SetRelativeMouseMode(SDL_FALSE) == 0, "relative mouse mode could not be reset");
    }
    std::printf("mouse input: SDL %s relative mode under the %s driver\n", applied ? "applied" : "refused", SDL_GetCurrentVideoDriver());
    SDL_Quit();
}
void Sum(std::int32_t handle, std::int32_t& x, std::int32_t& y) {
    MouseData data[MOUSE_MAX_DATA_NUM]{};
    const int count = sceMouseRead(handle, data, MOUSE_MAX_DATA_NUM);
    Require(count >= 0, "read failed");
    x = 0;
    y = 0;
    for (int index = 0; index < count; ++index) {
        x += data[index].x_axis;
        y += data[index].y_axis;
    }
}

}

int main() {
    Require(sceMouseInit() == 0, "init failed");
    const std::int32_t handle = sceMouseOpen(255, 0, 0, nullptr);
    Require(handle >= 0, "open failed");
    MouseInput input;
    std::int32_t x = 0;
    std::int32_t y = 0;

    input.HandleEvent(Motion(3, 10, -7), 3);
    input.HandleEvent(Motion(4, 100, 100), 3);
    Sum(handle, x, y);
    Require(x == 10 && y == -7, "unscaled deltas are not passed through or another window's motion was not ignored");

    input.SetPresentation(1920, 1080, 1843, 1037);
    for (int step = 0; step < 5; ++step) input.HandleEvent(Motion(3, 10, -7), 3);
    Sum(handle, x, y);
    Require(x == 52 && y == -36, "deltas are not scaled by the presented image with sub-pixel carry");

    input.SetPresentation(1920, 1080, 960, 540);
    input.HandleEvent(Motion(3, 1, 1), 3);
    input.HandleEvent(Motion(3, 1, 1), 3);
    Sum(handle, x, y);
    Require(x == 4 && y == 4, "a half-size presentation does not double the deltas");

    input.SetPresentation(0, 0, 0, 0);
    input.HandleEvent(Motion(3, 3, 3), 3);
    Sum(handle, x, y);
    Require(x == 6 && y == 6, "an empty presentation changed the scale");

    {
        SDL_Event leftDown{};
        leftDown.type = SDL_MOUSEBUTTONDOWN;
        leftDown.button.windowID = 3;
        leftDown.button.button = SDL_BUTTON_LEFT;
        leftDown.button.state = SDL_PRESSED;
        input.HandleEvent(leftDown, 3);
        MouseData data{};
        int read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 1, "left mouse button should use guest mouse bit 0");

        SDL_Event leftUp{};
        leftUp.type = SDL_MOUSEBUTTONUP;
        leftUp.button.windowID = 3;
        leftUp.button.button = SDL_BUTTON_LEFT;
        leftUp.button.state = SDL_RELEASED;
        input.HandleEvent(leftUp, 3);
        read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 0, "left mouse button up should clear guest mouse bit 0");

        SDL_Event rightDown{};
        rightDown.type = SDL_MOUSEBUTTONDOWN;
        rightDown.button.windowID = 3;
        rightDown.button.button = SDL_BUTTON_RIGHT;
        rightDown.button.state = SDL_PRESSED;
        input.HandleEvent(rightDown, 3);
        read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 2, "right mouse button should use guest mouse bit 1");

        SDL_Event rightUp{};
        rightUp.type = SDL_MOUSEBUTTONUP;
        rightUp.button.windowID = 3;
        rightUp.button.button = SDL_BUTTON_RIGHT;
        rightUp.button.state = SDL_RELEASED;
        input.HandleEvent(rightUp, 3);
        read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 0, "right mouse button up should clear guest mouse bit 1");

        SDL_Event middleDown{};
        middleDown.type = SDL_MOUSEBUTTONDOWN;
        middleDown.button.windowID = 3;
        middleDown.button.button = SDL_BUTTON_MIDDLE;
        middleDown.button.state = SDL_PRESSED;
        input.HandleEvent(middleDown, 3);
        read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 4, "middle mouse button should use guest mouse bit 2");

        SDL_Event middleUp{};
        middleUp.type = SDL_MOUSEBUTTONUP;
        middleUp.button.windowID = 3;
        middleUp.button.button = SDL_BUTTON_MIDDLE;
        middleUp.button.state = SDL_RELEASED;
        input.HandleEvent(middleUp, 3);
        read = sceMouseRead(handle, &data, 1);
        Require(read == 1, "failed to read mouse data");
        Require(data.buttons == 0, "middle mouse button up should clear guest mouse bit 2");
    }

    CaptureDecision(input, handle);
    CaptureApplied(input);
    std::puts("mouse input tests passed");
    return 0;
}
