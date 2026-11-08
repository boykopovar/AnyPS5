#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class HostNotifyMode : std::uint8_t { Off, On };

class IHostNotifySink {
public:
    virtual ~IHostNotifySink() = default;
    virtual void Notify(std::string_view title, std::string_view text) = 0;
};

using HostNotifyArgv = std::vector<std::string>;

std::optional<HostNotifyMode> ParseHostNotifyMode(const char* text);
HostNotifyMode HostNotifyModeFromEnvironment(const char* text);

HostNotifyArgv LinuxNotifyArgv(std::string_view title, std::string_view text);
HostNotifyArgv MacNotifyArgv(std::string_view title, std::string_view text);
std::string HostNotifyBase64(std::string_view bytes);

#endif
