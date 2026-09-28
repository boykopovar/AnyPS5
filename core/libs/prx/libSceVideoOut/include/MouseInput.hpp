#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_MOUSEINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_MOUSEINPUT_HPP

#include "SDL_events.h"

class MouseInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);

private:
    bool focused = true;
};

#endif
