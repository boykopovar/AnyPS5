#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <optional>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"

namespace {
// Guest prefixes (without leading slashes) mapped to host directories, e.g. save-data mount points:
// the PS5 hands the title a short mount point ("/savedata0") whose files live under _sd/<dir name>.
struct PathAliases {
    std::mutex mutex;
    std::vector<std::pair<std::string, std::string>> entries;
    std::vector<std::string> blocked;
};

PathAliases& Aliases() {
    static PathAliases aliases;
    return aliases;
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

std::optional<std::filesystem::path> ResolveAlias(const std::string& guestPath) {
    const auto relative = TrimSlashes(guestPath.c_str());
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    for (const auto& prefix : aliases.blocked) {
        if (relative == prefix || (relative.size() > prefix.size() && relative.compare(0, prefix.size(), prefix) == 0 && relative[prefix.size()] == '/'))
            throw std::filesystem::filesystem_error("Guest mount is unavailable", guestPath, std::make_error_code(std::errc::no_such_file_or_directory));
    }
    for (const auto& [prefix, host] : aliases.entries) {
        if (relative.size() < prefix.size() || relative.compare(0, prefix.size(), prefix) != 0) continue;
        if (relative.size() == prefix.size()) return std::filesystem::path(host).make_preferred();
        if (relative[prefix.size()] != '/') continue;
        std::filesystem::path result = std::filesystem::path(host) / std::filesystem::path(relative.substr(prefix.size() + 1));
        return result.make_preferred();
    }
    return std::nullopt;
}

struct WorkingDirectory {
    std::mutex mutex;
    const std::filesystem::path root = std::filesystem::canonical(std::filesystem::current_path());
    std::filesystem::path current = root;
};
WorkingDirectory& Directories() { static WorkingDirectory state; return state; }
std::filesystem::path Resolve(WorkingDirectory& state, const char* path) {
    std::string text(path);
    for (auto& character : text) if (character == '\\') character = '/';
    std::filesystem::path input(text);
#ifdef _WIN32
    // Preserve the existing ability to pass explicit native drive paths.
    if (input.has_root_name()) return input;
#endif
    auto guest = (std::filesystem::path("/") / state.current.lexically_relative(state.root));
    guest = (input.is_absolute() ? input : guest / input).lexically_normal();
    if (auto aliased = ResolveAlias(guest.relative_path().generic_string())) return *aliased;
    return (state.root / guest.relative_path()).make_preferred();
}
struct WrittenPathRegistry {
    std::mutex syncMutex;
    std::mutex mutex;
    std::map<std::filesystem::path, bool> dirty;
#ifdef _WIN32
    FILETIME mark = [] { FILETIME now; GetSystemTimeAsFileTime(&now); return now; }();
#endif
};
WrittenPathRegistry& Written() { static WrittenPathRegistry registry; return registry; }
#ifdef _WIN32
std::optional<FILETIME> WriteTime(const std::filesystem::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return data.ftLastWriteTime;
    const auto error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return std::nullopt;
    throw std::system_error(static_cast<int>(error), std::system_category(), "Reading the write time of " + path.string());
}
bool ChangedSince(const std::filesystem::path& path, const FILETIME& mark) {
    const auto time = WriteTime(path);
    return time && CompareFileTime(&*time, &mark) >= 0;
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
#endif
int DirectoryFailure(const std::error_code& error) {
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    return 5;
}
}

extern "C" void AddPathAlias_nid_no_patch(const char* guestPrefix, const char* hostPath) {
    if (guestPrefix == nullptr || hostPath == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    const auto prefix = TrimSlashes(guestPrefix);
    std::erase(aliases.blocked, prefix);
    for (auto& entry : aliases.entries) {
        if (entry.first == prefix) {
            entry.second = hostPath;
            return;
        }
    }
    aliases.entries.emplace_back(prefix, hostPath);
}

extern "C" void RemovePathAlias_nid_no_patch(const char* guestPrefix) {
    if (guestPrefix == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    const auto prefix = TrimSlashes(guestPrefix);
    std::erase_if(aliases.entries, [&](const auto& entry) { return entry.first == prefix; });
}

extern "C" void RecordWrittenPath_nid_no_patch(const std::filesystem::path& path) {
    auto& registry = Written();
    std::lock_guard lock(registry.mutex);
    registry.dirty[path.lexically_normal()] = true;
}

extern "C" std::vector<std::filesystem::path> WrittenPaths_nid_no_patch() {
    auto& registry = Written();
    std::lock_guard lock(registry.mutex);
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : registry.dirty) paths.push_back(entry.first);
    return paths;
}

extern "C" void SyncWrittenPaths_nid_no_patch() {
    auto& registry = Written();
    std::lock_guard serial(registry.syncMutex);
#ifdef _WIN32
    FILETIME start;
    GetSystemTimeAsFileTime(&start);
    FILETIME mark;
    std::vector<std::pair<std::filesystem::path, bool>> entries;
    {
        std::lock_guard lock(registry.mutex);
        mark = registry.mark;
        registry.mark = start;
        for (auto& [path, dirty] : registry.dirty) {
            entries.emplace_back(path, dirty);
            dirty = false;
        }
    }
    std::set<std::filesystem::path> flushed;
    std::vector<std::filesystem::path> missing;
    for (const auto& [path, dirty] : entries) {
        const auto time = WriteTime(path);
        if (!time) missing.push_back(path);
        if (!dirty && !(time && CompareFileTime(&*time, &mark) >= 0)) continue;
        if (time && flushed.insert(path).second) FlushPath(path);
        for (auto ancestor = path.parent_path(); ancestor.has_relative_path() && !flushed.contains(ancestor) && ChangedSince(ancestor, mark);
             ancestor = ancestor.parent_path()) {
            flushed.insert(ancestor);
            FlushPath(ancestor);
        }
    }
    std::lock_guard lock(registry.mutex);
    for (const auto& path : missing) {
        const auto entry = registry.dirty.find(path);
        if (entry != registry.dirty.end() && !entry->second) registry.dirty.erase(entry);
    }
#else
    ::sync();
    std::lock_guard lock(registry.mutex);
    registry.dirty.clear();
#endif
}

extern "C" void BlockPathAlias_nid_no_patch(const char* guestPrefix) {
    if (guestPrefix == nullptr) { APS5_INVALID_ARG_EX; }
    const auto prefix = TrimSlashes(guestPrefix);
    if (prefix.empty()) { APS5_INVALID_ARG_EX; }
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    std::erase_if(aliases.entries, [&](const auto& entry) { return entry.first == prefix; });
    std::erase(aliases.blocked, prefix);
    aliases.blocked.push_back(prefix);
}

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path) {
    if (!path) { APS5_INVALID_ARG_EX; }
    auto& state = Directories();
    std::lock_guard lock(state.mutex);
    return Resolve(state, path);
}

extern "C" int APS5_VABI chdir_nid_postfix(const char* path) {
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        std::error_code error;
        const auto resolved = std::filesystem::canonical(Resolve(state, path), error);
        if (error) { errno = DirectoryFailure(error); return -1; }
        if (!std::filesystem::is_directory(resolved, error)) {
            errno = error ? DirectoryFailure(error) : 20; return -1;
        }
        const auto relative = resolved.lexically_relative(state.root);
        if (relative.empty() || *relative.begin() == "..") { errno = 45; return -1; }
        state.current = resolved;
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return -1; }
}

extern "C" char* APS5_VABI getcwd_nid_postfix(char* buffer, std::size_t size) {
    if (buffer && size == 0) { errno = 22; return nullptr; }
    try {
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        std::error_code error;
        if (!std::filesystem::is_directory(state.current, error)) {
            errno = error ? DirectoryFailure(error) : 2; return nullptr;
        }
        const auto relative = state.current.lexically_relative(state.root);
        const auto path = relative == "." ? std::string("/") : "/" + relative.generic_string();
        const auto required = path.size() + 1;
        if ((buffer || size) && size < required) { errno = 34; return nullptr; }
        if (!buffer) buffer = static_cast<char*>(GuestHeap::GuestHeapAllocate_nid_postfix(size ? size : required));
        std::memcpy(buffer, path.c_str(), required);
        return buffer;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return nullptr; }
}

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName) + " not implemented");
}
