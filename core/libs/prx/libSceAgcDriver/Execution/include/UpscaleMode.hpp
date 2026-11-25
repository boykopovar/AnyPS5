#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

struct UpscaleMode {
    bool fsr = false;
    bool dlss = false;
    bool linear = false;
    float sharpness = 0.2f;
    int dlssQuality = 2;
};

inline int ParseDlssQuality(const char* value) {
    if (value == nullptr || *value == '\0') return 2;
    const std::string_view text(value);
    if (text == "quality" || text == "max_quality") return 2;
    if (text == "balanced") return 1;
    if (text == "performance" || text == "perf") return 0;
    if (text == "ultra_performance" || text == "ultra_perf") return 3;
    if (text == "dlaa" || text == "ultra_quality") return 4;
    throw std::runtime_error("Vulkan presentation: ANYPS5_DLSS_QUALITY must be quality, balanced, performance, ultra_performance, or dlaa");
}

inline UpscaleMode SelectUpscaleMode(const char* upscale, const char* sharpness, const char* dlssQuality = nullptr, const char* dlssSharpness = nullptr) {
    UpscaleMode mode;
    if (upscale != nullptr && *upscale != '\0') {
        const std::string_view text(upscale);
        if (text == "fsr") {
            mode.fsr = true;
        } else if (text == "dlss") {
            mode.dlss = true;
        } else if (text == "linear") {
            mode.linear = true;
        } else {
            throw std::runtime_error("Vulkan presentation: ANYPS5_UPSCALE must be fsr, dlss, linear, or unset");
        }
    }
    if (sharpness != nullptr && *sharpness != '\0') {
        const std::string_view text(sharpness);
        float value = 0.0f;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) || value < 0.0f || value > 2.0f) throw std::runtime_error("Vulkan presentation: ANYPS5_FSR_SHARPNESS must be a number from 0.0 to 2.0");
        mode.sharpness = value;
    }
    const char* qualitySetting = dlssQuality != nullptr ? dlssQuality : std::getenv("ANYPS5_DLSS_QUALITY");
    if (qualitySetting != nullptr && *qualitySetting != '\0') {
        mode.dlssQuality = ParseDlssQuality(qualitySetting);
    }
    const char* dlssSharpnessSetting = dlssSharpness != nullptr ? dlssSharpness : std::getenv("ANYPS5_DLSS_SHARPNESS");
    if (dlssSharpnessSetting != nullptr && *dlssSharpnessSetting != '\0') {
        const std::string_view text(dlssSharpnessSetting);
        float value = 0.0f;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) || value < 0.0f || value > 1.0f) throw std::runtime_error("Vulkan presentation: ANYPS5_DLSS_SHARPNESS must be a number from 0.0 to 1.0");
        mode.sharpness = value;
    }
    return mode;
}

}

#endif
