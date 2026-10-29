#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTFULLSCREEN_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTFULLSCREEN_HPP

enum class HostFullscreenMode {
    Windowed,
    Borderless,
};

HostFullscreenMode ParseHostFullscreenMode(const char* value);

#endif
