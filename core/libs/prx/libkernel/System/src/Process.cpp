#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#else
#include <sched.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/uio.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__linux__) && defined(__x86_64__)
#include <asm/prctl.h>
#include <sys/syscall.h>
#endif

#ifdef _WIN32
#include "prx/libc/include/ChildProcesses.hpp"
#else
#include <sys/wait.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifndef _WIN32
#include <sys/resource.h>
#endif

extern char** environ;
namespace {

constexpr int sceInvalidArgument = static_cast<int>(0x80020016u);
constexpr int errnoOperationNotPermitted = 1;
constexpr int errnoNoSuchProcess = 3;
constexpr int errnoNoChild = 10;
constexpr int errnoBadAddress = 14;
constexpr int errnoInvalidArgument = 22;
constexpr int errnoTooBig = 7;
constexpr int errnoNoExec = 8;
constexpr int errnoNoEnt = 2;
constexpr int errnoAccess = 13;
constexpr int errnoBadFile = 9;
constexpr int freebsdGroupsMax = 1023;
std::mutex supplementaryGroupsMutex;
std::vector<std::uint32_t> supplementaryGroups{0};
constexpr unsigned freebsdWaitOptions = 0x8000003Fu;
constexpr std::size_t ExecArgumentBytesLimit = 262144;
constexpr std::string_view ExecDefaultSearchPath = "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin";
constexpr std::string_view ExecApplicationRoot = "/app0/";
constexpr std::string_view ExecMainExecutable = "/app0/eboot.bin";

bool StoreGuestId(std::uint32_t* output) {
    if (output == nullptr) return true;
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    if (address > std::numeric_limits<std::uintptr_t>::max() - sizeof(*output)) return false;
    const auto end = address + sizeof(*output);
    for (auto cursor = address; cursor < end;) {
        VirtualQueryInfo info{};
        if (sceKernelVirtualQuery(reinterpret_cast<const void*>(cursor), 0, &info, sizeof(info)) != 0 ||
            info.end <= cursor || (info.protection & 2) == 0)
            return false;
        cursor = std::min(end, info.end);
    }
    const GuestArena::HostWrite guard(output, sizeof(*output));
    if (!guard.Open()) return false;
    constexpr std::uint32_t zero = 0;
#ifdef _WIN32
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), output, &zero, sizeof(zero), &written) != 0 && written == sizeof(zero);
#elif defined(__linux__)
    iovec local{const_cast<std::uint32_t*>(&zero), sizeof(zero)};
    iovec remote{output, sizeof(zero)};
    return ::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(sizeof(zero));
#elif defined(__APPLE__)
    return ::mach_vm_write(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(output),
        reinterpret_cast<vm_offset_t>(const_cast<std::uint32_t*>(&zero)), sizeof(zero)) == KERN_SUCCESS;
#else
    *output = zero;
    return true;
#endif
}

bool GuestRange(const void* pointer, std::size_t bytes) {
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    return start != 0 && bytes <= std::numeric_limits<std::uintptr_t>::max() - start;
}

bool ReadGuest(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    if (!GuestRange(source, bytes)) return false;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
#ifdef _WIN32
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), source, destination, bytes, &copied) != 0 && copied == bytes;
#elif defined(__linux__)
    iovec local{destination, bytes};
    iovec remote{const_cast<void*>(source), bytes};
    const auto copied = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("credentials: process_vm_readv failed with errno " + std::to_string(errno));
    return copied >= 0 && static_cast<std::size_t>(copied) == bytes;
#elif defined(__APPLE__)
    mach_vm_size_t copied = 0;
    return ::mach_vm_read_overwrite(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(source), bytes,
        reinterpret_cast<mach_vm_address_t>(destination), &copied) == KERN_SUCCESS && copied == bytes;
#else
    throw std::runtime_error("credentials: checked guest reads are not implemented on this host");
#endif
}

bool WriteGuest(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    if (!GuestRange(destination, bytes)) return false;
    const GuestArena::HostWrite guard(destination, bytes);
    if (!guard.Open()) return false;
#ifdef _WIN32
    SIZE_T copied = 0;
    return WriteProcessMemory(GetCurrentProcess(), destination, source, bytes, &copied) != 0 && copied == bytes;
#elif defined(__linux__)
    iovec local{const_cast<void*>(source), bytes};
    iovec remote{destination, bytes};
    const auto copied = ::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("credentials: process_vm_writev failed with errno " + std::to_string(errno));
    return copied >= 0 && static_cast<std::size_t>(copied) == bytes;
#elif defined(__APPLE__)
    return ::mach_vm_write(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(destination),
        reinterpret_cast<vm_offset_t>(source), static_cast<mach_msg_type_number_t>(bytes)) == KERN_SUCCESS;
#else
    throw std::runtime_error("credentials: checked guest writes are not implemented on this host");
#endif
}

std::atomic<std::uint32_t> gpoBits{0};
constexpr std::array<std::uint8_t, 16> openPsId{'A', 'n', 'y', 'P', 'S', '5', 'O', 'p', 'e', 'n', 'P', 's', 'I', 'd', 0, 1};

#ifdef _WIN32
std::string toUtf8(const wchar_t* value) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        throw std::system_error(GetLastError(), std::system_category(), "Converting a process argument to UTF-8");
    std::string text(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, text.data(), size, nullptr, nullptr) != size)
        throw std::system_error(GetLastError(), std::system_category(), "Converting a process argument to UTF-8");
    text.pop_back();
    return text;
}
#endif

class ProcessArguments {
public:
    ProcessArguments() {
#ifdef _WIN32
        int count = 0;
        const std::unique_ptr<LPWSTR, void (*)(LPWSTR*)> values(CommandLineToArgvW(GetCommandLineW(), &count),
            [](LPWSTR* parsed) { LocalFree(parsed); });
        if (!values)
            throw std::system_error(GetLastError(), std::system_category(), "Reading process arguments");
        for (int index = 0; index < count; ++index)
            arguments.push_back(toUtf8(values.get()[index]));
        wchar_t skip[2]{};
        if (GetEnvironmentVariableW(L"ANYPS5_LOADEXEC_ARGV_SKIP", skip, 2) == 1 && skip[0] == L'1') {
            if (arguments.size() < 2) throw std::runtime_error("LoadExec: missing guest process arguments");
            arguments.erase(arguments.begin());
            if (_wputenv_s(L"ANYPS5_LOADEXEC_ARGV_SKIP", L"") != 0 || !SetEnvironmentVariableW(L"ANYPS5_LOADEXEC_ARGV_SKIP", nullptr))
                throw std::runtime_error("LoadExec: cannot clear the argument marker");
        }
#else
        std::ifstream stream("/proc/self/cmdline", std::ios::binary);
        if (!stream)
            throw std::runtime_error("Cannot read process arguments");
        std::string argument;
        while (std::getline(stream, argument, '\0')) {
            if (stream.eof())
                throw std::runtime_error("Unterminated process argument");
            arguments.push_back(argument);
        }
        if (stream.bad() || !stream.eof())
            throw std::runtime_error("Reading process arguments failed");
#endif
        if (arguments.empty() || arguments.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Invalid process argument count");
        for (const auto& argument : arguments)
            pointers.push_back(argument.c_str());
        pointers.push_back(nullptr);
    }

    int GetCount() const { return static_cast<int>(arguments.size()); }
    const char** GetValues() { return pointers.data(); }

private:
    std::vector<std::string> arguments;
    std::vector<const char*> pointers;
};

ProcessArguments& getProcessArguments() {
    static ProcessArguments arguments;
    return arguments;
}

bool hasPriorityRange(int policy) {
    return policy == 1 || policy == 3;
}

constexpr int guestNoProcess = 3;
constexpr int guestFault = 14;
constexpr int guestInvalid = 22;
constexpr int guestRlimitCpu = 0;
constexpr int guestRlimitFsize = 1;
constexpr int guestPriorityProcess = 0;
constexpr int guestPriorityGroup = 1;
constexpr int guestPriorityUser = 2;
constexpr int guestRlimitData = 2;
constexpr int guestRlimitStack = 3;
constexpr int guestRlimitCore = 4;
constexpr int guestRlimitRss = 5;
constexpr int guestRlimitMemlock = 6;
constexpr int guestRlimitNproc = 7;
constexpr int guestRlimitNofile = 8;
constexpr int guestRlimitSbsize = 9;
constexpr int guestRlimitVmem = 10;
constexpr int guestRlimitNpts = 11;
constexpr int guestRlimitSwap = 12;
constexpr int guestRlimitKqueues = 13;
constexpr int guestRlimitUmtxp = 14;
constexpr int guestRlimitCount = 15;
constexpr std::int64_t guestRlimitInfinity = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t guestStackLimit = 0x200000;
constexpr std::int64_t guestProcessLimit = 1604;
constexpr std::int64_t guestProcessLimitPerUser = 1443;

#ifdef _WIN32
std::int64_t hostMemoryLimit() {
    BOOL inJob = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob))
        throw std::system_error(GetLastError(), std::system_category(), "getrlimit: IsProcessInJob failed");
    if (!inJob)
        return guestRlimitInfinity;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if (!QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &limits, sizeof(limits), nullptr))
        throw std::system_error(GetLastError(), std::system_category(), "getrlimit: QueryInformationJobObject failed");
    if ((limits.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY) == 0 || limits.ProcessMemoryLimit > static_cast<SIZE_T>(guestRlimitInfinity))
        return guestRlimitInfinity;
    return static_cast<std::int64_t>(limits.ProcessMemoryLimit);
}
#else
std::int64_t toGuestLimit(rlim_t value) {
    if (value == RLIM_INFINITY || value > static_cast<rlim_t>(guestRlimitInfinity))
        return guestRlimitInfinity;
    return static_cast<std::int64_t>(value);
}
#endif

}

namespace {
std::mutex openFileLimitMutex;
std::optional<GuestResourceLimit> guestOpenFileLimit;
std::mutex processLimitMutex;
GuestResourceLimit guestProcessResourceLimit{guestProcessLimit, guestProcessLimit};
std::mutex coreLimitMutex;
GuestResourceLimit guestCoreLimit{guestRlimitInfinity, guestRlimitInfinity};
}

namespace {

bool ExecProgramFound(std::string_view name, bool search, int& error, std::string& found) {
    const auto executable = [&error, &found](const std::string& candidate) {
        found = candidate;
        std::filesystem::path native;
        std::error_code probe;
        const int resolved = ResolvePathError_nid_no_patch(candidate.c_str(), native);
        if (resolved != 0 || !std::filesystem::exists(native, probe)) {
            error = resolved == 0 ? NotFoundError_nid_no_patch(native) : resolved;
            return false;
        }
        if (!std::filesystem::is_regular_file(native, probe) || !ExecutePermission_nid_no_patch(native)) {
            error = errnoAccess;
            return false;
        }
        return true;
    };
    if (name.empty()) {
        error = errnoNoEnt;
        return false;
    }
    if (!search || name.find('/') != std::string_view::npos) return executable(std::string(name));
    bool inaccessible = false;
    const char* variable = getenv_nid_postfix("PATH");
    std::string_view directories = variable != nullptr ? std::string_view(variable) : ExecDefaultSearchPath;
    while (true) {
        const auto separator = directories.find(':');
        const auto directory = directories.substr(0, separator);
        const std::string candidate = (directory.empty() ? std::string(".") : std::string(directory)) + '/' + std::string(name);
        found = candidate;
        std::filesystem::path native;
        std::error_code probe;
        if (ResolvePathError_nid_no_patch(candidate.c_str(), native) == 0 && std::filesystem::exists(native, probe)) {
            if (std::filesystem::is_regular_file(native, probe) && ExecutePermission_nid_no_patch(native)) return true;
            inaccessible = true;
        }
        if (separator == std::string_view::npos) {
            error = inaccessible ? errnoAccess : errnoNoEnt;
            return false;
        }
        directories.remove_prefix(separator + 1);
    }
}

std::string ExecFlattenedName(const std::string& guest) {
    std::string_view relative(guest);
    if (!relative.starts_with(ExecApplicationRoot))
        NotImplemented_nid_no_patch((std::string("exec: executables outside /app0 (") + guest + ")").c_str());
    relative.remove_prefix(ExecApplicationRoot.size());
    std::string name;
    while (true) {
        const std::size_t separator = relative.find('/');
        const std::string_view component = relative.substr(0, separator);
        if (component.empty() || component == "." || component == "..")
            NotImplemented_nid_no_patch((std::string("exec: a path with an empty, . or .. component (") + guest + ")").c_str());
        name += (name.empty() ? "" : ".") + std::string(component);
        if (separator == std::string_view::npos) return name;
        relative.remove_prefix(separator + 1);
    }
}

std::filesystem::path ExecOwnExecutable() {
#if defined(_WIN32)
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    if (length == 0 || length >= own.size())
        throw std::system_error(GetLastError(), std::system_category(), "exec: GetModuleFileNameW");
    own.resize(length);
    return std::filesystem::path(own);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) return std::filesystem::canonical(executable.data(), error);
    throw std::runtime_error("exec: cannot locate the executable");
#else
    std::error_code error;
    const auto own = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) throw std::system_error(error, "exec: cannot read /proc/self/exe");
    return own;
#endif
}

std::filesystem::path ExecConvertedTarget(const std::string& guest) {
    const auto own = ExecOwnExecutable();
    if (guest == ExecMainExecutable) return own;
    std::error_code error;
    const auto name = std::filesystem::path(reinterpret_cast<const char8_t*>((ExecFlattenedName(guest) + own.extension().string()).c_str()));
    const auto target = own.parent_path() / name;
    if (!std::filesystem::is_regular_file(target, error))
        NotImplemented_nid_no_patch((std::string("exec: ") + guest + " without its converted executable; relink with --executable " +
                                     std::string(guest.substr(ExecApplicationRoot.size())))
                                        .c_str());
    return target;
}

std::size_t ExecEntriesBytes(char* const* entries) {
    std::size_t bytes = 0;
    for (std::size_t index = 0; entries[index] != nullptr; ++index)
        bytes += std::strlen(entries[index]) + 1;
    return bytes;
}

std::vector<std::string> ExecArgumentVector(char* const* arguments) {
    std::vector<std::string> values;
    for (std::size_t index = 0; arguments[index] != nullptr; ++index) values.emplace_back(arguments[index]);
    return values;
}

#ifdef _WIN32
std::wstring ExecWide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("exec: argument is not valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring ExecQuoted(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
    std::wstring quoted = L"\"";
    for (std::size_t index = 0;; ++index) {
        std::size_t backslashes = 0;
        while (index < argument.size() && argument[index] == L'\\') {
            ++backslashes;
            ++index;
        }
        if (index == argument.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        quoted.append(argument[index] == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        quoted.push_back(argument[index]);
    }
    quoted.push_back(L'"');
    return quoted;
}

void ExecDuplicate(HANDLE source, DWORD access, HANDLE* target) {
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), target, access, TRUE, access ? 0 : DUPLICATE_SAME_ACCESS))
        throw std::system_error(GetLastError(), std::system_category(), "exec: duplicating a handle");
}

void ExecCloseHandle(HANDLE value) { CloseHandle(value); }

bool ExecWindowsImage(const std::filesystem::path& target) {
    std::ifstream image(target, std::ios::binary);
    std::array<char, 64> dos{};
    if (!image.read(dos.data(), dos.size()) || dos[0] != 'M' || dos[1] != 'Z') return false;
    const auto header = static_cast<std::size_t>(static_cast<unsigned char>(dos[63])) << 24 |
                        static_cast<std::size_t>(static_cast<unsigned char>(dos[62])) << 16 |
                        static_cast<std::size_t>(static_cast<unsigned char>(dos[61])) << 8 |
                        static_cast<std::size_t>(static_cast<unsigned char>(dos[60]));
    if (header < dos.size()) return false;
    image.seekg(static_cast<std::streamoff>(header));
    std::array<char, 6> coff{};
    if (!image.read(coff.data(), coff.size())) return false;
    return coff[0] == 'P' && coff[1] == 'E' && coff[2] == '\0' && coff[3] == '\0' &&
           static_cast<unsigned char>(coff[4]) == 0x64 && static_cast<unsigned char>(coff[5]) == 0x86;
}

[[noreturn]] void ExecReplaceWindows(const std::filesystem::path& target, const std::vector<std::string>& arguments, bool implicitEnvironment,
                                     char* const* environment) {
    constexpr wchar_t ParentVariable[] = L"ANYPS5_LOADEXEC_PARENT=";
    constexpr wchar_t SkipVariable[] = L"ANYPS5_LOADEXEC_ARGV_SKIP=";
    const wchar_t* parentPrefix = ParentVariable;
    const wchar_t* skipPrefix = SkipVariable;
    const std::size_t parentPrefixLength = std::size(ParentVariable) - 1;
    const std::size_t skipPrefixLength = std::size(SkipVariable) - 1;
    std::wstring command = ExecQuoted(target.wstring());
    for (const auto& argument : arguments) {
        if (!command.empty()) command += L' ';
        command += ExecQuoted(ExecWide(argument));
    }
    if (command.size() >= 32767) throw std::runtime_error("exec: command line is too long");
    std::vector<std::wstring> entries;
    const std::unique_ptr<wchar_t, decltype(&FreeEnvironmentStringsW)> inherited(GetEnvironmentStringsW(), FreeEnvironmentStringsW);
    if (!inherited) throw std::runtime_error("exec: cannot read the process environment");
    for (const wchar_t* at = inherited.get(); *at;) {
        const std::wstring entry(at);
        at += entry.size() + 1;
        entries.push_back(std::move(entry));
    }
    if (!implicitEnvironment) {
        const auto name = [](const std::wstring& entry) {
            const std::size_t separator = entry.find(L'=');
            return separator == std::wstring::npos ? entry : entry.substr(0, separator);
        };
        for (std::size_t index = 0; environment[index] != nullptr; ++index) {
            std::wstring replaced = ExecWide(environment[index]);
            const std::wstring replacedName = name(replaced);
            for (std::size_t existing = 0; existing < entries.size();) {
                if (name(entries[existing]) == replacedName) {
                    entries.erase(entries.begin() + existing);
                    continue;
                }
                ++existing;
            }
            entries.push_back(std::move(replaced));
        }
    }
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [parentPrefix, parentPrefixLength, skipPrefix, skipPrefixLength](const std::wstring& entry) {
                                     return _wcsnicmp(entry.c_str(), parentPrefix, parentPrefixLength) == 0 ||
                                            _wcsnicmp(entry.c_str(), skipPrefix, skipPrefixLength) == 0;
                                 }),
                  entries.end());
    std::unique_ptr<void, void (*)(HANDLE)> parent(nullptr, ExecCloseHandle);
    {
        HANDLE shared = nullptr;
        ExecDuplicate(GetCurrentProcess(), SYNCHRONIZE, &shared);
        parent.reset(shared);
    }
    std::vector<HANDLE> inheritedHandles{parent.get()};
    std::vector<std::unique_ptr<void, void (*)(HANDLE)>> standard;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    HANDLE* outputs[] = {&startup.StartupInfo.hStdInput, &startup.StartupInfo.hStdOutput, &startup.StartupInfo.hStdError};
    const DWORD identifiers[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (std::size_t index = 0; index < 3; ++index) {
        const HANDLE source = GetStdHandle(identifiers[index]);
        if (source && source != INVALID_HANDLE_VALUE) {
            HANDLE handle = nullptr;
            ExecDuplicate(source, 0, &handle);
            standard.emplace_back(handle, ExecCloseHandle);
            inheritedHandles.push_back(handle);
            *outputs[index] = handle;
        }
    }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
        throw std::system_error(GetLastError(), std::system_category(), "exec: creating process attributes");
    const std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, void (*)(LPPROC_THREAD_ATTRIBUTE_LIST)> cleanup(attributes, DeleteProcThreadAttributeList);
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles.data(),
                                   inheritedHandles.size() * sizeof(HANDLE), nullptr, nullptr))
        throw std::system_error(GetLastError(), std::system_category(), "exec: selecting inherited handles");
    startup.lpAttributeList = attributes;
    std::wostringstream value;
    value << ParentVariable << std::hex << reinterpret_cast<std::uintptr_t>(parent.get());
    entries.push_back(value.str());
    entries.push_back(std::wstring(SkipVariable) + L"1");
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
    std::vector<wchar_t> block;
    for (const auto& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(0);
    }
    block.push_back(0);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(target.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, block.data(), nullptr,
                        &startup.StartupInfo, &process))
        throw std::system_error(GetLastError(), std::system_category(), "exec: creating the converted executable");
    const std::unique_ptr<void, void (*)(HANDLE)> child(process.hProcess, ExecCloseHandle);
    const std::unique_ptr<void, void (*)(HANDLE)> thread(process.hThread, ExecCloseHandle);
    try {
        if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
            throw std::system_error(GetLastError(), std::system_category(), "exec: starting the converted executable");
        TerminateProcess(GetCurrentProcess(), 0);
    } catch (...) {
        TerminateProcess(process.hProcess, 1);
        throw;
    }
    std::abort();
}
#endif
}

extern "C" {

// unknown data
const char* __progname_nid_postfix = "eboot.bin";
static char* emptyEnvironment[1];
char** environ_nid_postfix = emptyEnvironment;

int APS5_VABI getargc_nid_postfix(void) {
    return getProcessArguments().GetCount();
}

const char** APS5_VABI getargv_nid_postfix(void) {
    return getProcessArguments().GetValues();
}

int APS5_VABI getpagesize_nid_postfix(void) {
    return PS5_PAGE_SIZE;
}

int APS5_VABI getpid_nid_postfix(void) {
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = ::getpid();
#endif
    if (pid == 0 || static_cast<std::uint64_t>(pid) > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Process identifier is outside the guest range");
    return static_cast<int>(pid);
}

int APS5_VABI getsid_nid_postfix(int pid) {
    const int self = getpid_nid_postfix();
    if (pid == 0 || pid == self) return self;
    *__error_nid_postfix() = errnoNoSuchProcess;
    return -1;
}

int APS5_VABI setpgid_nid_postfix(int pid, int pgid) {
    if (pgid < 0) {
        *__error_nid_postfix() = errnoInvalidArgument;
        return -1;
    }
    const int self = getpid_nid_postfix();
    if (pid != 0 && pid != self) {
        *__error_nid_postfix() = errnoNoSuchProcess;
        return -1;
    }
    *__error_nid_postfix() = errnoOperationNotPermitted;
    return -1;
}

int APS5_VABI setpgrp_nid_postfix(int pid, int pgrp) {
    return setpgid_nid_postfix(pid, pgrp);
}

int APS5_VABI getppid_nid_postfix(void) {
#ifdef _WIN32
    const DWORD self = GetCurrentProcessId();
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "getppid: CreateToolhelp32Snapshot");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    DWORD pid = 0;
    for (BOOL more = Process32FirstW(snapshot, &entry); more && !found; more = Process32NextW(snapshot, &entry)) {
        if (entry.th32ProcessID == self) {
            pid = entry.th32ParentProcessID;
            found = true;
        }
    }
    CloseHandle(snapshot);
    if (!found) throw std::runtime_error("getppid: the process is missing from the process snapshot");
#else
    const auto pid = ::getppid();
#endif
    if (static_cast<std::uint64_t>(pid) > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Parent process identifier is outside the guest range");
    return static_cast<int>(pid);
}

int APS5_VABI sceKernelGetAppInfo(int pid, void* info) {
    const bool own = pid == getpid_nid_postfix();
    if (!own && pid != 0) NotImplemented_nid_no_patch("sceKernelGetAppInfo: a pid other than the caller's and 0");
    if (info == nullptr) return 0;
    constexpr std::size_t StructureBytes = 0x58;
    constexpr std::size_t TitleIdOffset = 0x10;
    constexpr std::size_t TitleIdBytes = 10;
    constexpr std::size_t MarkerOffset = 0x50;
    auto* bytes = static_cast<std::uint8_t*>(info);
    std::memset(bytes, 0, StructureBytes);
    if (own) {
        const auto titleId = GetAppTitleId_nid_postfix();
        char field[TitleIdBytes] = {};
        std::strncpy(field, titleId.value, sizeof(field) - 1);
        const std::uint32_t appId = 1;
        std::memcpy(bytes, &appId, sizeof(appId));
        std::memcpy(bytes + TitleIdOffset, field, TitleIdBytes);
    }
    const std::uint32_t marker = 0xffffffffu;
    std::memcpy(bytes + MarkerOffset, &marker, sizeof(marker));
    return 0;
}

int APS5_VABI getuid_nid_postfix(void) {
    return 0;
}

int APS5_VABI geteuid_nid_postfix(void) {
    return 0;
}

int APS5_VABI getgid_nid_postfix(void) {
    return 0;
}

int APS5_VABI getegid_nid_postfix(void) {
    return 0;
}

int APS5_VABI issetugid_nid_postfix(void) {
    return 0;
}

int APS5_VABI getresuid_nid_postfix(std::uint32_t* ruid, std::uint32_t* euid, std::uint32_t* suid) {
    if (!StoreGuestId(ruid) || !StoreGuestId(euid) || !StoreGuestId(suid)) {
        *__error_nid_postfix() = errnoBadAddress;
        return -1;
    }
    return 0;
}

int APS5_VABI getresgid_nid_postfix(std::uint32_t* rgid, std::uint32_t* egid, std::uint32_t* sgid) {
    if (!StoreGuestId(rgid) || !StoreGuestId(egid) || !StoreGuestId(sgid)) {
        *__error_nid_postfix() = errnoBadAddress;
        return -1;
    }
    return 0;
}

#ifndef __EMSCRIPTEN__
void APS5_VABI exit_nid_postfix(int code) {
    LibcExit_nid_no_patch(code);
}
#endif

[[noreturn]] void APS5_VABI _exit_nid_postfix(int status) {
    LibcTerminate_nid_no_patch(status);
}

int APS5_VABI system_nid_postfix(const char* command) {
    constexpr int shellNotExecuted = 127 << 8;
    return command == nullptr ? 1 : shellNotExecuted;
}

int APS5_VABI waitpid_nid_postfix(int pid, int* status, int options) {
    if ((static_cast<unsigned>(options) & ~freebsdWaitOptions) != 0) {
        *__error_nid_postfix() = errnoInvalidArgument;
        return -1;
    }
#ifdef _WIN32
    int encoded = 0;
    const std::int64_t reaped = WaitChildProcess_nid_no_patch(pid, static_cast<unsigned>(options), &encoded);
    if (reaped == -2) {
        *__error_nid_postfix() = errnoNoChild;
        return -1;
    }
    if (reaped == -1) return -1;
    if (reaped == 0) return 0;
    if (status != nullptr) *status = encoded;
    return static_cast<int>(reaped);
#else
    unsigned hostOptions = static_cast<unsigned>(options) & 3u;
    if ((static_cast<unsigned>(options) & 4u) != 0) hostOptions |= static_cast<unsigned>(WCONTINUED);
    int encoded = 0;
    const int result = ::waitpid(pid, &encoded, static_cast<int>(hostOptions));
    if (result < 0) {
        *__error_nid_postfix() = errno;
        return -1;
    }
    if (result == 0) return 0;
    if (WIFCONTINUED(encoded)) {
        encoded = 0x13;
    } else if (WIFSIGNALED(encoded) || WIFSTOPPED(encoded)) {
        const int hostSignal = WIFSIGNALED(encoded) ? WTERMSIG(encoded) : WSTOPSIG(encoded);
        int guestSignal = hostSignal;
#ifndef __APPLE__
        switch (hostSignal) {
        case 7: guestSignal = 10; break;
        case 10: guestSignal = 30; break;
        case 12: guestSignal = 31; break;
        case 17: guestSignal = 20; break;
        case 18: guestSignal = 19; break;
        case 19: guestSignal = 17; break;
        case 20: guestSignal = 18; break;
        case 23: guestSignal = 16; break;
        case 29: guestSignal = 23; break;
        case 31: guestSignal = 12; break;
        case 16:
        case 30:
            NotImplemented_nid_no_patch("waitpid: a child stopped or died with a signal without a guest equivalent");
            return -1;
        default: break;
        }
#endif
        encoded = WIFSTOPPED(encoded) ? ((encoded & ~0xff00) | (guestSignal << 8)) : ((encoded & ~0x7f) | guestSignal);
    }
    if (status != nullptr) *status = encoded;
    return result;
#endif
}

int APS5_VABI setrlimit_nid_postfix(int resource, const GuestResourceLimit* limit) {
    GuestResourceLimit requested{};
    if (limit == nullptr || !GuestCopyIn_nid_no_patch(&requested, limit, sizeof(requested))) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    if (resource < 0 || resource >= guestRlimitCount) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }

    if (requested.rlim_cur < 0)
        requested.rlim_cur = guestRlimitInfinity;
    if (requested.rlim_max < 0)
        requested.rlim_max = guestRlimitInfinity;
    if (requested.rlim_cur > requested.rlim_max)
        requested.rlim_cur = requested.rlim_max;

    GuestResourceLimit current{};
    getrlimit_nid_postfix(resource, &current);

    if (resource == guestRlimitNofile) {
        if (requested.rlim_max <= current.rlim_max && requested.rlim_cur <= requested.rlim_max) {
            std::lock_guard lock(openFileLimitMutex);
            guestOpenFileLimit = requested;
            return 0;
        }
    }
    if (resource == guestRlimitNproc) {
        requested.rlim_cur = std::clamp(requested.rlim_cur, std::int64_t{1}, guestProcessLimitPerUser);
        requested.rlim_max = std::clamp(requested.rlim_max, std::int64_t{1}, guestProcessLimitPerUser);
        std::lock_guard lock(processLimitMutex);
        guestProcessResourceLimit = requested;
        return 0;
    }
    if (resource == guestRlimitCore && requested.rlim_max == current.rlim_max) {
        std::lock_guard lock(coreLimitMutex);
        guestCoreLimit.rlim_cur = requested.rlim_cur;
        return 0;
    }
    if ((resource == guestRlimitData || resource == guestRlimitStack) && requested.rlim_cur == current.rlim_cur && requested.rlim_max > current.rlim_max)
        return 0;
    if (requested.rlim_cur != current.rlim_cur || requested.rlim_max != current.rlim_max)
        throw std::runtime_error("setrlimit: changing the limit of resource " + std::to_string(resource) + " is not supported");
    return 0;
}

int APS5_VABI wait_nid_postfix(int* status) {
    return waitpid_nid_postfix(-1, status, 0);
}

int ExecProgram(const char* file, char* const* arguments, char* const* environment, bool search, bool implicitEnvironment) {
    const auto fail = [](int error) { *__error_nid_postfix() = error; return -1; };
    if (file == nullptr) return fail(errnoBadFile);
    if (arguments == nullptr) return fail(errnoBadAddress);
    if (file[0] == '\0') return fail(errnoNoEnt);
    static char* const noEnvironment[] = {nullptr};
    char* const* entries = implicitEnvironment ? static_cast<char* const*>(environ) : (environment != nullptr ? environment : noEnvironment);
    if (ExecEntriesBytes(arguments) + ExecEntriesBytes(entries) > ExecArgumentBytesLimit) return fail(errnoTooBig);
    int error = errnoNoEnt;
    std::string program;
    if (!ExecProgramFound(file, search, error, program)) return fail(error);
    const auto target = ExecConvertedTarget(program);
#ifdef _WIN32
    if (!ExecWindowsImage(target)) return fail(errnoNoExec);
    ExecReplaceWindows(target, ExecArgumentVector(arguments), implicitEnvironment, entries);
#elif defined(__linux__)
    const int descriptor = ::open(target.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "exec: opening the converted executable");
    std::array<unsigned char, 20> header{};
    if (::pread(descriptor, header.data(), header.size(), 0) != static_cast<ssize_t>(header.size()) || header[0] != 0x7f ||
        header[1] != 'E' || header[2] != 'L' || header[3] != 'F' || header[4] != 2 || header[5] != 1 ||
        (header[7] != 0 && header[7] != 3) || header[17] != 0 || (header[16] != 2 && header[16] != 3) || header[18] != 62 ||
        header[19] != 0) {
        ::close(descriptor);
        return fail(errnoNoExec);
    }
    if (::access(target.c_str(), X_OK) != 0) {
        ::close(descriptor);
        return fail(errnoAccess);
    }
    std::vector<std::string> merged;
    if (!implicitEnvironment)
        for (std::size_t index = 0; entries[index] != nullptr; ++index) merged.emplace_back(entries[index]);
    for (std::size_t index = 0; environ[index] != nullptr; ++index) {
        const std::string_view entry = environ[index];
        const std::size_t separator = entry.find('=');
        const std::string_view name = separator == std::string_view::npos ? entry : entry.substr(0, separator);
        bool overridden = false;
        for (const auto& guest : merged) {
            const std::size_t guestSeparator = guest.find('=');
            if ((guestSeparator == std::string::npos ? std::string_view(guest) : std::string_view(guest).substr(0, guestSeparator)) == name) {
                overridden = true;
                break;
            }
        }
        if (!overridden) merged.emplace_back(environ[index]);
    }
    std::vector<char*> replacement;
    for (auto& entry : merged) replacement.push_back(entry.data());
    replacement.push_back(nullptr);
    fexecve(descriptor, arguments, replacement.data());
    const int replaced = errno;
    ::close(descriptor);
    throw std::system_error(replaced, std::generic_category(), "exec: replacing the executable");
#else
    static_cast<void>(target);
    NotImplemented_nid_no_patch("exec: process replacement on this host");
    return -1;
#endif
}

int APS5_VABI execvp_nid_postfix(const char* file, char* const* arguments) {
    return ExecProgram(file, arguments, nullptr, true, true);
}

int APS5_VABI execv_nid_postfix(const char* path, char* const* arguments) {
    return ExecProgram(path, arguments, nullptr, false, true);
}

int APS5_VABI execve_nid_postfix(const char* path, char* const* arguments, char* const* environment) {
    return ExecProgram(path, arguments, environment, false, false);
}

int APS5_VABI sceKernelGetCurrentCpu(void) {
#ifdef _WIN32
    PROCESSOR_NUMBER processor{};
    GetCurrentProcessorNumberEx(&processor);
    unsigned index = processor.Number;
    for (WORD group = 0; group < processor.Group; ++group) {
        const auto count = GetActiveProcessorCount(group);
        if (count == 0)
            throw std::system_error(GetLastError(), std::system_category(), "Reading processor group size");
        index += count;
    }
#else
    const int cpu = ::sched_getcpu();
    if (cpu < 0)
        throw std::system_error(errno, std::generic_category(), "Reading current processor");
    const auto index = static_cast<unsigned>(cpu);
#endif
    return GuestCpuFromHost(index, scePthreadSelf()->affinity.load(std::memory_order_relaxed));
}

int APS5_VABI sceKernelGetMainSocId(void) {
    return 0;
}

int APS5_VABI sceKernelGetCpumode(void) {
    return 0;
}

std::uint64_t APS5_VABI sceKernelGetGPI(void) {
    return gpoBits.load(std::memory_order_relaxed);
}

void APS5_VABI sceKernelSetGPO(std::uint32_t bits) {
    gpoBits.store(bits, std::memory_order_relaxed);
}

int APS5_VABI sceKernelGetOpenPsId(void* openPsIdOutput) {
    if (!openPsIdOutput)
        throw std::invalid_argument("sceKernelGetOpenPsId: null output");
    std::memcpy(openPsIdOutput, openPsId.data(), openPsId.size());
    return 0;
}

void* APS5_VABI sceKernelGetProcParam(void) {
    return const_cast<void*>(ApplicationProcessParameters_nid_no_patch());
}

int APS5_VABI sceKernelUuidCreate(std::uint32_t* uuid) {
    if (!uuid) return sceInvalidArgument;
    std::array<std::uint32_t, 4> value;
    HostEntropy_nid_no_patch(value.data(), sizeof(value));
    value[1] = (value[1] & 0x0fffffffu) | 0x40000000u;
    value[2] = (value[2] & 0xffffff3fu) | 0x80u;
    if (!GuestCopyOut_nid_no_patch(uuid, value.data(), sizeof(value))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelCreateUuidV4(std::uint8_t* uuid) {
    if (!uuid) return sceInvalidArgument;
    std::array<std::uint8_t, 16> value;
    HostEntropy_nid_no_patch(value.data(), value.size());
    value[6] = static_cast<std::uint8_t>((value[6] & 0x0fu) | 0x40u);
    value[8] = static_cast<std::uint8_t>((value[8] & 0x3fu) | 0x80u);
    if (!GuestCopyOut_nid_no_patch(uuid, value.data(), value.size())) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

void APS5_VABI sceKernelSync(void) {
    SyncWrittenPaths_nid_no_patch();
}

void APS5_VABI sync_nid_postfix(void) {
    sceKernelSync();

}

int APS5_VABI sched_get_priority_max_nid_postfix(int policy) {
    if (!hasPriorityRange(policy)) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    return 767;
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy) {
    if (!hasPriorityRange(policy)) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    return 256;
}

int APS5_VABI sysarch(int operation, void* parameters) {
    constexpr int i386GetFsBase = 7;
    constexpr int i386GetGsBase = 9;
    constexpr int getFsBase = 128;
    constexpr int getGsBase = 130;
    constexpr int setGsBase = 131;
    constexpr std::uint64_t userAddressEnd = 0x0000800000000000ull;
    constexpr std::array<int, 14> definedOperations{0, 1, 3, 4, 7, 8, 9, 10, 11, 128, 129, 130, 131, 132};
    if (std::find(definedOperations.begin(), definedOperations.end(), operation) == definedOperations.end()) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    const bool i386 = operation == i386GetFsBase || operation == i386GetGsBase;
    if (!i386 && operation != getFsBase && operation != getGsBase && operation != setGsBase)
        NotImplemented_nid_no_patch(("sysarch: operation " + std::to_string(operation)).c_str());
#if defined(__linux__) && defined(__x86_64__)
    if (parameters == nullptr) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    std::uint64_t base = 0;
    if (operation == setGsBase) {
        std::memcpy(&base, parameters, sizeof(base));
        if (base >= userAddressEnd) {
            *__error_nid_postfix() = guestInvalid;
            return -1;
        }
        if (::syscall(SYS_arch_prctl, ARCH_SET_GS, base) != 0) throw std::system_error(errno, std::generic_category(), "sysarch: setting the GS base");
        return 0;
    }
    const bool fs = operation == getFsBase || operation == i386GetFsBase;
    if (::syscall(SYS_arch_prctl, fs ? ARCH_GET_FS : ARCH_GET_GS, &base) != 0)
        throw std::system_error(errno, std::generic_category(), "sysarch: reading a segment base");
    if (i386) {
        const auto low = static_cast<std::uint32_t>(base);
        std::memcpy(parameters, &low, sizeof(low));
        return 0;
    }
    std::memcpy(parameters, &base, sizeof(base));
    return 0;
#else
    static_cast<void>(parameters);
    NotImplemented_nid_no_patch("sysarch: segment bases on this host");
    return 0;
#endif
}

int APS5_VABI getrusage_nid_postfix(int who, GuestResourceUsage* usage) {
    if (who != -1 && who != 0 && who != 1) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    if (usage == nullptr) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    GuestResourceUsage result{};
    if (who == -1) {
        if (!WriteGuest(usage, &result, sizeof(result))) {
            *__error_nid_postfix() = guestFault;
            return -1;
        }
        return 0;
    }
#ifdef _WIN32
    FILETIME creation{};
    FILETIME exitTime{};
    FILETIME kernel{};
    FILETIME user{};
    if (who == 0) {
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "getrusage: GetProcessTimes failed");
    } else {
        if (!GetThreadTimes(GetCurrentThread(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "getrusage: GetThreadTimes failed");
    }
    const auto toMicros = [](const FILETIME& time) {
        return ((static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime) / 10ULL;
    };
    const auto userMicros = toMicros(user);
    const auto kernelMicros = toMicros(kernel);
    result.ru_utime.tv_sec = static_cast<std::int64_t>(userMicros / 1000000ULL);
    result.ru_utime.tv_usec = static_cast<std::int64_t>(userMicros % 1000000ULL);
    result.ru_stime.tv_sec = static_cast<std::int64_t>(kernelMicros / 1000000ULL);
    result.ru_stime.tv_usec = static_cast<std::int64_t>(kernelMicros % 1000000ULL);
#else
    struct rusage native{};
    if (::getrusage(who == 0 ? RUSAGE_SELF : RUSAGE_THREAD, &native) != 0)
        throw std::system_error(errno, std::generic_category(), "getrusage: getrusage failed");
    result.ru_utime.tv_sec = static_cast<std::int64_t>(native.ru_utime.tv_sec);
    result.ru_utime.tv_usec = static_cast<std::int64_t>(native.ru_utime.tv_usec);
    result.ru_stime.tv_sec = static_cast<std::int64_t>(native.ru_stime.tv_sec);
    result.ru_stime.tv_usec = static_cast<std::int64_t>(native.ru_stime.tv_usec);
    result.ru_maxrss = static_cast<std::int64_t>(native.ru_maxrss);
    result.ru_ixrss = static_cast<std::int64_t>(native.ru_ixrss);
    result.ru_idrss = static_cast<std::int64_t>(native.ru_idrss);
    result.ru_isrss = static_cast<std::int64_t>(native.ru_isrss);
    result.ru_minflt = static_cast<std::int64_t>(native.ru_minflt);
    result.ru_majflt = static_cast<std::int64_t>(native.ru_majflt);
    result.ru_nswap = static_cast<std::int64_t>(native.ru_nswap);
    result.ru_inblock = static_cast<std::int64_t>(native.ru_inblock);
    result.ru_oublock = static_cast<std::int64_t>(native.ru_oublock);
    result.ru_msgsnd = static_cast<std::int64_t>(native.ru_msgsnd);
    result.ru_msgrcv = static_cast<std::int64_t>(native.ru_msgrcv);
    result.ru_nsignals = static_cast<std::int64_t>(native.ru_nsignals);
    result.ru_nvcsw = static_cast<std::int64_t>(native.ru_nvcsw);
    result.ru_nivcsw = static_cast<std::int64_t>(native.ru_nivcsw);
#endif
    if (!WriteGuest(usage, &result, sizeof(result))) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    return 0;
}

int APS5_VABI getrlimit_nid_postfix(int resource, GuestResourceLimit* limit) {
    if (resource < 0 || resource >= guestRlimitCount) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    if (limit == nullptr) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    GuestResourceLimit result{};
    switch (resource) {
    case guestRlimitData: {
#ifdef _WIN32
        const std::int64_t memory = hostMemoryLimit();
        result.rlim_cur = memory;
        result.rlim_max = memory;
#else
        rlimit native{};
        if (::getrlimit(RLIMIT_DATA, &native) != 0)
            throw std::system_error(errno, std::generic_category(), "getrlimit: getrlimit failed");
        result.rlim_cur = toGuestLimit(native.rlim_cur);
        result.rlim_max = toGuestLimit(native.rlim_max);
#endif
        break;
    }
    case guestRlimitNofile: {
        std::lock_guard lock(openFileLimitMutex);
        if (!guestOpenFileLimit) {
#ifdef _WIN32
            constexpr std::int64_t crtDescriptors = 8192;
            guestOpenFileLimit = GuestResourceLimit{crtDescriptors, crtDescriptors};
#else
            rlimit native{};
            if (::getrlimit(RLIMIT_NOFILE, &native) != 0)
                throw std::system_error(errno, std::generic_category(), "getrlimit: getrlimit failed");
            guestOpenFileLimit = GuestResourceLimit{toGuestLimit(native.rlim_cur), toGuestLimit(native.rlim_max)};
#endif
        }
        result = *guestOpenFileLimit;
        break;
    }
    case guestRlimitStack:
        result.rlim_cur = guestStackLimit;
        result.rlim_max = guestStackLimit;
        break;
    case guestRlimitNproc: {
        std::lock_guard lock(processLimitMutex);
        result = guestProcessResourceLimit;
        break;
    }
    case guestRlimitCore: {
        std::lock_guard lock(coreLimitMutex);
        result = guestCoreLimit;
        break;
    }
    case guestRlimitCpu:
    case guestRlimitFsize:
    case guestRlimitRss:
    case guestRlimitMemlock:
    case guestRlimitSbsize:
    case guestRlimitVmem:
    case guestRlimitNpts:
    case guestRlimitSwap:
    case guestRlimitKqueues:
    case guestRlimitUmtxp:
        result.rlim_cur = guestRlimitInfinity;
        result.rlim_max = guestRlimitInfinity;
        break;
    default:
        throw std::runtime_error("getrlimit: unsupported resource " + std::to_string(resource));
    }
    if (!WriteGuest(limit, &result, sizeof(result))) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    return 0;
}

int APS5_VABI getpriority_nid_postfix(int which, int who) {
    if (which != guestPriorityProcess && which != guestPriorityGroup && which != guestPriorityUser) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    const bool found = which == guestPriorityUser ? (who == 0 ? getuid_nid_postfix() : who) == getuid_nid_postfix()
                                                  : who == 0 || who == getpid_nid_postfix();
    if (!found) {
        *__error_nid_postfix() = guestNoProcess;
        return -1;
    }
    return 0;
}

int APS5_VABI setpriority_nid_postfix(int which, int who, int niceval) {
    if (which != guestPriorityProcess && which != guestPriorityGroup && which != guestPriorityUser) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    const bool found = which == guestPriorityUser ? (who == 0 ? getuid_nid_postfix() : who) == getuid_nid_postfix()
                                                  : who == 0 || who == getpid_nid_postfix();
    if (!found) {
        *__error_nid_postfix() = guestNoProcess;
        return -1;
    }
    if (which != guestPriorityProcess) {
        throw std::runtime_error(std::string("setpriority: setting the nice value of ") +
                                 (which == guestPriorityGroup ? "PRIO_PGRP" : "PRIO_USER") + " to " + std::to_string(niceval) +
                                 " is not verified on the console");
    }
    return 0;
}

int APS5_VABI getdtablesize_nid_postfix(void) {
    GuestResourceLimit limit{};
    getrlimit_nid_postfix(guestRlimitNofile, &limit);

    std::int64_t tableSize = limit.rlim_cur;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    tableSize = std::min(tableSize, static_cast<std::int64_t>(::getdtablesize()));
#endif
    if (tableSize > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(tableSize);
}

int APS5_VABI sceKernelIsTrinityMode(void) {
    return 0;
}

int APS5_VABI sceKernelHasTrinityMode(void) {
    return 0;
}

void APS5_VABI sceKernelGetOperationMode(int* mode, int* submode) {
    if (!mode || !submode)
        throw std::invalid_argument("sceKernelGetOperationMode: null output");
    *mode = 2;
    *submode = 0;
}

int APS5_VABI seteuid_nid_postfix(std::uint32_t euid) {
    if (euid != 0)
        throw std::runtime_error("seteuid: unsupported user id " + std::to_string(euid));
    return 0;
}

int APS5_VABI setegid_nid_postfix(std::uint32_t egid) {
    if (egid != 0)
        throw std::runtime_error("setegid: unsupported group id " + std::to_string(egid));
    return 0;
}

int APS5_VABI setuid_nid_postfix(std::uint32_t uid) {
    if (uid != 0)
        throw std::runtime_error("setuid: unsupported user id " + std::to_string(uid));
    return 0;
}

int APS5_VABI setgid_nid_postfix(std::uint32_t gid) {
    if (gid != 0)
        throw std::runtime_error("setgid: unsupported group id " + std::to_string(gid));
    return 0;
}

int APS5_VABI setreuid_nid_postfix(std::uint32_t ruid, std::uint32_t euid) {
    constexpr std::uint32_t unchanged = 0xffffffffu;
    if (ruid != 0 && ruid != unchanged)
        throw std::runtime_error("setreuid: unsupported real user id " + std::to_string(ruid));
    if (euid != 0 && euid != unchanged)
        throw std::runtime_error("setreuid: unsupported effective user id " + std::to_string(euid));
    return 0;
}

int APS5_VABI setregid_nid_postfix(std::uint32_t rgid, std::uint32_t egid) {
    constexpr std::uint32_t unchanged = 0xffffffffu;
    if (rgid != 0 && rgid != unchanged)
        throw std::runtime_error("setregid: unsupported real group id " + std::to_string(rgid));
    if (egid != 0 && egid != unchanged)
        throw std::runtime_error("setregid: unsupported effective group id " + std::to_string(egid));
    return 0;
}

int APS5_VABI setresuid_nid_postfix(std::uint32_t ruid, std::uint32_t euid, std::uint32_t suid) {
    constexpr std::uint32_t unchanged = 0xffffffffu;
    if (ruid != 0 && ruid != unchanged)
        throw std::runtime_error("setresuid: unsupported real user id " + std::to_string(ruid));
    if (euid != 0 && euid != unchanged)
        throw std::runtime_error("setresuid: unsupported effective user id " + std::to_string(euid));
    if (suid != 0 && suid != unchanged)
        throw std::runtime_error("setresuid: unsupported saved user id " + std::to_string(suid));
    return 0;
}

int APS5_VABI setresgid_nid_postfix(std::uint32_t rgid, std::uint32_t egid, std::uint32_t sgid) {
    constexpr std::uint32_t unchanged = 0xffffffffu;
    if (rgid != 0 && rgid != unchanged)
        throw std::runtime_error("setresgid: unsupported real group id " + std::to_string(rgid));
    if (egid != 0 && egid != unchanged)
        throw std::runtime_error("setresgid: unsupported effective group id " + std::to_string(egid));
    if (sgid != 0 && sgid != unchanged)
        throw std::runtime_error("setresgid: unsupported saved group id " + std::to_string(sgid));
    return 0;
}

int APS5_VABI setgroups_nid_postfix(int count, const std::uint32_t* groups) {
    if (count < 0 || count > freebsdGroupsMax) {
        *__error_nid_postfix() = errnoInvalidArgument;
        return -1;
    }
    if (count > 0 && groups == nullptr) {
        *__error_nid_postfix() = errnoBadAddress;
        return -1;
    }
    std::vector<std::uint32_t> requested;
    if (count > 0) {
        requested.resize(static_cast<std::size_t>(count));
        if (!ReadGuest(requested.data(), groups, requested.size() * sizeof(requested[0]))) {
            *__error_nid_postfix() = errnoBadAddress;
            return -1;
        }
    }
    if (count > 0 && requested[0] != 0)
        throw std::runtime_error("setgroups: unsupported effective group id " + std::to_string(requested[0]));
    std::lock_guard lock(supplementaryGroupsMutex);
    if (count == 0) {
        supplementaryGroups.resize(1);
        return 0;
    }
    supplementaryGroups.swap(requested);
    return 0;
}

int APS5_VABI getgroups_nid_postfix(int capacity, std::uint32_t* groups) {
    std::lock_guard lock(supplementaryGroupsMutex);
    const int count = static_cast<int>(supplementaryGroups.size());
    if (capacity == 0) return count;
    if (capacity < count) {
        *__error_nid_postfix() = errnoInvalidArgument;
        return -1;
    }
    if (groups == nullptr) {
        *__error_nid_postfix() = errnoBadAddress;
        return -1;
    }
    if (!WriteGuest(groups, supplementaryGroups.data(), supplementaryGroups.size() * sizeof(supplementaryGroups[0]))) {
        *__error_nid_postfix() = errnoBadAddress;
        return -1;
    }
    return count;
}

}
