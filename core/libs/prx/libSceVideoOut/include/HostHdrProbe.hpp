#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTHDRPROBE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTHDRPROBE_HPP

#include "SDL.h"

class HostHdrProbe {
public:
    HostHdrProbe() = default;
    ~HostHdrProbe();
    HostHdrProbe(const HostHdrProbe&) = delete;
    HostHdrProbe& operator=(const HostHdrProbe&) = delete;

    void Poll(SDL_Window* window);
    void Release() noexcept;

private:
    void* factory = nullptr;
    void* monitor = nullptr;
    bool published = false;
};

#endif
