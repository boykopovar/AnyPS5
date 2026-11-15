#include "prx/libSceVideoOut/include/DialogInput.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"

#include "SDL_keyboard.h"
#include "SDL_mouse.h"

void DialogInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    ModalDialogInputKind kind;
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.windowID != windowId) return;
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_LEFT:
                case SDL_SCANCODE_UP:
                    kind = ModalDialogInputKind::Previous;
                    break;
                case SDL_SCANCODE_RIGHT:
                case SDL_SCANCODE_DOWN:
                    kind = ModalDialogInputKind::Next;
                    break;
                case SDL_SCANCODE_TAB:
                    kind = (event.key.keysym.mod & KMOD_SHIFT) != 0 ? ModalDialogInputKind::Previous : ModalDialogInputKind::Next;
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                case SDL_SCANCODE_SPACE:
                    if (event.key.repeat != 0) return;
                    kind = ModalDialogInputKind::Confirm;
                    break;
                case SDL_SCANCODE_ESCAPE:
                    if (event.key.repeat != 0) return;
                    kind = ModalDialogInputKind::Cancel;
                    break;
                default:
                    return;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.windowID != windowId || event.wheel.y == 0) return;
            kind = event.wheel.y > 0 ? ModalDialogInputKind::Previous : ModalDialogInputKind::Next;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.windowID != windowId) return;
            if (event.button.button == SDL_BUTTON_LEFT) kind = ModalDialogInputKind::Confirm;
            else if (event.button.button == SDL_BUTTON_RIGHT) kind = ModalDialogInputKind::Cancel;
            else return;
            break;
        default:
            return;
    }
    ModalDialogPublishInput_nid_postfix(kind);
}
