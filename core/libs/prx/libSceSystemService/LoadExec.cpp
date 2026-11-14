#include "LoadExec.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
extern char** environ;
#elif defined(__APPLE__)
#include <crt_externs.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <spawn.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

std::wstring Wide(const std::string& text) {
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("LoadExec: argument is too long");
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw std::runtime_error("LoadExec: argument is not valid UTF-8");
    std::wstring wide(size, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size) != size)
        throw std::runtime_error("LoadExec: cannot convert argument to UTF-16");
    return wide;
}

std::wstring Quote(const std::wstring& text) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t character : text) {
        if (character == L'\\') { ++slashes; continue; }
        result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += character;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

Handle Duplicate(HANDLE source, DWORD access) {
    HANDLE handle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &handle, access, TRUE,
                         access ? 0 : DUPLICATE_SAME_ACCESS))
        throw std::system_error(GetLastError(), std::system_category(), "LoadExec: duplicating a process handle");
    return Handle(handle, CloseHandle);
}

[[noreturn]] void Replace(const std::filesystem::path& target, const std::vector<std::string>& arguments) {
    std::wstring command = Quote(target.wstring());
    for (const auto& argument : arguments) {
        if (!command.empty()) command += L' ';
        command += Quote(Wide(argument));
    }
    if (command.size() >= 32767) throw std::runtime_error("LoadExec: command line is too long");
    auto parent = Duplicate(GetCurrentProcess(), SYNCHRONIZE);
    LogFileRelease_nid_no_patch();
    std::vector<Handle> standard;
    std::vector<HANDLE> handles{parent.get()};
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    HANDLE* outputs[] = {&startup.StartupInfo.hStdInput, &startup.StartupInfo.hStdOutput, &startup.StartupInfo.hStdError};
    const DWORD identifiers[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (std::size_t index = 0; index < 3; ++index) {
        const HANDLE source = GetStdHandle(identifiers[index]);
        if (source && source != INVALID_HANDLE_VALUE) {
            standard.push_back(Duplicate(source, 0));
            *outputs[index] = standard.back().get();
            handles.push_back(*outputs[index]);
        }
    }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
        throw std::system_error(GetLastError(), std::system_category(), "LoadExec: creating process attributes");
    const std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, decltype(&DeleteProcThreadAttributeList)> cleanup(attributes, DeleteProcThreadAttributeList);
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(), handles.size() * sizeof(HANDLE), nullptr, nullptr))
        throw std::system_error(GetLastError(), std::system_category(), "LoadExec: selecting inherited handles");
    startup.lpAttributeList = attributes;
    const std::unique_ptr<wchar_t, decltype(&FreeEnvironmentStringsW)> inherited(GetEnvironmentStringsW(), FreeEnvironmentStringsW);
    if (!inherited) throw std::runtime_error("LoadExec: cannot read process environment");
    constexpr wchar_t Variable[] = L"ANYPS5_LOADEXEC_PARENT=";
    constexpr wchar_t Skip[] = L"ANYPS5_LOADEXEC_ARGV_SKIP=";
    std::vector<std::wstring> entries;
    for (const wchar_t* at = inherited.get(); *at;) {
        const std::wstring entry(at);
        if (_wcsnicmp(entry.c_str(), Variable, std::size(Variable) - 1) != 0 &&
            _wcsnicmp(entry.c_str(), Skip, std::size(Skip) - 1) != 0) {
            entries.push_back(entry);
        }
        at += entry.size() + 1;
    }
    std::wostringstream value;
    value << Variable << std::hex << reinterpret_cast<std::uintptr_t>(parent.get());
    entries.push_back(value.str());
    entries.push_back(std::wstring(Skip) + L"1");
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
    std::vector<wchar_t> environment;
    for (const auto& entry : entries) {
        environment.insert(environment.end(), entry.begin(), entry.end());
        environment.push_back(0);
    }
    environment.push_back(0);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(target.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                        environment.data(), target.parent_path().c_str(), &startup.StartupInfo, &process))
        throw std::system_error(GetLastError(), std::system_category(), "LoadExec: creating the converted executable");
    Handle child(process.hProcess, CloseHandle), thread(process.hThread, CloseHandle);
    try {
        LibcStartExit_nid_postfix();
        LibcRunShutdown_nid_postfix();
        std::fflush(nullptr);
        if (ResumeThread(thread.get()) == static_cast<DWORD>(-1))
            throw std::system_error(GetLastError(), std::system_category(), "LoadExec: starting the converted executable");
    } catch (...) {
        TerminateProcess(child.get(), 1);
        throw;
    }
    thread.reset();
    child.reset();
    parent.reset();
    LibcTerminate_nid_no_patch(0);
}
#elif defined(__linux__)
[[noreturn]] void Replace(const std::filesystem::path& target, const std::vector<std::string>& arguments) {
    const int descriptor = open(target.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "LoadExec: opening the converted executable");
    const auto close = [](int* fd) { ::close(*fd); delete fd; };
    const std::unique_ptr<int, decltype(close)> file(new int(descriptor), close);
    std::array<unsigned char, 20> header{};
    if (pread(descriptor, header.data(), header.size(), 0) != static_cast<ssize_t>(header.size()) ||
        header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F' ||
        header[4] != 2 || header[5] != 1 || (header[7] != 0 && header[7] != 3) || header[17] != 0 || (header[16] != 2 && header[16] != 3) || header[18] != 62 || header[19] != 0 || access(target.c_str(), X_OK) != 0)
        throw std::runtime_error("LoadExec: target is not an executable x86-64 Linux ELF");
    std::vector<char*> pointers;
    for (const auto& argument : arguments) pointers.push_back(const_cast<char*>(argument.c_str()));
    pointers.push_back(nullptr);
    LibcStartExit_nid_postfix();
    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    if (const int host = HostStandardError_nid_no_patch(); host >= 0 && dup2(host, 2) != 2)
        throw std::system_error(errno, std::generic_category(), "LoadExec: giving the host standard error back to descriptor 2");
    LogFileRelease_nid_no_patch();
    std::filesystem::current_path(target.parent_path());
    if ((descriptor > 3 && close_range(3, static_cast<unsigned>(descriptor - 1), 0) != 0) ||
        close_range(static_cast<unsigned>(std::max(3, descriptor + 1)), ~0u, 0) != 0)
        throw std::system_error(errno, std::generic_category(), "LoadExec: closing the old executable's descriptors");
    fexecve(descriptor, pointers.data(), environ);
    throw std::system_error(errno, std::generic_category(), "LoadExec: replacing the executable");
}
#elif defined(__APPLE__)
[[noreturn]] void Replace(const std::filesystem::path& target, const std::vector<std::string>& arguments) {
    const int descriptor = open(target.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "LoadExec: opening the converted executable");
    mach_header_64 header{};
    const auto read = pread(descriptor, &header, sizeof(header), 0);
    ::close(descriptor);
    if (read != static_cast<ssize_t>(sizeof(header)) || header.magic != MH_MAGIC_64 || header.cputype != CPU_TYPE_X86_64 || header.filetype != MH_EXECUTE || access(target.c_str(), X_OK) != 0)
        throw std::runtime_error("LoadExec: target is not an executable x86-64 Mach-O");
    std::vector<char*> pointers;
    for (const auto& argument : arguments) pointers.push_back(const_cast<char*>(argument.c_str()));
    pointers.push_back(nullptr);
    posix_spawnattr_t attributes;
    posix_spawn_file_actions_t actions;
    if (posix_spawnattr_init(&attributes) != 0 || posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETEXEC | POSIX_SPAWN_CLOEXEC_DEFAULT) != 0)
        throw std::runtime_error("LoadExec: preparing the replacement");
    LibcStartExit_nid_postfix();
    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    if (const int host = HostStandardError_nid_no_patch(); host >= 0 && dup2(host, 2) != 2)
        throw std::system_error(errno, std::generic_category(), "LoadExec: giving the host standard error back to descriptor 2");
    LogFileRelease_nid_no_patch();
    for (const int standard : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
        if (fcntl(standard, F_GETFD) != -1 && posix_spawn_file_actions_addinherit_np(&actions, standard) != 0)
            throw std::runtime_error("LoadExec: keeping the standard streams");
    }
    std::filesystem::current_path(target.parent_path());
    const int error = posix_spawn(nullptr, target.c_str(), &actions, &attributes, pointers.data(), *_NSGetEnviron());
    throw std::system_error(error, std::generic_category(), "LoadExec: replacing the executable");
}
#endif

}

extern "C" bool LoadConvertedExecutable_nid_no_patch(const std::filesystem::path& target, const char* const* arguments) {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    if (!arguments || !arguments[0]) return false;
    std::vector<std::string> copied;
    for (std::size_t index = 0; arguments[index]; ++index) copied.emplace_back(arguments[index]);
    Replace(target, copied);
#else
    (void)target;
    (void)arguments;
    return false;
#endif
}
