#include "prx/libkernel/System/include/HostNotify.hpp"

#include <cstdio>
#include <string>

namespace {

void HostNotifyLog(const std::string& line) {
    std::fprintf(stderr, "[host-notify] %s\n", line.c_str());
}

}

std::optional<HostNotifyMode> ParseHostNotifyMode(const char* text) {
    if (text == nullptr || *text == '\0' || std::string_view(text) == "0") return HostNotifyMode::Off;
    if (std::string_view(text) == "1") return HostNotifyMode::On;
    return std::nullopt;
}

HostNotifyMode HostNotifyModeFromEnvironment(const char* text) {
    if (const auto mode = ParseHostNotifyMode(text)) return *mode;
    HostNotifyLog("ANYPS5_HOST_NOTIFY must be 0 or 1, not '" + std::string(text) + "'; host notifications stay off");
    return HostNotifyMode::Off;
}
