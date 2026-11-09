#ifndef CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUMESETTINGS_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUMESETTINGS_HPP

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

#include "prx/libScePad/include/HostVolume.hpp"

inline bool ParseHostVolumeFlag(std::string_view name, std::string_view value) {
    if (value == "0") return false;
    if (value == "1") return true;
    throw std::runtime_error(std::string(name) + " must be 0 or 1, not '" + std::string(value) + "'");
}

inline int ParseHostVolumeLevel(std::string_view name, std::string_view value) {
    int parsed = 0;
    const auto end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != end || parsed < 0 || parsed > HostVolume::MaxVolume) {
        throw std::runtime_error(std::string(name) + " must be a whole number from 0 to " + std::to_string(HostVolume::MaxVolume) + ", not '" + std::string(value) + "'");
    }
    return parsed;
}

inline std::string HostVolumeMessage(const HostVolume& state) {
    const std::string level = std::to_string(state.Volume()) + "%";
    if (state.Muted()) return "Host volume: muted (volume " + level + ")";
    if (state.FocusMuted()) return "Host volume: muted while the window has no focus (volume " + level + ")";
    return "Host volume: " + level;
}

#endif
