#include "prx/libSceMouse/include/Api.hpp"
#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SDL.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#undef main

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

SDL_Event Wheel(unsigned windowId, int y) {
    SDL_Event event{};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = windowId;
    event.wheel.y = y;
    return event;
}

SDL_Event Button(unsigned windowId, std::uint8_t button, bool pressed) {
    SDL_Event event{};
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.windowID = windowId;
    event.button.button = button;
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

struct Drained {
    int count = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t wheel = 0;
    std::uint32_t buttons = 0;
};

Drained Drain(std::int32_t handle) {
    MouseData data[MOUSE_MAX_DATA_NUM]{};
    const int count = sceMouseRead(handle, data, MOUSE_MAX_DATA_NUM);
    Require(count >= 0, "read failed");
    Drained drained;
    drained.count = count;
    for (int index = 0; index < count; ++index) {
        drained.x += data[index].x_axis;
        drained.y += data[index].y_axis;
        drained.wheel += data[index].wheel;
        drained.buttons = data[index].buttons;
    }
    return drained;
}

void OpenDialog() {
    ModalDialogRequest request;
    request.message = u"Choose";
    request.buttons = {u"Yes", u"No"};
    Require(ModalDialogOpen_nid_postfix(request), "the dialog did not open");
}

void DialogButtonPress(MouseInput& input, std::int32_t handle) {
    static_cast<void>(Drain(handle));
    ModalDialogSetPresenter_nid_postfix(true);
    OpenDialog();
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, true), 3);
    Require(Drain(handle).buttons == 0, "a button press reached the title while a dialog was shown");
    ModalDialogClose_nid_postfix();
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, true), 3);
    Require(Drain(handle).buttons == 1, "a button press does not reach the title after the dialog closed");
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, false), 3);
    Require(Drain(handle).buttons == 0, "a button release does not reach the title");
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

void DialogButtonRelease(MouseInput& input, std::int32_t handle) {
    static_cast<void>(Drain(handle));
    ModalDialogSetPresenter_nid_postfix(true);
    input.HandleEvent(Button(3, SDL_BUTTON_RIGHT, true), 3);
    Require(Drain(handle).buttons == 2, "a button press does not reach the title without a dialog");
    OpenDialog();
    input.HandleEvent(Button(3, SDL_BUTTON_RIGHT, false), 3);
    auto drained = Drain(handle);
    Require(drained.count == 1 && drained.buttons == 0, "the held button was not released when the dialog started taking clicks");
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, true), 3);
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, false), 3);
    Require(Drain(handle).count == 0, "a button press or release reached the title while a dialog was shown");
    ModalDialogClose_nid_postfix();
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, true), 3);
    Require(Drain(handle).buttons == 1, "a button press does not reach the title after the dialog closed");
    input.HandleEvent(Button(3, SDL_BUTTON_LEFT, false), 3);
    Require(Drain(handle).buttons == 0, "a button release does not reach the title after the dialog closed");
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

void DialogMotionAndWheel(MouseInput& input, std::int32_t handle) {
    static_cast<void>(Drain(handle));
    ModalDialogSetPresenter_nid_postfix(true);
    input.HandleEvent(Wheel(3, 2), 3);
    auto drained = Drain(handle);
    Require(drained.wheel == 2, "the wheel does not reach the title without a dialog");
    OpenDialog();
    input.HandleEvent(Motion(3, 5, 5), 3);
    input.HandleEvent(Wheel(3, 2), 3);
    drained = Drain(handle);
    Require(drained.count == 0, "motion or wheel reached the title while a dialog was shown");
    ModalDialogClose_nid_postfix();
    input.HandleEvent(Motion(3, 4, 6), 3);
    drained = Drain(handle);
    Require(drained.x == 8 && drained.y == 12, "motion does not reach the title after the dialog closed");
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
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
    DialogMotionAndWheel(input, handle);
    DialogButtonPress(input, handle);
    DialogButtonRelease(input, handle);
    CaptureDecision(input, handle);
    CaptureApplied(input);
    std::puts("mouse input tests passed");
    return 0;
}
