#include "SDL_config.h"

#include <cstdio>

int main() {
#ifdef SDL_JOYSTICK_HIDAPI
    std::puts("pad_sdl: SDL drives controllers through HIDAPI");
    return 0;
#else
    std::fputs("pad_sdl: SDL is built without HIDAPI, so a DualSense gets no light bar, touchpad or trigger effects\n", stderr);
    return 1;
#endif
}
