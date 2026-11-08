#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

enum class HostNotifyMode : std::uint8_t { Off, On };

class IHostNotifySink {
public:
    virtual ~IHostNotifySink() = default;
    virtual void Notify(std::string_view title, std::string_view text) = 0;
};

std::optional<HostNotifyMode> ParseHostNotifyMode(const char* text);
HostNotifyMode HostNotifyModeFromEnvironment(const char* text);

#endif
