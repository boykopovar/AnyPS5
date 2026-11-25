#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP

#include <cmath>
#include <stdexcept>
#include <string_view>
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 260000
#include <cerrno>
#include <cfenv>
#include <xlocale.h>
#else
#include <charconv>
#endif

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
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 260000
        const int savedErrno = errno;
        const locale_t locale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
        if (locale == nullptr) throw std::runtime_error("Vulkan presentation: could not create the numeric locale");
        std::fenv_t environment;
        std::feholdexcept(&environment);
        std::fesetround(FE_TONEAREST);
        errno = 0;
        char* end = nullptr;
        value = strtof_l(sharpness, &end, locale);
        const bool underflowed = value == 0.0f && errno == ERANGE;
        std::fesetenv(&environment);
        freelocale(locale);
        errno = savedErrno;
        const bool decimal = text.front() != '+' && text.find_first_not_of("0123456789eE+-.") == std::string_view::npos;
        const bool parsed = decimal && end == text.data() + text.size() && !underflowed;
#else
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        const bool parsed = error == std::errc{} && end == text.data() + text.size();
#endif
        if (!parsed || !std::isfinite(value) || value < 0.0f || value > 2.0f) throw std::runtime_error("Vulkan presentation: ANYPS5_FSR_SHARPNESS must be a number from 0.0 to 2.0");
        mode.sharpness = value;
    }
    return mode;
}

}

#endif
