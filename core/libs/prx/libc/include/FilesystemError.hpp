#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESYSTEMERROR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESYSTEMERROR_HPP

#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
#endif
#include "General.hpp"

inline int FilesystemError(const std::error_code& error) {
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::operation_not_permitted) return 1;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::is_a_directory) return 21;
    if (error == std::errc::directory_not_empty) return 66;
    if (error == std::errc::device_or_resource_busy) return 16;
    if (error == std::errc::read_only_file_system) return 30;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    if (error == std::errc::not_enough_memory) return 12;
    if (error == std::errc::invalid_argument) return 22;
    if (error == std::errc::cross_device_link) return 18;
    if (error == std::errc::file_exists) return 17;
    if (error == std::errc::no_space_on_device) return 28;
    return 5;
}

enum class LinkPrefix_nid_no_patch { Resolves, Missing, BlockedByFile };

inline bool HasNonDirectoryPrefix_nid_no_patch(const std::filesystem::path& native);

#ifdef _WIN32
inline std::filesystem::path FinalDirectory_nid_no_patch(const std::filesystem::path& directory) {
    const HANDLE handle = CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return directory;
    std::wstring name(MAX_PATH, L'\0');
    DWORD length = GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), VOLUME_NAME_DOS);
    if (length >= name.size()) {
        name.resize(length + 1);
        length = GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), VOLUME_NAME_DOS);
    }
    CloseHandle(handle);
    if (length == 0 || length >= name.size()) return directory;
    name.resize(length);
    if (name.rfind(L"\\\\?\\UNC\\", 0) == 0) name.replace(0, 7, L"\\");
    else if (name.rfind(L"\\\\?\\", 0) == 0) name.erase(0, 4);
    return name;
}
#endif

inline LinkPrefix_nid_no_patch LinkPrefixState_nid_no_patch(const std::filesystem::path& prefix) {
#ifdef _WIN32
    constexpr unsigned MaxDepth = 32;
    thread_local unsigned depth = 0;
    if (depth >= MaxDepth) return LinkPrefix_nid_no_patch::Resolves;
    const DWORD attributes = GetFileAttributesW(prefix.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return LinkPrefix_nid_no_patch::Resolves;
    const HANDLE handle = CreateFileW(prefix.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return LinkPrefix_nid_no_patch::Resolves;
    }
    const DWORD code = GetLastError();
    if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) return LinkPrefix_nid_no_patch::Resolves;
    std::string text;
    try {
        if (!File::WindowsSymbolicLink::Read(prefix, text, "lookup")) return LinkPrefix_nid_no_patch::Resolves;
    } catch (const std::system_error&) {
        return LinkPrefix_nid_no_patch::Resolves;
    }
    const std::filesystem::path target(text);
    struct DepthGuard {
        explicit DepthGuard(unsigned& value) : value(value) { ++value; }
        ~DepthGuard() { --value; }
        unsigned& value;
    } guard(depth);
    const bool blocked = HasNonDirectoryPrefix_nid_no_patch((target.is_absolute() ? target : FinalDirectory_nid_no_patch(prefix.parent_path()) / target) / ".");
    return blocked ? LinkPrefix_nid_no_patch::BlockedByFile : LinkPrefix_nid_no_patch::Missing;
#else
    (void)prefix;
    return LinkPrefix_nid_no_patch::Resolves;
#endif
}

inline bool HasNonDirectoryPrefix_nid_no_patch(const std::filesystem::path& native) {
    std::error_code error;
    const auto trimmed = native.has_filename() ? native : native.parent_path();
    for (auto prefix = trimmed.parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
        const auto link = LinkPrefixState_nid_no_patch(prefix);
        if (link != LinkPrefix_nid_no_patch::Resolves) return link == LinkPrefix_nid_no_patch::BlockedByFile;
        const auto status = std::filesystem::status(prefix, error);
        if (std::filesystem::exists(status)) return !std::filesystem::is_directory(status);
    }
    return false;
}

inline int NotFoundError_nid_no_patch(const std::filesystem::path& native) {
    return HasNonDirectoryPrefix_nid_no_patch(native) ? 20 : 2;
}

std::error_code RenameNative_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination, bool replacesExisting);

extern "C" bool ExecutePermission_nid_no_patch(const std::filesystem::path& native);

inline int ResolvePathError_nid_no_patch(const char* path, std::filesystem::path& resolved, bool module = false, bool archived = false) {
    try {
        resolved = archived ? ResolveArchivedPath_nid_no_patch(path, module) : module ? ResolvePathOrRelinkedModule_nid_no_patch(path) : ResolvePath_nid_no_patch(path);
        return 0;
    } catch (const std::filesystem::filesystem_error& error) {
        if (error.code() == std::errc::illegal_byte_sequence) return 2;
        return FilesystemError(error.code());
    }
}

#endif
