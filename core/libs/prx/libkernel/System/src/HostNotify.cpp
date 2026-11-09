#include "prx/libkernel/System/include/HostNotify.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <cerrno>
#include <cstdio>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

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

HostNotifyArgv LinuxNotifyArgv(std::string_view title, std::string_view text) {
    return {"notify-send", "--", std::string(title), std::string(text)};
}

HostNotifyArgv MacNotifyArgv(std::string_view title, std::string_view text) {
    return {"osascript", "-e", "on run argv", "-e", "display notification (item 2 of argv) with title (item 1 of argv)", "-e", "end run", std::string(title), std::string(text)};
}

std::string HostNotifyBase64(std::string_view bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t index = 0;
    for (; index + 3 <= bytes.size(); index += 3) {
        const auto b0 = static_cast<std::uint8_t>(bytes[index]);
        const auto b1 = static_cast<std::uint8_t>(bytes[index + 1]);
        const auto b2 = static_cast<std::uint8_t>(bytes[index + 2]);
        out += alphabet[b0 >> 2];
        out += alphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += alphabet[((b1 & 0x0f) << 2) | (b2 >> 6)];
        out += alphabet[b2 & 0x3f];
    }
    const auto rest = bytes.size() - index;
    if (rest == 1) {
        const auto b0 = static_cast<std::uint8_t>(bytes[index]);
        out += alphabet[b0 >> 2];
        out += alphabet[(b0 & 0x03) << 4];
        out += "==";
    } else if (rest == 2) {
        const auto b0 = static_cast<std::uint8_t>(bytes[index]);
        const auto b1 = static_cast<std::uint8_t>(bytes[index + 1]);
        out += alphabet[b0 >> 2];
        out += alphabet[((b0 & 0x03) << 4) | (b1 >> 4)];
        out += alphabet[(b1 & 0x0f) << 2];
        out += '=';
    }
    return out;
}

HostNotifyArgv WindowsNotifyArgv(std::string_view title, std::string_view text) {
    const std::string script =
        "Add-Type -AssemblyName System.Windows.Forms,System.Drawing;"
        "$t=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('" + HostNotifyBase64(title) + "'));"
        "$x=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('" + HostNotifyBase64(text) + "'));"
        "$n=New-Object System.Windows.Forms.NotifyIcon;"
        "$n.Icon=[System.Drawing.SystemIcons]::Information;"
        "$n.Visible=$true;"
        "$n.ShowBalloonTip(5000,$t,$x,[System.Windows.Forms.ToolTipIcon]::Info);"
        "Start-Sleep -Seconds 6;"
        "$n.Dispose()";
    std::string utf16;
    utf16.reserve(script.size() * 2);
    for (const char character : script) {
        utf16 += character;
        utf16 += '\0';
    }
    return {"powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", HostNotifyBase64(utf16)};
}

std::string HostNotifyBackendName(HostNotifyBackend backend) {
    switch (backend) {
    case HostNotifyBackend::NotifySend:
        return "notify-send";
    case HostNotifyBackend::Osascript:
        return "osascript";
    case HostNotifyBackend::PowerShell:
        return "PowerShell";
    case HostNotifyBackend::None:
        break;
    }
    return "no backend";
}

HostNotifyArgv HostNotifyArgvFor(HostNotifyBackend backend, std::string_view title, std::string_view text) {
    switch (backend) {
    case HostNotifyBackend::NotifySend:
        return LinuxNotifyArgv(title, text);
    case HostNotifyBackend::Osascript:
        return MacNotifyArgv(title, text);
    case HostNotifyBackend::PowerShell:
        return WindowsNotifyArgv(title, text);
    case HostNotifyBackend::None:
        break;
    }
    return {};
}

bool ExecutableInPath(std::string_view name, std::string_view pathList, const HostNotifyIsExecutable& isExecutable) {
    std::size_t start = 0;
    while (start <= pathList.size()) {
        auto end = pathList.find(':', start);
        if (end == std::string_view::npos) end = pathList.size();
        const auto directory = pathList.substr(start, end - start);
        if (!directory.empty() && isExecutable(std::string(directory) + "/" + std::string(name))) return true;
        start = end + 1;
    }
    return false;
}

HostNotifyBackend SelectHostNotifyBackend(std::string_view pathList, const HostNotifyIsExecutable& isExecutable) {
#if defined(__APPLE__)
    (void)pathList;
    (void)isExecutable;
    return HostNotifyBackend::Osascript;
#elif defined(_WIN32)
    (void)pathList;
    (void)isExecutable;
    return HostNotifyBackend::PowerShell;
#else
    return ExecutableInPath("notify-send", pathList, isExecutable) ? HostNotifyBackend::NotifySend : HostNotifyBackend::None;
#endif
}

DetachedHostNotifySink::DetachedHostNotifySink(HostNotifyBackend backendValue, HostNotifySpawn spawnValue)
    : backend(backendValue), spawn(std::move(spawnValue)) {}

namespace {

constexpr int MaxNoticesInFlight = 4;
std::atomic<int> noticesInFlight{0};

}

void DetachedHostNotifySink::Notify(std::string_view title, std::string_view text) {
    auto argv = HostNotifyArgvFor(backend, title, text);
    if (argv.empty()) return;
    const auto name = HostNotifyBackendName(backend);
    if (noticesInFlight.fetch_add(1) >= MaxNoticesInFlight) {
        noticesInFlight.fetch_sub(1);
        HostNotifyLog("the host notice was dropped: " + std::to_string(MaxNoticesInFlight) + " notices are still being shown with " + name);
        return;
    }
    try {
        std::thread([spawn = spawn, argv = std::move(argv), name]() {
            bool shown = false;
            try {
                shown = spawn(argv);
            } catch (...) {
                shown = false;
            }
            noticesInFlight.fetch_sub(1);
            if (!shown) HostNotifyLog("the host notice could not be shown with " + name);
        }).detach();
    } catch (const std::system_error&) {
        noticesInFlight.fetch_sub(1);
        HostNotifyLog("the host notice could not be started with " + name);
    }
}

std::unique_ptr<IHostNotifySink> MakeHostNotifySink(HostNotifyMode mode, HostNotifyBackend backend, HostNotifySpawn spawn) {
    if (mode == HostNotifyMode::Off || backend == HostNotifyBackend::None) return nullptr;
    return std::make_unique<DetachedHostNotifySink>(backend, std::move(spawn));
}

#ifndef _WIN32
bool SpawnHostNotifyCommand(const HostNotifyArgv& argv) {
    if (argv.empty()) return false;
    std::vector<char*> arguments;
    arguments.reserve(argv.size() + 1);
    for (const auto& argument : argv) arguments.push_back(const_cast<char*>(argument.c_str()));
    arguments.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return false;
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = 0;
    const int started = posix_spawnp(&pid, arguments[0], &actions, nullptr, arguments.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (started != 0) return false;

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

#ifdef _WIN32
bool SpawnHostNotifyCommand(const HostNotifyArgv& argv) {
    if (argv.empty()) return false;
    std::wstring commandLine;
    for (const auto& argument : argv) {
        if (argument.find('"') != std::string::npos) return false;
        if (!commandLine.empty()) commandLine += L' ';
        commandLine += L'"';
        for (const char character : argument) commandLine += static_cast<wchar_t>(static_cast<unsigned char>(character));
        commandLine += L'"';
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    const bool succeeded = GetExitCodeProcess(process.hProcess, &code) && code == 0;
    CloseHandle(process.hProcess);
    return succeeded;
}
#endif

namespace {

bool DefaultIsExecutable(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

}

void SendHostNotification(std::string_view title, std::string_view text) {
    static const std::unique_ptr<IHostNotifySink> sink = [] {
        const auto mode = HostNotifyModeFromEnvironment(std::getenv("ANYPS5_HOST_NOTIFY"));
        if (mode == HostNotifyMode::Off) return std::unique_ptr<IHostNotifySink>();
        const auto path = std::getenv("PATH");
        const auto backend = SelectHostNotifyBackend(path == nullptr ? std::string_view() : std::string_view(path), DefaultIsExecutable);
        if (backend == HostNotifyBackend::None) {
            HostNotifyLog("notify-send is not in PATH; host notifications stay off");
            return std::unique_ptr<IHostNotifySink>();
        }
        return MakeHostNotifySink(mode, backend, SpawnHostNotifyCommand);
    }();
    if (sink) sink->Notify(title, text);
}

HostNotifyMode HostNotifyModeFromEnvironment(const char* text) {
    if (const auto mode = ParseHostNotifyMode(text)) return *mode;
    HostNotifyLog("ANYPS5_HOST_NOTIFY must be 0 or 1, not '" + std::string(text) + "'; host notifications stay off");
    return HostNotifyMode::Off;
}
