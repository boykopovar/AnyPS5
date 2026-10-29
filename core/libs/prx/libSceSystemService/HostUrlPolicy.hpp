#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

enum class OpenUrlMode {
    Never,
    Ask,
    Always,
};

inline OpenUrlMode ParseOpenUrlMode(const char* configured, std::string_view caller) {
    if (configured == nullptr || configured[0] == '\0') return OpenUrlMode::Never;
    const std::string_view text(configured);
    if (text == "never") return OpenUrlMode::Never;
    if (text == "ask") return OpenUrlMode::Ask;
    if (text == "always") return OpenUrlMode::Always;
    throw std::runtime_error(std::string(caller) + ": ANYPS5_OPEN_URLS must be never, ask or always, not \"" + std::string(text) + "\"");
}

inline OpenUrlMode OpenUrlModeFromEnvironment(std::string_view caller) {
    return ParseOpenUrlMode(std::getenv("ANYPS5_OPEN_URLS"), caller);
}
