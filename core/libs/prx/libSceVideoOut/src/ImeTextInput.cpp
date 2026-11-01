#include "prx/libSceVideoOut/include/ImeTextInput.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

#include <cstring>

#include "SDL_keyboard.h"

void ImeTextInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    ImeHostInput input{};
    switch (event.type) {
        case SDL_TEXTINPUT:
            if (event.text.windowID != windowId) return;
            static_assert(sizeof(input.text) >= sizeof(event.text.text));
            std::memcpy(input.text, event.text.text, sizeof(event.text.text));
            input.kind = ImeHostInputKind::Text;
            break;
        case SDL_KEYDOWN:
            if (event.key.windowID != windowId) return;
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_BACKSPACE: input.kind = ImeHostInputKind::Backspace; break;
                case SDL_SCANCODE_DELETE: input.kind = ImeHostInputKind::Delete; break;
                case SDL_SCANCODE_LEFT: input.kind = ImeHostInputKind::Left; break;
                case SDL_SCANCODE_RIGHT: input.kind = ImeHostInputKind::Right; break;
                case SDL_SCANCODE_HOME: input.kind = ImeHostInputKind::Home; break;
                case SDL_SCANCODE_END: input.kind = ImeHostInputKind::End; break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    if (event.key.repeat != 0) return;
                    input.kind = ImeHostInputKind::Enter;
                    break;
                case SDL_SCANCODE_ESCAPE:
                    if (event.key.repeat != 0) return;
                    input.kind = ImeHostInputKind::Escape;
                    break;
                default: return;
            }
            break;
        default:
            return;
    }
    ImePublishInput_nid_postfix(input);
}
