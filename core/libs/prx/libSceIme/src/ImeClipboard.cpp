#include "prx/libSceIme/include/ImeClipboard.hpp"

#include <SDL.h>

namespace ImeClipboard {

void CopyToHost(const std::u16string& text) {
    const std::string utf8 = Utf8(text);
    SDL_SetClipboardText(utf8.c_str());
}

}
