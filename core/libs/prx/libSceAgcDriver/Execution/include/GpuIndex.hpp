#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUINDEX_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUINDEX_HPP

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver {

inline std::optional<std::size_t> ParseGpuIndex(const char* value) {
    if (value == nullptr || *value == '\0') return std::nullopt;
    const std::string_view text(value);
    if (text.size() > 4) throw std::runtime_error("ANYPS5_GPU_INDEX=\"" + std::string(text) + "\" is not a Vulkan device index");
    std::size_t index = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') throw std::runtime_error("ANYPS5_GPU_INDEX=\"" + std::string(text) + "\" is not a Vulkan device index");
        index = index * 10 + static_cast<std::size_t>(digit - '0');
    }
    return index;
}

inline std::optional<std::size_t> RequestedGpuIndex() {
    return ParseGpuIndex(std::getenv("ANYPS5_GPU_INDEX"));
}

}

#endif
