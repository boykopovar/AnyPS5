#include "prx/libSceVideoOut/include/KeyboardInput.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

#include "SDL_keyboard.h"

void KeyboardInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    KeyboardInputEvent input{};
    switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (event.key.windowID != windowId || !focused || event.key.repeat != 0)
            return;
        if (event.key.keysym.scancode <= SDL_SCANCODE_UNKNOWN || event.key.keysym.scancode > SDL_SCANCODE_RGUI)
            return;
        input.keyCode = static_cast<std::uint16_t>(event.key.keysym.scancode);
        input.pressed = event.type == SDL_KEYDOWN;
        if ((event.key.keysym.mod & KMOD_NUM) != 0)
            input.led |= KEYBOARD_LED_NUM_LOCK;
        if ((event.key.keysym.mod & KMOD_CAPS) != 0)
            input.led |= KEYBOARD_LED_CAPS_LOCK;
        if ((event.key.keysym.mod & KMOD_SCROLL) != 0)
            input.led |= KEYBOARD_LED_SCROLL_LOCK;
        break;
    case SDL_WINDOWEVENT:
        if (event.window.windowID != windowId)
            return;
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            focused = false;
            input.resetKeys = true;
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
    KeyboardPublishInput_nid_postfix(input);
}
