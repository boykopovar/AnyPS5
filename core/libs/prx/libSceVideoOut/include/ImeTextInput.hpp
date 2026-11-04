#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_IMETEXTINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_IMETEXTINPUT_HPP

#include <string>

#include "SDL_events.h"

class ImeTextInput {
public:
    using ClipboardProvider = bool (*)(std::string& text);

    void SetClipboardProvider(ClipboardProvider provider) { clipboard = provider; }
    void HandleEvent(const SDL_Event& event, unsigned windowId);

private:
    ClipboardProvider clipboard = nullptr;
};

#endif
