#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_OUTPUTDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_OUTPUTDEVICE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline std::optional<std::size_t> FindOutputDevice(const std::vector<std::string>& names, std::string_view wanted) {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == wanted) return index;
    }
    return std::nullopt;
}

#endif
