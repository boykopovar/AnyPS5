#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libc/include/ChildProcesses.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#include "prx/libc/include/WindowsDescriptor.hpp"
#endif
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#endif
#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#include <unistd.h>
#endif

extern "C" char* APS5_VABI getenv_nid_postfix(const char* name);
#if defined(__linux__)
extern char** environ;
#endif

namespace {

constexpr int NoSuchFile = 2;
constexpr int BadDescriptor = 9;
constexpr int OutOfMemory = 12;
constexpr int Fault = 14;
constexpr int InvalidArgument = 22;
constexpr std::string_view DefaultSearchPath = "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin";
constexpr std::string_view ApplicationRoot = "/app0/";
constexpr std::string_view MainExecutable = "/app0/eboot.bin";
#ifdef _WIN32
constexpr wchar_t LoadExecParentVariable[] = L"ANYPS5_LOADEXEC_PARENT";
constexpr wchar_t LoadExecSkipVariable[] = L"ANYPS5_LOADEXEC_ARGV_SKIP=";
#else
constexpr std::uint32_t SpawnFlagsImplemented = 0x03;
#endif
constexpr std::uint32_t SpawnFlagResetIds = 0x01;
constexpr std::uint32_t SpawnFlagSetGroup = 0x02;

PosixSpawnAttributes& Attributes(PosixSpawnAttributes* const* attributes, const char* function) {
    if (attributes == nullptr || *attributes == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized attributes");
    return **attributes;
}

PosixSpawnFileActions& FileActions(PosixSpawnFileActions* const* actions, const char* function) {
    if (actions == nullptr || *actions == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized file actions");
    return **actions;
}

int Append(PosixSpawnFileActions& actions, PosixSpawnFileAction action) {
    try {
        actions.actions.push_back(std::move(action));
    } catch (const std::bad_alloc&) {
        return OutOfMemory;
    }
    return 0;
}

bool GuestPathExists(const std::string& path) {
    std::filesystem::path native;
    if (ResolvePathError_nid_no_patch(path.c_str(), native) != 0) return false;
    std::error_code error;
    return std::filesystem::exists(native, error);
}

bool ProgramFound(std::string_view name, bool searchesPath, std::string& found) {
    if (name.empty()) return false;
    if (!searchesPath || name.find('/') != std::string_view::npos) {
        found = std::string(name);
        return GuestPathExists(found);
    }
    const char* variable = getenv_nid_postfix("PATH");
    std::string_view directories = variable != nullptr ? std::string_view(variable) : DefaultSearchPath;
    while (true) {
        const auto separator = directories.find(':');
        const auto directory = directories.substr(0, separator);
        found = (directory.empty() ? std::string(".") : std::string(directory)) + '/' + std::string(name);
        if (GuestPathExists(found)) return true;
        if (separator == std::string_view::npos) return false;
        directories.remove_prefix(separator + 1);
    }
}

std::string FlattenedExecutableName(const std::string& guest, const char* function) {
    std::string_view relative(guest);
    if (!relative.starts_with(ApplicationRoot)) NotImplemented_nid_no_patch((std::string(function) + ": executables outside /app0 (" + guest + ")").c_str());
    relative.remove_prefix(ApplicationRoot.size());
    std::string name;
    while (true) {
        const std::size_t separator = relative.find('/');
        const std::string_view component = relative.substr(0, separator);
        if (component.empty() || component == "." || component == "..") NotImplemented_nid_no_patch((std::string(function) + ": a path with an empty, . or .. component (" + guest + ")").c_str());
        name += (name.empty() ? "" : ".") + std::string(component);
        if (separator == std::string_view::npos) return name;
        relative.remove_prefix(separator + 1);
    }
}

std::filesystem::path OwnExecutable() {
#if defined(_WIN32)
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    if (length == 0 || length >= own.size()) throw std::system_error(GetLastError(), std::system_category(), "posix_spawn: GetModuleFileNameW");
    own.resize(length);
    return std::filesystem::path(own);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) return std::filesystem::canonical(executable.data(), error);
    throw std::runtime_error("posix_spawn: cannot locate the executable");
#else
    std::error_code error;
    const auto own = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) throw std::system_error(error, "posix_spawn: cannot read /proc/self/exe");
    return own;
#endif
}

std::filesystem::path ConvertedExecutableTarget(const char* function, const std::string& guest) {
    const auto own = OwnExecutable();
    if (guest == MainExecutable) return own;
    std::error_code error;
    const auto name = std::filesystem::path(reinterpret_cast<const char8_t*>((FlattenedExecutableName(guest, function) + own.extension().string()).c_str()));
    const auto target = own.parent_path() / name;
    if (!std::filesystem::is_regular_file(target, error))
        NotImplemented_nid_no_patch((std::string(function) + ": " + guest + " without its converted executable; relink with --executable " + std::string(guest.substr(ApplicationRoot.size()))).c_str());
    return target;
}

int HostOpenFlags(std::int32_t sceFlags) {
    int f = 0;
    const int acc = sceFlags & SCE_KERNEL_O_ACCMODE;
    if (acc == SCE_KERNEL_O_RDONLY) f |= 0;
    else if (acc == SCE_KERNEL_O_WRONLY) f |= 1;
    else if (acc == SCE_KERNEL_O_RDWR) f |= 2;
    else throw std::invalid_argument("posix_spawn_file_actions_addopen: invalid access mode");
#ifdef _WIN32
    if (sceFlags & SCE_KERNEL_O_APPEND) f |= _O_APPEND;
    if (sceFlags & SCE_KERNEL_O_CREAT) f |= _O_CREAT;
    if (sceFlags & SCE_KERNEL_O_TRUNC) f |= _O_TRUNC;
    if (sceFlags & SCE_KERNEL_O_EXCL) f |= _O_EXCL;
    f |= _O_BINARY;
#else
    if (sceFlags & SCE_KERNEL_O_APPEND) f |= O_APPEND;
    if (sceFlags & SCE_KERNEL_O_CREAT) f |= O_CREAT;
    if (sceFlags & SCE_KERNEL_O_TRUNC) f |= O_TRUNC;
    if (sceFlags & SCE_KERNEL_O_EXCL) f |= O_EXCL;
#endif
    return f;
}

#ifdef _WIN32
std::wstring Utf16(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("posix_spawn: argument is not valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring QuotedArgument(const std::wstring& argument) {
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
#endif

std::string HexFlag(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    if (value == 0) text.push_back('0');
    while (value > 0) {
        text.insert(text.begin(), digits[value & 0xfu]);
        value >>= 4;
    }
    return "0x" + text;
}

std::uint32_t SpawnAttributeFlags(PosixSpawnAttributes* const* attributes) {
    return attributes != nullptr && *attributes != nullptr ? static_cast<std::uint32_t>(static_cast<std::uint16_t>((*attributes)->flags)) : 0;
}

#ifdef _WIN32
struct ChildStreams {
    HANDLE handles[3] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
};

HANDLE DuplicateInheritable(HANDLE source) {
    HANDLE handle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &handle, 0, TRUE, DUPLICATE_SAME_ACCESS))
        throw std::system_error(GetLastError(), std::system_category(), "posix_spawn: duplicating a descriptor");
    return handle;
}

void CloseRedirectedStreams(ChildStreams& streams) {
    for (HANDLE handle : streams.handles)
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
}

void ReplaceChildStream(ChildStreams& streams, int descriptor, HANDLE source) {
    HANDLE previous = streams.handles[descriptor];
    streams.handles[descriptor] = source == INVALID_HANDLE_VALUE ? INVALID_HANDLE_VALUE : DuplicateInheritable(source);
    if (previous != INVALID_HANDLE_VALUE) CloseHandle(previous);
}

int ApplyChildRedirection(ChildStreams& streams, const PosixSpawnFileAction& action) {
    switch (action.kind) {
    case PosixSpawnFileActionKind::Close:
        if (action.descriptor >= 0 && action.descriptor < 3) ReplaceChildStream(streams, action.descriptor, INVALID_HANDLE_VALUE);
        return 0;
    case PosixSpawnFileActionKind::Duplicate:
        if (action.descriptor < 0 || action.newDescriptor < 0) return BadDescriptor;
        if (action.newDescriptor > 2)
            NotImplemented_nid_no_patch(("posix_spawn: a child descriptor " + std::to_string(action.newDescriptor) + " beyond the standard streams").c_str());
        {
            const HANDLE source = WindowsDescriptorHandle_nid_no_patch(action.descriptor);
            if (source == INVALID_HANDLE_VALUE) return BadDescriptor;
            ReplaceChildStream(streams, action.newDescriptor, source);
        }
        return 0;
    case PosixSpawnFileActionKind::Open: {
        if (action.descriptor < 0) return BadDescriptor;
        if (action.descriptor > 2)
            NotImplemented_nid_no_patch(("posix_spawn: a child descriptor " + std::to_string(action.descriptor) + " beyond the standard streams").c_str());
        std::filesystem::path native;
        if (const int error = ResolvePathError_nid_no_patch(action.path.c_str(), native)) return error;
        try {
            const int fd = ::_wopen(native.c_str(), HostOpenFlags(action.openFlags), action.mode);
            if (fd < 0) return errno;
            const HANDLE source = WindowsDescriptorHandle_nid_no_patch(fd);
            ReplaceChildStream(streams, action.descriptor, source);
            ::_close(fd);
        } catch (const std::invalid_argument&) {
            return InvalidArgument;
        }
        return 0;
    }
    }
    return 0;
}

std::vector<wchar_t> ChildEnvironment(char* const* environment) {
    std::vector<std::wstring> entries;
    const auto missesMarkers = [](const std::wstring& entry) {
        return entry.compare(0, std::size(LoadExecParentVariable) - 1, LoadExecParentVariable) != 0 &&
            entry.compare(0, std::size(LoadExecSkipVariable) - 1, LoadExecSkipVariable) != 0;
    };
    if (environment != nullptr)
        for (std::size_t index = 0; environment[index] != nullptr; ++index) {
            const std::wstring entry = Utf16(environment[index]);
            if (missesMarkers(entry))
                entries.push_back(entry);
        }
    else {
        const LPWCH block = GetEnvironmentStringsW();
        for (const wchar_t* text = block; *text != L'\0'; ) {
            std::size_t length = 0;
            while (text[length] != L'\0') ++length;
            if (length > 0) {
                const std::wstring entry(text, length);
                if (missesMarkers(entry))
                    entries.push_back(entry);
            }
            text += length + 1;
        }
        FreeEnvironmentStringsW(block);
    }
    entries.push_back(std::wstring(LoadExecSkipVariable) + L"1");
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
    std::vector<wchar_t> block;
    for (const auto& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(0);
    }
    block.push_back(0);
    return block;
}

int SpawnChild(const char* function, const std::string& guest, PosixSpawnFileActions* const* actions,
    PosixSpawnAttributes* const* attributes, std::int32_t* processId, char* const* arguments, char* const* environment) {
    const auto target = ConvertedExecutableTarget(function, guest);
    if (const std::uint32_t attributes_flags = SpawnAttributeFlags(attributes); attributes_flags != 0)
        NotImplemented_nid_no_patch(("posix_spawn: the attribute flags " + HexFlag(attributes_flags) + " on this host").c_str());
    ChildStreams streams;
    const DWORD identifiers[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    bool redirected = false;
    for (std::size_t index = 0; index < 3; ++index) {
        const HANDLE source = GetStdHandle(identifiers[index]);
        if (source == nullptr || source == INVALID_HANDLE_VALUE) continue;
        streams.handles[index] = DuplicateInheritable(source);
        redirected = true;
    }
    if (actions != nullptr) {
        for (const auto& action : (*actions)->actions) {
            if (action.kind != PosixSpawnFileActionKind::Close || (action.descriptor >= 0 && action.descriptor < 3)) redirected = true;
            const int error = ApplyChildRedirection(streams, action);
            if (error != 0) {
                CloseRedirectedStreams(streams);
                return error;
            }
        }
    }
    std::wstring command = L"\"" + target.wstring() + L"\"";
    for (std::size_t index = 0; arguments[index] != nullptr; ++index)
        command += L" " + QuotedArgument(Utf16(arguments[index]));
    if (command.size() >= 32767) throw std::runtime_error("posix_spawn: command line is too long");
    auto environmentBlock = ChildEnvironment(environment);
    PROCESS_INFORMATION process{};
    const BOOL created = [&] {
        if (!redirected || (streams.handles[0] == INVALID_HANDLE_VALUE && streams.handles[1] == INVALID_HANDLE_VALUE && streams.handles[2] == INVALID_HANDLE_VALUE)) {
            STARTUPINFOW plain{};
            plain.cb = sizeof(plain);
            return CreateProcessW(const_cast<wchar_t*>(target.c_str()), command.data(), nullptr, nullptr, FALSE,
                environmentBlock.empty() ? 0u : CREATE_UNICODE_ENVIRONMENT, environmentBlock.empty() ? nullptr : environmentBlock.data(),
                nullptr, &plain, &process);
        }
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = streams.handles[0];
        startup.StartupInfo.hStdOutput = streams.handles[1];
        startup.StartupInfo.hStdError = streams.handles[2];
        std::vector<HANDLE> inherited;
        for (HANDLE handle : streams.handles)
            if (handle != INVALID_HANDLE_VALUE) inherited.push_back(handle);
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes) ||
            !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(), inherited.size() * sizeof(HANDLE), nullptr, nullptr))
            throw std::system_error(GetLastError(), std::system_category(), "posix_spawn: selecting inherited handles");
        const std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, void (*)(_PROC_THREAD_ATTRIBUTE_LIST*)> cleanup(list, DeleteProcThreadAttributeList);
        return CreateProcessW(const_cast<wchar_t*>(target.c_str()), command.data(), nullptr, nullptr, TRUE,
            EXTENDED_STARTUPINFO_PRESENT | (environmentBlock.empty() ? 0u : CREATE_UNICODE_ENVIRONMENT),
            environmentBlock.empty() ? nullptr : environmentBlock.data(), nullptr, &startup.StartupInfo, &process);
    }();
    if (!created) {
        const DWORD error = GetLastError();
        CloseRedirectedStreams(streams);
        throw std::system_error(error, std::system_category(), "posix_spawn: creating the converted executable");
    }
    CloseRedirectedStreams(streams);
    CloseHandle(process.hThread);
    RegisterChildProcess_nid_no_patch(process.dwProcessId, static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(process.hProcess)));
    *processId = static_cast<std::int32_t>(process.dwProcessId);
    return 0;
}
#else
int SpawnChild(const char* function, const std::string& guest, PosixSpawnFileActions* const* actions,
    PosixSpawnAttributes* const* attributes, std::int32_t* processId, char* const* arguments, char* const* environment) {
    const auto target = ConvertedExecutableTarget(function, guest);
    posix_spawnattr_t hostAttributes;
    posix_spawnattr_t* usedAttributes = nullptr;
    const std::uint32_t flags = SpawnAttributeFlags(attributes);
    if ((flags & ~SpawnFlagsImplemented) != 0)
        NotImplemented_nid_no_patch(("posix_spawn: the attribute flags " + HexFlag(flags)).c_str());
    if (flags != 0) {
        if (posix_spawnattr_init(&hostAttributes) != 0) return Fault;
        usedAttributes = &hostAttributes;
        if ((flags & SpawnFlagResetIds) != 0) posix_spawnattr_setflags(usedAttributes, POSIX_SPAWN_RESETIDS);
        if ((flags & SpawnFlagSetGroup) != 0) {
            posix_spawnattr_setflags(usedAttributes, POSIX_SPAWN_SETPGROUP);
            posix_spawnattr_setpgroup(usedAttributes, (*attributes)->processGroup);
        }
    }
    posix_spawn_file_actions_t hostActions;
    posix_spawn_file_actions_t* usedActions = nullptr;
    if (actions != nullptr && !(*actions)->actions.empty()) {
        if (posix_spawn_file_actions_init(&hostActions) != 0) {
            if (usedAttributes != nullptr) posix_spawnattr_destroy(usedAttributes);
            return Fault;
        }
        usedActions = &hostActions;
    }
    int result = 0;
    if (actions != nullptr && usedActions != nullptr)
        for (const auto& action : (*actions)->actions) {
            switch (action.kind) {
            case PosixSpawnFileActionKind::Close:
                result = posix_spawn_file_actions_addclose(usedActions, action.descriptor);
                break;
            case PosixSpawnFileActionKind::Duplicate:
                result = posix_spawn_file_actions_adddup2(usedActions, action.descriptor, action.newDescriptor);
                break;
            case PosixSpawnFileActionKind::Open: {
                std::filesystem::path native;
                if (const int error = ResolvePathError_nid_no_patch(action.path.c_str(), native)) result = error;
                else {
                    try {
                        result = posix_spawn_file_actions_addopen(usedActions, action.descriptor, native.c_str(), HostOpenFlags(action.openFlags), action.mode);
                    } catch (const std::invalid_argument&) {
                        result = InvalidArgument;
                    }
                }
                break;
            }
            }
            if (result != 0) break;
        }
    if (result == 0) {
        char* const* hostEnvironment = environment;
#if defined(__linux__)
        if (hostEnvironment == nullptr) hostEnvironment = environ;
#elif defined(__APPLE__)
        if (hostEnvironment == nullptr) hostEnvironment = *_NSGetEnviron();
#endif
        pid_t childPid = -1;
        result = posix_spawn(&childPid, target.c_str(), usedActions, usedAttributes, arguments, hostEnvironment);
        if (result == 0) *processId = static_cast<std::int32_t>(childPid);
    }
    if (usedActions != nullptr) posix_spawn_file_actions_destroy(usedActions);
    if (usedAttributes != nullptr) posix_spawnattr_destroy(usedAttributes);
    return result;
}
#endif

}

extern "C" {

int APS5_VABI posix_spawnattr_init_nid_postfix(PosixSpawnAttributes** attributes) {
    if (attributes == nullptr) throw std::invalid_argument("posix_spawnattr_init: null attributes");
    auto* created = new (std::nothrow) PosixSpawnAttributes{};
    if (created == nullptr) return OutOfMemory;
    *attributes = created;
    return 0;
}

int APS5_VABI posix_spawnattr_destroy_nid_postfix(PosixSpawnAttributes** attributes) {
    if (attributes == nullptr) throw std::invalid_argument("posix_spawnattr_destroy: null attributes");
    delete *attributes;
    return 0;
}

int APS5_VABI posix_spawnattr_setflags_nid_postfix(PosixSpawnAttributes** attributes, std::int16_t flags) {
    Attributes(attributes, "posix_spawnattr_setflags").flags = flags;
    return 0;
}

int APS5_VABI posix_spawnattr_setsigdefault_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalDefault) {
    auto& target = Attributes(attributes, "posix_spawnattr_setsigdefault");
    if (signalDefault == nullptr) throw std::invalid_argument("posix_spawnattr_setsigdefault: null signal set");
    target.signalDefault = *signalDefault;
    return 0;
}

int APS5_VABI posix_spawnattr_getflags_nid_postfix(PosixSpawnAttributes* const* attributes, std::int16_t* flags) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getflags");
    if (flags == nullptr) throw std::invalid_argument("posix_spawnattr_getflags: null flags");
    *flags = source.flags;
    return 0;
}

int APS5_VABI posix_spawnattr_getpgroup_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* processGroup) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getpgroup");
    if (processGroup == nullptr) throw std::invalid_argument("posix_spawnattr_getpgroup: null process group");
    *processGroup = source.processGroup;
    return 0;
}

int APS5_VABI posix_spawnattr_getschedparam_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingParameter) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getschedparam");
    if (schedulingParameter == nullptr) throw std::invalid_argument("posix_spawnattr_getschedparam: null scheduling parameter");
    *schedulingParameter = source.schedulingPriority;
    return 0;
}

int APS5_VABI posix_spawnattr_getschedpolicy_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingPolicy) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getschedpolicy");
    if (schedulingPolicy == nullptr) throw std::invalid_argument("posix_spawnattr_getschedpolicy: null scheduling policy");
    *schedulingPolicy = source.schedulingPolicy;
    return 0;
}

int APS5_VABI posix_spawnattr_getsigdefault_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalDefault) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getsigdefault");
    if (signalDefault == nullptr) throw std::invalid_argument("posix_spawnattr_getsigdefault: null signal set");
    *signalDefault = source.signalDefault;
    return 0;
}

int APS5_VABI posix_spawnattr_getsigmask_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalMask) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getsigmask");
    if (signalMask == nullptr) throw std::invalid_argument("posix_spawnattr_getsigmask: null signal set");
    *signalMask = source.signalMask;
    return 0;
}

int APS5_VABI posix_spawnattr_setpgroup_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t processGroup) {
    Attributes(attributes, "posix_spawnattr_setpgroup").processGroup = processGroup;
    return 0;
}

int APS5_VABI posix_spawnattr_setschedparam_nid_postfix(PosixSpawnAttributes** attributes, const std::int32_t* schedulingParameter) {
    auto& target = Attributes(attributes, "posix_spawnattr_setschedparam");
    if (schedulingParameter == nullptr) throw std::invalid_argument("posix_spawnattr_setschedparam: null scheduling parameter");
    target.schedulingPriority = *schedulingParameter;
    return 0;
}

int APS5_VABI posix_spawnattr_setschedpolicy_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t schedulingPolicy) {
    Attributes(attributes, "posix_spawnattr_setschedpolicy").schedulingPolicy = schedulingPolicy;
    return 0;
}

int APS5_VABI posix_spawnattr_setsigmask_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalMask) {
    auto& target = Attributes(attributes, "posix_spawnattr_setsigmask");
    if (signalMask == nullptr) throw std::invalid_argument("posix_spawnattr_setsigmask: null signal set");
    target.signalMask = *signalMask;
    return 0;
}

int APS5_VABI posix_spawn_file_actions_init_nid_postfix(PosixSpawnFileActions** actions) {
    if (actions == nullptr) throw std::invalid_argument("posix_spawn_file_actions_init: null file actions");
    auto* created = new (std::nothrow) PosixSpawnFileActions{};
    if (created == nullptr) return -1;
    *actions = created;
    return 0;
}

int APS5_VABI posix_spawn_file_actions_destroy_nid_postfix(PosixSpawnFileActions** actions) {
    delete &FileActions(actions, "posix_spawn_file_actions_destroy");
    return 0;
}

int APS5_VABI posix_spawn_file_actions_addopen_nid_postfix(PosixSpawnFileActions** actions, int descriptor, const char* path, int openFlags, std::uint16_t mode) {
    if (descriptor < 0) return BadDescriptor;
    if (path == nullptr) throw std::invalid_argument("posix_spawn_file_actions_addopen: null path");
    auto& target = FileActions(actions, "posix_spawn_file_actions_addopen");
    try {
        return Append(target, {PosixSpawnFileActionKind::Open, descriptor, path, openFlags, mode, 0});
    } catch (const std::bad_alloc&) {
        return OutOfMemory;
    }
}

int APS5_VABI posix_spawn_file_actions_adddup2_nid_postfix(PosixSpawnFileActions** actions, int descriptor, int newDescriptor) {
    if (descriptor < 0 || newDescriptor < 0) return BadDescriptor;
    return Append(FileActions(actions, "posix_spawn_file_actions_adddup2"), {PosixSpawnFileActionKind::Duplicate, descriptor, {}, 0, 0, newDescriptor});
}

int APS5_VABI posix_spawn_file_actions_addclose_nid_postfix(PosixSpawnFileActions** actions, int descriptor) {
    if (descriptor < 0) return BadDescriptor;
    return Append(FileActions(actions, "posix_spawn_file_actions_addclose"), {PosixSpawnFileActionKind::Close, descriptor, {}, 0, 0, 0});
}

int SpawnProgram(const char* function, const char* file, bool searchesPath, PosixSpawnFileActions* const* actions,
    PosixSpawnAttributes* const* attributes, std::int32_t* processId, char* const* arguments, char* const* environment) {
    if (file == nullptr) throw std::invalid_argument(std::string(function) + ": null path");
    if (actions != nullptr) FileActions(actions, function);
    if (attributes != nullptr) Attributes(attributes, function);
    std::string found;
    if (!ProgramFound(file, searchesPath, found)) return NoSuchFile;
    if (arguments == nullptr) return Fault;
    return SpawnChild(function, found, actions, attributes, processId, arguments, environment);
}

int APS5_VABI posix_spawn_nid_postfix(std::int32_t* processId, const char* path, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment) {
    return SpawnProgram("posix_spawn", path, false, actions, attributes, processId, arguments, environment);
}

int APS5_VABI posix_spawnp_nid_postfix(std::int32_t* processId, const char* file, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment) {
    return SpawnProgram("posix_spawnp", file, true, actions, attributes, processId, arguments, environment);
}

}
