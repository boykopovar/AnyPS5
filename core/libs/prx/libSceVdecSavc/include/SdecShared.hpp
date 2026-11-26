#pragma once

#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <span>

namespace {

struct LevelLimits {
    std::uint32_t level;
    std::uint32_t maxDpbMbs;
    std::uint32_t maxFrameMbs;
};

struct SdecSizes {
    std::uint64_t first;
    std::uint64_t second;
    std::uint64_t third;
};

inline const LevelLimits* FindLevel(std::span<const LevelLimits> levels, std::uint32_t level) {
    const auto found = std::find_if(levels.begin(), levels.end(), [level](const LevelLimits& limits) { return limits.level == level; });
    return found == levels.end() ? nullptr : &*found;
}

inline std::uint64_t Up(std::uint64_t value, std::uint64_t multiple) {
    return (value + multiple - 1) / multiple * multiple;
}

inline std::uint64_t Surface(std::uint64_t elementSize, std::uint64_t width, std::uint64_t height) {
    const std::uint64_t granule = std::max<std::uint64_t>(256, 64 * elementSize);
    std::uint64_t pitchAlignment = 64;
    while (pitchAlignment * height % granule != 0) pitchAlignment *= 2;
    return Up(width * elementSize, pitchAlignment) * height;
}

inline bool DestroyedWithoutCodec(void* decoder) {
    const int savedErrno = errno;
    std::uint64_t codec = 0;
    std::uint64_t field = 0;
    bool done = GuestCopyIn_nid_no_patch(&codec, static_cast<unsigned char*>(decoder) + 0x10, sizeof(codec)) &&
                codec == 0 &&
                GuestCopyIn_nid_no_patch(&field, static_cast<unsigned char*>(decoder) + 0x8, sizeof(field));
    if (done && field != 0) {
        const std::uint64_t zero = 0;
        done = GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(decoder) + 0x8, &zero, sizeof(zero));
    }
    errno = savedErrno;
    return done;
}

}
