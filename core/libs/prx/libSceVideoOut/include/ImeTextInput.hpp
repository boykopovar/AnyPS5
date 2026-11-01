#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_IMETEXTINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_IMETEXTINPUT_HPP

#include "SDL_events.h"

class ImeTextInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);
};

#endif
