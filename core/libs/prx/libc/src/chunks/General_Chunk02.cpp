#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/RunReport.hpp"
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>
#include <limits>
#include <optional>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#else
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/uio.h>
#include "prx/libc/include/specifics/linux/GuestFaultCopy.hpp"
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#include <unistd.h>
#ifdef __linux__
#include <sys/uio.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#endif
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"

namespace {
// Guest prefixes (without leading slashes) mapped to host directories, e.g. save-data mount points:
// the PS5 hands the title a short mount point ("/savedata0") whose files live under _sd/<dir name>.
struct PathAliases {
    std::mutex mutex;
    std::vector<std::pair<std::string, std::string>> entries;
    std::vector<std::string> blocked;
    std::vector<std::pair<std::string, PathProvider>> providers;
    std::atomic<std::uint64_t> generation{0};
};

PathAliases& Aliases() {
    static auto* aliases = new PathAliases;
    return *aliases;
}

std::size_t HostPageSize() {
    static const std::size_t size = [] {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return static_cast<std::size_t>(info.dwPageSize);
#else
        const long value = sysconf(_SC_PAGESIZE);
        if (value <= 0) throw std::runtime_error("cannot query the host page size");
        return static_cast<std::size_t>(value);
#endif
    }();
    return size;
}

struct RestoreErrno {
    int value = errno;
    ~RestoreErrno() { errno = value; }
};

bool CopyGuestBytes(void* destination, const void* source, std::size_t bytes, bool prepare = true) {
    const RestoreErrno restore;
    if (prepare) GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
#ifdef _WIN32
    if (GuestArena::GuestArenaCopyOwned_nid_no_patch(reinterpret_cast<std::uintptr_t>(source), destination, bytes)) return true;
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), source, destination, bytes, &copied) != 0 && copied == bytes;
#elif defined(__linux__)
#if defined(__x86_64__)
    if (GuestFaultCopy::Ready()) return GuestFaultCopy::Copy(destination, source, bytes);
#endif
    iovec local{destination, bytes};
    iovec remote{const_cast<void*>(source), bytes};
    const int saved = errno;
    const auto copied = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("guest path: process_vm_readv failed with errno " + std::to_string(errno));
    errno = saved;
    return copied >= 0 && static_cast<std::size_t>(copied) == bytes;
#elif defined(__APPLE__)
    mach_vm_size_t copied = 0;
    return mach_vm_read_overwrite(mach_task_self(), reinterpret_cast<mach_vm_address_t>(source), bytes,
               reinterpret_cast<mach_vm_address_t>(destination), &copied) == KERN_SUCCESS && copied == bytes;
#else
    throw std::runtime_error("checked guest path copies are not implemented on this host");
#endif
}

std::string TrimSlashes(const char* path) {
    std::string s(path);
    std::size_t start = 0;
    while (start < s.size() && (s[start] == '/' || s[start] == '\\')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == '/' || s[end - 1] == '\\')) {
        --end;
    }
    return s.substr(start, end - start);
}

bool SameName(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    const auto fold = [](unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
    for (std::size_t i = 0; i < left.size(); ++i) if (fold(left[i]) != fold(right[i])) return false;
    return true;
}

bool HasPrefix(const std::string& path, const std::string& prefix) {
    return path.size() >= prefix.size() && SameName(std::string_view(path).substr(0, prefix.size()), prefix) &&
           (path.size() == prefix.size() || path[prefix.size()] == '/');
}

std::filesystem::path RelinkedModuleName(std::filesystem::path path) {
    path += ".guest.prx";
    return path;
}

struct NameMatch {
    std::string name;
    std::optional<std::filesystem::path> path;
    bool ambiguous = false;
    bool stem = false;
};

std::string_view NameStem(std::string_view name) {
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos || dot == 0 ? name : name.substr(0, dot);
}

bool SameRelinkedStem(std::string_view filename, std::string_view name) {
    constexpr std::string_view suffix = ".guest.prx";
    if (filename.size() <= suffix.size() || !SameName(filename.substr(filename.size() - suffix.size()), suffix)) return false;
    return SameName(NameStem(filename.substr(0, filename.size() - suffix.size())), NameStem(name));
}

#ifdef _WIN32
bool CaseInsensitiveDirectoryWindows(const std::filesystem::path& directory) {
    const auto handle = CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const std::unique_ptr<void, decltype(&CloseHandle)> owner(handle, &CloseHandle);
    struct { ULONG flags; } information{};
    constexpr auto caseSensitiveInfo = static_cast<FILE_INFO_BY_HANDLE_CLASS>(23);
    return GetFileInformationByHandleEx(handle, caseSensitiveInfo, &information, sizeof(information)) &&
        (information.flags & FILE_CS_FLAG_CASE_SENSITIVE_DIR) == 0;
}

std::error_code MatchNamesWindows(const std::filesystem::path& directory, std::span<NameMatch> names) {
    for (auto& name : names) {
        const auto pattern = directory / name.name;
        WIN32_FIND_DATAW data{};
        const auto handle = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
        if (handle == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) continue;
            return {static_cast<int>(error), std::system_category()};
        }
        const std::unique_ptr<void, decltype(&FindClose)> owner(handle, &FindClose);
        do {
            const auto filename = std::filesystem::path(data.cFileName);
            if (!SameName(filename.string(), name.name)) continue;
            name.ambiguous = name.ambiguous || name.path.has_value();
            name.path = directory / filename;
        } while (FindNextFileW(handle, &data));
        const auto error = GetLastError();
        if (error != ERROR_NO_MORE_FILES) return {static_cast<int>(error), std::system_category()};
    }
    return {};
}
#endif

std::error_code MatchNames(const std::filesystem::path& directory, std::span<NameMatch> names) {
#ifdef _WIN32
    bool stem = false;
    for (const auto& name : names) stem = stem || name.stem;
    if (!stem && CaseInsensitiveDirectoryWindows(directory)) return MatchNamesWindows(directory, names);
#endif
    std::error_code error;
    std::filesystem::directory_iterator entry(directory, error), end;
    for (; !error && entry != end; entry.increment(error)) {
        const auto filename = entry->path().filename().string();
        for (auto& name : names) {
            if (name.stem) {
                std::error_code status;
                if (!SameRelinkedStem(filename, name.name) || !entry->is_regular_file(status) || status) continue;
                if (!name.path || filename.size() < name.path->filename().string().size()) name.path = entry->path();
                continue;
            }
            if (!SameName(filename, name.name)) continue;
            name.ambiguous = name.ambiguous || name.path.has_value();
            name.path = entry->path();
        }
    }
    return error;
}

std::filesystem::path MatchedModuleOrPath(const std::filesystem::path& path, const std::filesystem::path& relinked, const NameMatch& match) {
    if (match.ambiguous) throw std::runtime_error("Ambiguous case-insensitive guest path: " + relinked.string());
    std::error_code error;
    return match.path && std::filesystem::is_regular_file(*match.path, error) ? *match.path : path;
}

std::filesystem::path PathOrRelinkedModule(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::exists(std::filesystem::symlink_status(path, error))) return path;
    return RelinkedModulePath_nid_no_patch(path);
}

std::vector<std::string> SplitNames(std::string_view text) {
    std::vector<std::string> names;
    while (!text.empty()) {
        const auto separator = text.find('/');
        names.emplace_back(text.substr(0, separator));
        if (separator == std::string_view::npos) break;
        text.remove_prefix(separator + 1);
        if (text.empty()) names.emplace_back();
    }
    return names;
}

std::string JoinNames(const std::vector<std::string>& names) {
    std::string joined;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) joined += '/';
        joined += names[i];
    }
    return joined;
}

std::filesystem::path Child(std::filesystem::path parent, const std::string& name) {
    parent /= "";
    parent += name;
    return parent;
}

std::filesystem::path ResolveHostPath(std::filesystem::path root, const std::vector<std::string>& relative, bool module = false) {
    const auto direct = Child(root, JoinNames(relative));
    std::error_code error;
    auto first = relative.begin();
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(direct.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return direct;
    const auto failure = GetLastError();
    const bool missing = attributes == INVALID_FILE_ATTRIBUTES && (failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND);
    if (!missing && std::filesystem::exists(direct, error)) return direct;
    for (auto last = relative.end(); last != relative.begin();) {
        const auto parent = Child(root, JoinNames(std::vector<std::string>(relative.begin(), std::prev(last))));
        const auto parentAttributes = GetFileAttributesW(parent.c_str());
        const bool present = parentAttributes != INVALID_FILE_ATTRIBUTES && (parentAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
        if (!present && !std::filesystem::exists(parent, error)) { --last; continue; }
        root = parent;
        first = std::prev(last);
        break;
    }
#else
    if (std::filesystem::exists(direct, error)) return direct;
#endif
    for (auto part = first; part != relative.end(); ++part) {
        const auto candidate = Child(root, *part);
        const auto status = std::filesystem::symlink_status(candidate, error);
        if (std::filesystem::exists(status) || (error && error != std::errc::no_such_file_or_directory)) {
            root = candidate;
            continue;
        }
        const bool last = module && std::next(part) == relative.end();
        const auto relinked = RelinkedModuleName(candidate);
        NameMatch names[] = {{*part}, {relinked.filename().string()}};
        error = MatchNames(root, std::span(names, last ? 2 : 1));
        if (names[0].ambiguous) throw std::runtime_error("Ambiguous case-insensitive guest path: " + candidate.string());
        const bool matched = !error && names[0].path;
        root = matched ? *names[0].path : candidate;
        if (!last || matched) continue;
        if (std::filesystem::is_regular_file(relinked, error)) return relinked;
        return MatchedModuleOrPath(candidate, relinked, names[1]);
    }
    return module ? PathOrRelinkedModule(root) : root;
}

std::optional<std::filesystem::path> ResolveAlias(const std::string& guestPath, bool module) {
    const auto relative = TrimSlashes(guestPath.c_str());
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    for (const auto& prefix : aliases.blocked) {
        if (HasPrefix(relative, prefix))
            throw std::filesystem::filesystem_error("Guest mount is unavailable", guestPath, std::make_error_code(std::errc::no_such_file_or_directory));
    }
    for (const auto& [prefix, host] : aliases.entries) {
        if (!HasPrefix(relative, prefix)) continue;
        if (relative.size() == prefix.size()) {
            const auto result = std::filesystem::path(host).make_preferred();
            return module ? PathOrRelinkedModule(result) : result;
        }
        if (guestPath.back() == '/') {
            auto result = ResolveHostPath(host, SplitNames(std::string_view(relative).substr(prefix.size() + 1))).make_preferred();
            result /= "";
            return module ? PathOrRelinkedModule(result) : result;
        }
        return ResolveHostPath(host, SplitNames(std::string_view(relative).substr(prefix.size() + 1)), module).make_preferred();
    }
    return std::nullopt;
}

struct WorkingDirectory {
    std::mutex mutex;
    const std::filesystem::path root = std::filesystem::canonical(std::filesystem::current_path());
    std::filesystem::path current = root;
};
WorkingDirectory& Directories() {
    static auto* state = new WorkingDirectory;
    return *state;
}
std::optional<std::filesystem::path> ProvidedPath(const WorkingDirectory& state, const std::vector<std::string>& names) {
    const auto relative = TrimSlashes(JoinNames(names).c_str());
    PathProvider provide = nullptr;
    {
        auto& aliases = Aliases();
        std::lock_guard lock(aliases.mutex);
        for (const auto& [path, provider] : aliases.providers) {
            if (SameName(relative, path)) provide = provider;
        }
    }
    if (provide == nullptr) return std::nullopt;
    const auto supplied = ResolveHostPath(state.root, names);
    std::error_code error;
    if (std::filesystem::exists(supplied, error) || error) return supplied;
    auto provided = provide(state.root);
    if (provided.empty()) return std::nullopt;
    return provided.make_preferred();
}
std::filesystem::path HostPath(WorkingDirectory& state, const std::vector<std::string>& names, bool module = false) {
    if (auto aliased = ResolveAlias(JoinNames(names), module)) return *aliased;
    if (auto provided = ProvidedPath(state, names)) return *provided;
    return ResolveHostPath(state.root, names, module).make_preferred();
}
void RequireHostDirectory(WorkingDirectory& state, const std::vector<std::string>& names) {
    const auto host = HostPath(state, names);
    std::error_code error;
    const auto status = std::filesystem::status(host, error);
    if (std::filesystem::is_directory(status)) return;
    if (!error) error = std::make_error_code(std::filesystem::exists(status) ? std::errc::not_a_directory : std::errc::no_such_file_or_directory);
#ifdef _WIN32
    if (error == std::errc::no_such_file_or_directory) {
        for (auto prefix = host.parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
            std::error_code prefixError;
            const auto prefixStatus = std::filesystem::status(prefix, prefixError);
            if (!std::filesystem::exists(prefixStatus)) continue;
            if (!std::filesystem::is_directory(prefixStatus)) error = std::make_error_code(std::errc::not_a_directory);
            break;
        }
    }
#endif
    throw std::filesystem::filesystem_error("Guest path component is not a directory", host, error);
}
void RejectNonDirectory(const std::filesystem::path& host) {
    auto target = host;
    while (!target.has_filename() && target.has_relative_path()) target = target.parent_path();
    std::error_code error;
    const auto status = std::filesystem::status(target, error);
    if (std::filesystem::exists(status) && !std::filesystem::is_directory(status))
        throw std::filesystem::filesystem_error("Guest path with a trailing slash is not a directory", host, std::make_error_code(std::errc::not_a_directory));
}
std::vector<std::string> WithTrailingSeparator(std::vector<std::string> names, bool trailing) {
    if (trailing && !names.empty()) names.emplace_back();
    return names;
}
std::filesystem::path Resolve(WorkingDirectory& state, const char* path, bool module = false, bool archived = false) {
    std::string text(path);
    constexpr std::size_t MaxPath = 1024;
    constexpr std::size_t MaxName = 255;
    if (text.size() >= MaxPath) throw std::filesystem::filesystem_error("Guest path is too long", std::make_error_code(std::errc::filename_too_long));
    std::size_t name = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        name = text[index] == '/' || text[index] == '\\' ? 0 : name + 1;
        if (name <= MaxName) continue;
        const std::size_t end = index + 1 - name;
        std::error_code status;
        if (end == 0 || std::filesystem::is_directory(Resolve(state, text.substr(0, end).c_str()), status)) {
            throw std::filesystem::filesystem_error("Guest path name is too long", std::make_error_code(std::errc::filename_too_long));
        }
        break;
    }
    for (auto& character : text) if (character == '\\') character = '/';
#ifdef _WIN32
    // Preserve the existing ability to pass explicit native drive paths.
    if (std::filesystem::path input(text); input.has_root_name()) return module ? PathOrRelinkedModule(input) : input;
#endif
    if (text.empty() || text.front() != '/') text = state.current.lexically_relative(state.root).generic_string() + "/" + text;
    std::vector<std::string> names;
    bool trailing = true;
    bool endsWithDot = false;
    const auto parts = SplitNames(text);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto& name = parts[i];
        if (name.empty() && i + 1 != parts.size()) continue;
        if (!name.empty()) endsWithDot = name == ".";
        if (name == ".." && !names.empty()) {
            if (!(archived && AppArchiveHasDirectory_nid_no_patch(HostPath(state, names)))) RequireHostDirectory(state, WithTrailingSeparator(names, trailing));
            names.pop_back();
        }
        trailing = name.empty() || name == "." || name == "..";
        if (!trailing) names.push_back(name);
    }
    if (endsWithDot && !names.empty() && !(archived && AppArchiveHasDirectory_nid_no_patch(HostPath(state, names)))) RequireHostDirectory(state, names);
    const bool directory = trailing && !names.empty();
    auto resolved = HostPath(state, WithTrailingSeparator(std::move(names), trailing), module);
    if (directory) RejectNonDirectory(resolved);
    return resolved;
}
#ifdef _WIN32
using WriteMark = FILETIME;
WriteMark Now() {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return now;
}
bool NotBefore(const WriteMark& time, const WriteMark& mark) {
    return CompareFileTime(&time, &mark) >= 0;
}
#else
using WriteMark = timespec;
WriteMark Now() {
    timespec now;
#ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &now);
#else
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
#endif
    return now;
}
bool NotBefore(const WriteMark& time, const WriteMark& mark) {
    if (time.tv_sec == mark.tv_sec) return time.tv_nsec >= mark.tv_nsec;
    return time.tv_sec >= mark.tv_sec;
}
#endif
struct WrittenPathRegistry {
    std::mutex syncMutex;
    std::mutex mutex;
    std::map<std::filesystem::path, bool> dirty;
    WriteMark mark = Now();
};
WrittenPathRegistry& Written() { static WrittenPathRegistry registry; return registry; }
#ifdef _WIN32
std::optional<WriteMark> WriteTime(const std::filesystem::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return data.ftLastWriteTime;
    const auto error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return std::nullopt;
    throw std::system_error(static_cast<int>(error), std::system_category(), "Reading the write time of " + path.string());
}
void FlushPath(const std::filesystem::path& path) {
    const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return;
        if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) return;
        throw std::system_error(static_cast<int>(error), std::system_category(), "Opening " + path.string() + " for sync");
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> owner(handle, &CloseHandle);
    if (!FlushFileBuffers(handle))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Flushing " + path.string());
}
#else
std::optional<WriteMark> WriteTime(const std::filesystem::path& path) {
    struct stat data;
#ifdef __APPLE__
    if (::stat(path.c_str(), &data) == 0) return data.st_mtimespec;
#else
    if (::stat(path.c_str(), &data) == 0) return data.st_mtim;
#endif
    if (errno == ENOENT || errno == ENOTDIR) return std::nullopt;
    throw std::system_error(errno, std::generic_category(), "Reading the write time of " + path.string());
}
void FlushPath(const std::filesystem::path& path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) {
        if (errno == ENOENT || errno == ENOTDIR || errno == EACCES) return;
        throw std::system_error(errno, std::generic_category(), "Opening " + path.string() + " for sync");
    }
    const int result = ::fsync(descriptor);
    const int error = errno;
    ::close(descriptor);
    if (result != 0 && error != EINVAL) throw std::system_error(error, std::generic_category(), "Flushing " + path.string());
}
#endif
bool ChangedSince(const std::filesystem::path& path, const WriteMark& mark) {
    const auto time = WriteTime(path);
    return time && NotBefore(*time, mark);
}
int DirectoryFailure(const std::error_code& error) {
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    return 5;
}
bool IsDirectory(const std::filesystem::path& path, std::error_code& error) {
#ifdef _WIN32
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    if (error) return false;
    return std::filesystem::is_directory(verbatim, error);
#else
    return std::filesystem::is_directory(path, error);
#endif
}

#ifdef _WIN32
std::optional<std::filesystem::path> FinalDirectoryPath(const std::filesystem::path& path, bool& notDirectory) {
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    if (error) return std::nullopt;
    const HANDLE handle = ::CreateFileW(verbatim.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
    FILE_BASIC_INFO basic{};
    std::wstring final(MAX_PATH, L'\0');
    DWORD length = 0;
    if (::GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic))) {
        notDirectory = (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        if (!notDirectory) {
            length = ::GetFinalPathNameByHandleW(handle, final.data(), static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED);
            if (length >= final.size()) {
                final.resize(length);
                length = ::GetFinalPathNameByHandleW(handle, final.data(), length, FILE_NAME_NORMALIZED);
            }
        }
    }
    ::CloseHandle(handle);
    if (length == 0 || length >= final.size()) return std::nullopt;
    final.resize(length);
    if (final.starts_with(LR"(\\?\UNC\)")) final.replace(0, 8, LR"(\\)");
    else if (final.starts_with(LR"(\\?\)")) final.erase(0, 4);
    return std::filesystem::path(std::move(final));
}

int SetLongWorkingDirectory(WorkingDirectory& state, const std::filesystem::path& resolved, const std::error_code& failure) {
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(resolved, error);
    if (error || !verbatim.native().starts_with(LR"(\\?\)")) return DirectoryFailure(failure);
    bool notDirectory = false;
    const auto target = FinalDirectoryPath(resolved, notDirectory);
    if (notDirectory) return 20;
    if (!target) return DirectoryFailure(failure);
    const auto root = FinalDirectoryPath(state.root, notDirectory);
    if (!root) return DirectoryFailure(failure);
    const auto relative = target->lexically_relative(*root);
    if (relative.empty() || *relative.begin() == "..") return 45;
    state.current = relative == "." ? state.root : state.root / relative;
    return 0;
}
#endif

int SetWorkingDirectory(WorkingDirectory& state, const std::filesystem::path& resolved) {
    std::error_code error;
    const auto canonical = std::filesystem::canonical(resolved, error);
#ifdef _WIN32
    if (error) return SetLongWorkingDirectory(state, resolved, error);
#else
    if (error) return DirectoryFailure(error);
#endif
    if (!std::filesystem::is_directory(canonical, error)) return error ? DirectoryFailure(error) : 20;
    auto relative = canonical.lexically_relative(state.root);
#ifdef _WIN32
    if (relative.empty() || *relative.begin() == "..") {
        bool notDirectory = false;
        const auto target = FinalDirectoryPath(canonical, notDirectory);
        const auto root = FinalDirectoryPath(state.root, notDirectory);
        if (target && root) relative = target->lexically_relative(*root);
    }
    if (relative.empty() || *relative.begin() == "..") {
        auto ancestor = canonical;
        while (!std::filesystem::equivalent(ancestor, state.root, error)) {
            if (error) return DirectoryFailure(error);
            const auto parent = ancestor.parent_path();
            if (parent == ancestor) return 45;
            ancestor = parent;
        }
        relative = canonical.lexically_relative(ancestor);
    }
#endif
    if (relative.empty() || *relative.begin() == "..") return 45;
#ifdef _WIN32
    state.current = relative == "." ? state.root : state.root / relative;
#else
    state.current = canonical;
#endif
    return 0;
}
}

extern "C" void GuestFaultCopyReinstall_nid_no_patch() {
#if defined(__linux__) && defined(__x86_64__)
    GuestFaultCopy::Reinstall();
#endif
}

extern "C" bool GuestCopyInUnsynced_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(source);
    if (start == 0 || bytes > UINTPTR_MAX - start) return false;
    return CopyGuestBytes(destination, source, bytes, false);
}

extern "C" bool GuestCopyIn_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(source);
    if (start == 0 || bytes > UINTPTR_MAX - start) return false;
    return CopyGuestBytes(destination, source, bytes);
}

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path) {
    auto resolved = ResolveLocked(path, false, true);
    AppArchiveRefuseOnly_nid_no_patch(resolved, path, "reading");
    return resolved;
}

extern "C" std::filesystem::path ResolvePathOrRelinkedModule_nid_no_patch(const char* path) {
    auto resolved = ResolveLocked(path, true, true);
    AppArchiveRefuseOnly_nid_no_patch(resolved, path, "reading");
    return resolved;
}

extern "C" std::filesystem::path RelinkedModulePath_nid_no_patch(const std::filesystem::path& path) {
    const auto relinked = RelinkedModuleName(path);
    std::error_code error;
    if (std::filesystem::is_regular_file(relinked, error)) return relinked;
    const auto parent = relinked.parent_path().empty() ? std::filesystem::path(".") : relinked.parent_path();
    NameMatch names[] = {{relinked.filename().string()}, {path.filename().string(), std::nullopt, false, true}};
    MatchNames(parent, names);
    if (names[0].ambiguous || names[0].path) return MatchedModuleOrPath(path, relinked, names[0]);
    for (const char* directory : {"prx", "sce_module", "sce_modules"}) {
        NameMatch nested[] = {{relinked.filename().string()}};
        MatchNames(parent / directory, nested);
        if (nested[0].ambiguous || nested[0].path) return MatchedModuleOrPath(path, relinked, nested[0]);
    }
    return names[1].path ? *names[1].path : path;
}

extern "C" int SetWorkingDirectory_nid_no_patch(const char* resolved) {
    if (resolved == nullptr) return 14;
    auto& state = Directories();
    std::lock_guard lock(state.mutex);
    try {
        return SetWorkingDirectory(state, std::filesystem::path(reinterpret_cast<const char8_t*>(resolved)));
    } catch (const std::bad_alloc&) {
        return 12;
    } catch (const std::filesystem::filesystem_error& error) {
        return DirectoryFailure(error.code());
    }
}

extern "C" bool FinalPathComponentIsDot_nid_no_patch(const char* path) {
    std::string_view text(path);
    while (!text.empty() && (text.back() == '/' || text.back() == '\\')) text.remove_suffix(1);
    const auto separator = text.find_last_of("/\\");
    const auto component = text.substr(separator == std::string_view::npos ? 0 : separator + 1);
    return component == ".";
}

extern "C" int APS5_VABI chdir_nid_postfix(const char* path) {
    if (!path) { errno = 14; return -1; }
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) { errno = error; return -1; }
    if (copy.empty()) { errno = 2; return -1; }
    try {
        AppArchiveReady_nid_no_patch();
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        const auto resolved = Resolve(state, copy.c_str(), false, true);
        AppArchiveRefuseOnly_nid_no_patch(resolved, copy.c_str(), "changing the working directory to");
        const int error = SetWorkingDirectory(state, resolved);
        if (error) { errno = error == 2 ? NotFoundError_nid_no_patch(resolved) : error; return -1; }
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return -1; }
}

extern "C" char* APS5_VABI getcwd_nid_postfix(char* buffer, std::size_t size) {
    if (buffer && size == 0) { errno = 22; return nullptr; }
    if (buffer && size == 1) { errno = 34; return nullptr; }
    try {
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        std::error_code error;
        if (!IsDirectory(state.current, error)) {
            errno = error ? DirectoryFailure(error) : 2; return nullptr;
        }
        const auto relative = state.current.lexically_relative(state.root);
        const auto path = relative == "." ? std::string("/") : "/" + relative.generic_string();
        const auto required = path.size() + 1;
        if (buffer && size < required) { errno = 34; return nullptr; }
        if (!buffer) buffer = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(required));
        if (!buffer) { errno = 12; return nullptr; }
        std::memcpy(buffer, path.c_str(), required);
        return buffer;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return nullptr; }
}

#ifdef _WIN32
extern "C" int WindowsOpenError_nid_no_patch(unsigned long error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_WRITE_PROTECT:
    case ERROR_NETWORK_ACCESS_DENIED:
    case ERROR_CANNOT_MAKE:
    case ERROR_DRIVE_LOCKED:
        return EACCES;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_TOO_MANY_OPEN_FILES:
        return EMFILE;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_DISK_FULL:
        return ENOSPC;
    case ERROR_CANT_RESOLVE_FILENAME:
        return ELOOP;
    default:
        return EINVAL;
    }
}

extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static DWORD StandardHandleId(int descriptor) {
    return descriptor == 0 ? STD_INPUT_HANDLE : descriptor == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
}
extern "C" void* HostStandardHandle_nid_no_patch(int descriptor) {
    if (descriptor < 0 || descriptor > 2) return nullptr;
    const HANDLE current = GetStdHandle(StandardHandleId(descriptor));
    if (current == nullptr || current == INVALID_HANDLE_VALUE) return nullptr;
    const auto previous = _set_thread_local_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {});
    const auto owned = ::_get_osfhandle(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    if (reinterpret_cast<HANDLE>(owned) != current) return current;
    HANDLE kept = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), current, GetCurrentProcess(), &kept, 0, TRUE, DUPLICATE_SAME_ACCESS))
        throw std::runtime_error("Cannot keep the host standard handle of descriptor " + std::to_string(descriptor));
    return kept;
}
extern "C" void KeepStandardHandle_nid_no_patch(int descriptor, void* handle) {
    if (handle != nullptr && !SetStdHandle(StandardHandleId(descriptor), handle))
        throw std::runtime_error("Cannot keep the host standard handle of descriptor " + std::to_string(descriptor));
}
extern "C" int DuplicateHostDescriptor_nid_no_patch(int descriptor) {
    const auto previous = _set_thread_local_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {});
    const int kept = ::_dup(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    return kept;
}
#endif

namespace {

std::atomic<std::uint64_t> notImplementedCalls{0};

}

extern "C" [[noreturn]] void (NotImplemented_nid_no_patch)(const char* funcName) {
    NotImplementedAt_nid_no_patch(funcName, "", 0);
}

extern "C" [[noreturn]] void NotImplementedAt_nid_no_patch(const char* funcName, const char* file, int line) {
    notImplementedCalls.fetch_add(1, std::memory_order_relaxed);
    LibcRunReportMissingFunction_nid_no_patch(funcName, file, line);
    throw std::runtime_error(std::string(funcName) + " not implemented");
}

extern "C" std::uint64_t NotImplementedCalls_nid_no_patch() {
    return notImplementedCalls.load(std::memory_order_relaxed);
}
