#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_KEYBOARDINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_KEYBOARDINPUT_HPP

#include "SDL_events.h"

class KeyboardInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);

private:
    bool focused = true;
    bool suspended = false;
};

#endif
