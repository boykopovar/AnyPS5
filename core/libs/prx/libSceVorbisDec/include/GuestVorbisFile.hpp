#ifndef CORE_LIBS_PRX_LIBSCEVORBISDEC_INCLUDE_GUESTVORBISFILE_HPP
#define CORE_LIBS_PRX_LIBSCEVORBISDEC_INCLUDE_GUESTVORBISFILE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace VorbisDec {

struct GuestCallbacks {
    std::size_t (APS5_VABI *read_func)(void*, std::size_t, std::size_t, void*);
    int (APS5_VABI *seek_func)(void*, std::int64_t, int);
    int (APS5_VABI *close_func)(void*);
    std::int64_t (APS5_VABI *tell_func)(void*);
};

struct GuestInfo {
    std::int32_t version;
    std::int32_t channels;
    std::int64_t rate;
    std::int64_t bitrate_upper;
    std::int64_t bitrate_nominal;
    std::int64_t bitrate_lower;
    std::int64_t bitrate_window;
    void* codec_setup;
};

struct GuestComment {
    char** user_comments;
    std::int32_t* comment_lengths;
    std::int32_t comments;
    char* vendor;
};

using GuestFilter = void (APS5_VABI *)(float**, std::int64_t, std::int64_t, void*);

static_assert(sizeof(GuestCallbacks) == 32);
static_assert(offsetof(GuestInfo, rate) == 8 && offsetof(GuestInfo, bitrate_window) == 40 && offsetof(GuestInfo, codec_setup) == 48);
static_assert(sizeof(GuestInfo) == 56);
static_assert(offsetof(GuestComment, comment_lengths) == 8 && offsetof(GuestComment, comments) == 16 && offsetof(GuestComment, vendor) == 24);
static_assert(sizeof(GuestComment) == 32);

}

#endif
