#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTNOTIFY_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class HostNotifyMode : std::uint8_t { Off, On };

enum class HostNotifyBackend : std::uint8_t { None, NotifySend, Osascript, PowerShell };

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
HostNotifyArgv WindowsNotifyArgv(std::string_view title, std::string_view text);
HostNotifyArgv HostNotifyArgvFor(HostNotifyBackend backend, std::string_view title, std::string_view text);

using HostNotifyIsExecutable = std::function<bool(const std::string&)>;

bool ExecutableInPath(std::string_view name, std::string_view pathList, const HostNotifyIsExecutable& isExecutable);
HostNotifyBackend SelectHostNotifyBackend(std::string_view pathList, const HostNotifyIsExecutable& isExecutable);

using HostNotifySpawn = std::function<bool(const HostNotifyArgv&)>;

bool SpawnHostNotifyCommand(const HostNotifyArgv& argv);

class DetachedHostNotifySink final : public IHostNotifySink {
public:
    DetachedHostNotifySink(HostNotifyBackend backend, HostNotifySpawn spawn);
    void Notify(std::string_view title, std::string_view text) override;

private:
    HostNotifyBackend backend;
    HostNotifySpawn spawn;
};

std::unique_ptr<IHostNotifySink> MakeHostNotifySink(HostNotifyMode mode, HostNotifyBackend backend, HostNotifySpawn spawn);

void SendHostNotification(std::string_view title, std::string_view text);

#endif
