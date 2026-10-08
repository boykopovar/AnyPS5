#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#endif

#include "prx/libSceSystemService/LoadExec.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32

namespace {

constexpr wchar_t PreviousProcessVariable[] = L"ANYPS5_LOAD_EXEC_PREVIOUS_PROCESS";
constexpr std::size_t CommandLineCapacity = 32767;

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("sceSystemServiceLoadExec: argument too long");
    const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size == 0) throw std::invalid_argument("sceSystemServiceLoadExec: argument is not UTF-8");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

void AppendArgument(std::wstring& commandLine, const std::wstring& argument) {
    if (!commandLine.empty()) commandLine += L' ';
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        commandLine += argument;
        return;
    }
    commandLine += L'"';
    std::size_t backslashes = 0;
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        commandLine.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        commandLine += character;
        backslashes = 0;
    }
    commandLine.append(backslashes * 2, L'\\');
    commandLine += L'"';
}

std::wstring ExecutablePath() {
    std::wstring path(CommandLineCapacity, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "sceSystemServiceLoadExec: executable path");
    if (size >= path.size()) throw std::length_error("sceSystemServiceLoadExec: executable path too long");
    path.resize(size);
    return path;
}

class PreviousProcessWait final {
public:
    PreviousProcessWait() {
        wchar_t value[32]{};
        const auto length = GetEnvironmentVariableW(PreviousProcessVariable, value, static_cast<DWORD>(std::size(value)));
        if (length == 0) return;
        if (_wputenv_s(PreviousProcessVariable, L"") != 0)
            throw std::runtime_error("sceSystemServiceLoadExec: cannot clear the previous process variable");
        if (length >= std::size(value)) throw std::runtime_error("sceSystemServiceLoadExec: invalid previous process handle");
        wchar_t* end = nullptr;
        const auto handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(std::wcstoull(value, &end, 10)));
        if (end == value || *end != L'\0') throw std::runtime_error("sceSystemServiceLoadExec: invalid previous process handle");
        const auto waited = WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
        if (waited != WAIT_OBJECT_0) throw std::runtime_error("sceSystemServiceLoadExec: waiting for the previous process failed");
    }
};

const PreviousProcessWait previousProcessWait;

}

[[noreturn]] void SystemService::RestartProcess(const std::vector<std::string>& arguments) {
    const auto executable = ExecutablePath();
    std::wstring commandLine;
    AppendArgument(commandLine, executable);
    for (const auto& argument : arguments) AppendArgument(commandLine, Widen(argument));
    if (commandLine.size() >= CommandLineCapacity) throw std::length_error("sceSystemServiceLoadExec: command line too long");

    HANDLE self = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &self, SYNCHRONIZE, TRUE, 0))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "sceSystemServiceLoadExec: process handle");
    const auto value = std::to_wstring(reinterpret_cast<std::uintptr_t>(self));
    if (!SetEnvironmentVariableW(PreviousProcessVariable, value.c_str())) {
        const auto error = GetLastError();
        CloseHandle(self);
        throw std::system_error(static_cast<int>(error), std::system_category(), "sceSystemServiceLoadExec: previous process variable");
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const auto created = CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process);
    const auto error = GetLastError();
    SetEnvironmentVariableW(PreviousProcessVariable, nullptr);
    if (!created) {
        CloseHandle(self);
        throw std::system_error(static_cast<int>(error), std::system_category(), "sceSystemServiceLoadExec: starting the title");
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), 0);
    std::_Exit(0);
}

#else

extern "C" const char** APS5_VABI getargv_nid_postfix(void);

namespace {

void ResetSignals() {
    sigset_t none;
    sigemptyset(&none);
    if (pthread_sigmask(SIG_SETMASK, &none, nullptr) != 0)
        throw std::runtime_error("sceSystemServiceLoadExec: cannot unblock signals");
    for (int number = 1; number < NSIG; ++number) {
        struct sigaction action {};
        if (sigaction(number, nullptr, &action) != 0 || action.sa_handler != SIG_IGN) continue;
        action.sa_handler = SIG_DFL;
        sigaction(number, &action, nullptr);
    }
}

void CloseDescriptorsOnExec() {
    std::vector<int> descriptors;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
        const auto descriptor = std::stoi(entry.path().filename().string());
        if (descriptor > STDERR_FILENO) descriptors.push_back(descriptor);
    }
    for (const auto descriptor : descriptors) {
        const auto flags = fcntl(descriptor, F_GETFD);
        if (flags >= 0) fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC);
    }
}

}

[[noreturn]] void SystemService::RestartProcess(const std::vector<std::string>& arguments) {
    const auto current = getargv_nid_postfix();
    if (current == nullptr || current[0] == nullptr) throw std::runtime_error("sceSystemServiceLoadExec: no program name");
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(current[0]));
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);

    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    CloseDescriptorsOnExec();
    ResetSignals();
    execv("/proc/self/exe", argv.data());
    throw std::system_error(errno, std::generic_category(), "sceSystemServiceLoadExec: starting the title");
}

#endif
