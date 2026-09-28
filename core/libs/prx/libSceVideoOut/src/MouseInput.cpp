#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"

#include "SDL_mouse.h"

void MouseInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    MouseInputEvent input{};
    switch (event.type) {
        case SDL_MOUSEMOTION:
            if (event.motion.windowID != windowId || !focused) return;
            input.x = event.motion.xrel;
            input.y = event.motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.windowID != windowId || !focused) return;
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: input.button = 1; break;
                case SDL_BUTTON_RIGHT: input.button = 2; break;
                case SDL_BUTTON_MIDDLE: input.button = 4; break;
                default: return;
            }
            input.pressed = event.type == SDL_MOUSEBUTTONDOWN;
            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.windowID != windowId || !focused) return;
            input.wheel = event.wheel.y;
            input.tilt = event.wheel.x;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                input.wheel = -input.wheel;
                input.tilt = -input.tilt;
            }
            if (input.wheel == 0 && input.tilt == 0) return;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.windowID != windowId) return;
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                focused = false;
                input.resetButtons = true;
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                focused = true;
                input.connectionChange = true;
            } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                focused = false;
                input.connectionChange = true;
                input.connected = false;
            } else {
                return;
            }
            break;
        default:
            return;
    }
    MousePublishInput_nid_postfix(input);
}
