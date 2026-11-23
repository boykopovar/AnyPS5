#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_WINDOWTITLEUPDATE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_WINDOWTITLEUPDATE_HPP

#include <cstdint>

struct WindowTitleUpdate {
    static bool ShouldPush(std::uint64_t frameNum, bool fpsWindowClosed) {
        return frameNum == 1 || fpsWindowClosed;
    }
};

#endif
