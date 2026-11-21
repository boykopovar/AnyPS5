extern "C" {
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/System/include/ProcessWait.hpp"
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
#include "prx/libc/include/HostExecutable.hpp"
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
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach/mach.h>
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
    const int saved = errno;
    const auto copied = ::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno == EFAULT) errno = saved;
    return copied == static_cast<ssize_t>(sizeof(zero));
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
    const int saved = errno;
    const auto copied = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("credentials: process_vm_readv failed with errno " + std::to_string(errno));
    errno = saved;
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
    const int saved = errno;
    const auto copied = ::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("credentials: process_vm_writev failed with errno " + std::to_string(errno));
    errno = saved;
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
#elif defined(__APPLE__)
        const int count = *_NSGetArgc();
        char** values = *_NSGetArgv();
        for (int index = 0; index < count; ++index)
            arguments.push_back(values[index]);
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
constexpr int guestAccess = 13;
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

std::filesystem::path ExecConvertedTarget(const std::string& guest) {
    const auto own = HostExecutable::Own("exec");
    if (guest == ExecMainExecutable) return own;
    std::error_code error;
    const auto name = std::filesystem::path(reinterpret_cast<const char8_t*>((HostExecutable::ConvertedName(guest, [](const std::string& reason) { NotImplemented_nid_no_patch(("exec: " + reason).c_str()); }) + own.extension().string()).c_str()));
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
    std::wstring command = HostExecutable::QuotedArgument(target.wstring());
    for (const auto& argument : arguments) {
        if (!command.empty()) command += L' ';
        command += HostExecutable::QuotedArgument(HostExecutable::Wide(argument, "exec"));
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
            std::wstring replaced = HostExecutable::Wide(environment[index], "exec");
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


const char* __progname_nid_postfix = "eboot.bin";
static char* emptyEnvironment[1];
char** environ_nid_postfix = emptyEnvironment;

namespace {
struct EnvironmentBinding {
    EnvironmentBinding() { BindGuestEnvironment_nid_no_patch(&environ_nid_postfix); }
    ~EnvironmentBinding() { BindGuestEnvironment_nid_no_patch(nullptr); }
};
EnvironmentBinding environmentBinding;
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
    constexpr int guestMode = 2;
    constexpr int guestSubmode = 0;
    if (!GuestCopyOut_nid_no_patch(mode, &guestMode, sizeof(guestMode)) ||
        !GuestCopyOut_nid_no_patch(submode, &guestSubmode, sizeof(guestSubmode)))
        throw std::invalid_argument("sceKernelGetOperationMode: output is not writable");
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
    if (static_cast<std::uint32_t>(count) > static_cast<std::uint32_t>(freebsdGroupsMax) + 1) {
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
    const auto size = static_cast<std::uint32_t>(capacity);
    if (size == 0) return count;
    if (size < static_cast<std::uint32_t>(count)) {
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

namespace {

struct ProgramNameRegistration {
    ProgramNameRegistration() { LibcRegisterProgramName_nid_no_patch(&__progname_nid_postfix); }
    ~ProgramNameRegistration() { LibcRegisterProgramName_nid_no_patch(nullptr); }
    ProgramNameRegistration(const ProgramNameRegistration&) = delete;
    ProgramNameRegistration& operator=(const ProgramNameRegistration&) = delete;
};

const ProgramNameRegistration programNameRegistration;

}
