#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

struct UpscaleMode {
    bool fsr = false;
    float sharpness = 0.2f;
};

inline UpscaleMode SelectUpscaleMode(const char* upscale, const char* sharpness) {
    UpscaleMode mode;
    if (upscale != nullptr && *upscale != '\0') {
        if (std::string_view(upscale) != "fsr") throw std::runtime_error("Vulkan presentation: ANYPS5_UPSCALE must be fsr or unset");
        mode.fsr = true;
    }
    if (sharpness != nullptr && *sharpness != '\0') {
        const std::string_view text(sharpness);
        float value = 0.0f;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) || value < 0.0f || value > 2.0f) throw std::runtime_error("Vulkan presentation: ANYPS5_FSR_SHARPNESS must be a number from 0.0 to 2.0");
        mode.sharpness = value;
    }
    return mode;
}

}

#endif
