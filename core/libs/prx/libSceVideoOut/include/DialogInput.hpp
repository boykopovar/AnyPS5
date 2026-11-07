#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_DIALOGINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_DIALOGINPUT_HPP

#include "SDL_events.h"

class DialogInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);
};

#endif
