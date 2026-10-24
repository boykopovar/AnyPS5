#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <filesystem>
#include <new>
#ifdef _WIN32
#include <cstddef>
#include <cstring>
#include <vector>
#include <windows.h>
#endif

std::error_code RenameNative_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination, [[maybe_unused]] bool replacesExisting) {
    namespace fs = std::filesystem;
    std::error_code error;
#ifdef _WIN32
    if (replacesExisting) {
        const auto target = fs::absolute(destination, error);
        if (error) return error;
        struct RenameInfo { DWORD flags; HANDLE root; DWORD nameBytes; WCHAR name[1]; };
        static_assert(sizeof(RenameInfo) == sizeof(FILE_RENAME_INFO) && offsetof(RenameInfo, name) == offsetof(FILE_RENAME_INFO, FileName));
        if (target.native().size() > (MAXDWORD - sizeof(RenameInfo)) / sizeof(wchar_t)) return std::make_error_code(std::errc::filename_too_long);
        const auto nameBytes = target.native().size() * sizeof(wchar_t);
        std::vector<std::byte> buffer(sizeof(RenameInfo) + nameBytes);
        auto* info = reinterpret_cast<RenameInfo*>(buffer.data());
        constexpr DWORD replaceExisting = 0x00000001, posixSemantics = 0x00000002, ignoreReadOnly = 0x00000040;
        const auto attributes = GetFileAttributesW(destination.c_str());
        info->flags = replaceExisting | posixSemantics | (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) ? ignoreReadOnly : 0);
        info->nameBytes = static_cast<DWORD>(nameBytes);
        std::memcpy(info->name, target.c_str(), nameBytes);
        const auto handle = CreateFileW(source.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return std::error_code(static_cast<int>(GetLastError()), std::system_category());
        constexpr auto renameInfoEx = static_cast<FILE_INFO_BY_HANDLE_CLASS>(22);
        if (!SetFileInformationByHandle(handle, renameInfoEx, info, static_cast<DWORD>(buffer.size()))) error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        CloseHandle(handle);
        return error;
    }
#endif
    fs::rename(source, destination, error);
    return error;
}

#ifdef _WIN32
static bool BelowSymbolicLinkLoop(const std::filesystem::path& directory) {
    const auto handle = CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_CANT_RESOLVE_FILENAME;
    CloseHandle(handle);
    return false;
}
#endif

static int RenamePath_nid_no_patch(const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr int GuestEnoent = 2;
    constexpr int GuestEnotdir = 20;
    constexpr int GuestEisdir = 21;
    constexpr int GuestEinval = 22;
    constexpr int GuestEnotempty = 66;
    namespace fs = std::filesystem;
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    if (sourceStatus.type() == fs::file_type::not_found) return GuestEnoent;

    const auto parent = destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
    error.clear();
    const auto parentStatus = fs::status(parent, error);
    if (error) return FilesystemError(error);
    if (!fs::exists(parentStatus)) return GuestEnoent;
    if (!fs::is_directory(parentStatus)) return GuestEnotdir;

    error.clear();
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    const bool destinationExists = destinationStatus.type() != fs::file_type::not_found;
    const bool sourceIsDirectory = fs::is_directory(sourceStatus);
    const bool destinationIsDirectory = destinationExists && fs::is_directory(destinationStatus);
    bool sameFile = false;
    if (destinationExists) {
        error.clear();
        sameFile = fs::equivalent(source, destination, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return FilesystemError(error);
        if (!sameFile && sourceIsDirectory && !destinationIsDirectory) return GuestEnotdir;
        if (!sameFile && !sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
    }
    if (sourceIsDirectory && !sameFile) {
        error.clear();
        const auto canonicalSource = fs::weakly_canonical(source, error);
        if (error) return FilesystemError(error);
        const auto canonicalDestination = fs::weakly_canonical(destination, error);
        if (error) return FilesystemError(error);
        const auto relative = canonicalDestination.lexically_relative(canonicalSource);
        if (!relative.empty() && *relative.begin() != "..") return GuestEinval;
    }
    if (destinationExists && !sameFile && destinationIsDirectory) {
        error.clear();
        if (!fs::is_empty(destination, error)) {
            return error ? FilesystemError(error) : GuestEnotempty;
        }
    }
    error = RenameNative_nid_no_patch(source, destination, destinationExists && (!sourceIsDirectory || !sameFile));
    return error ? FilesystemError(error) : 0;
}

static int SourceLookupError_nid_no_patch(const std::filesystem::path& source) {
    std::error_code status;
    const auto sourceStatus = std::filesystem::symlink_status(source, status);
    if (HasNonDirectoryPrefix_nid_no_patch(source)) return 20;
    if (status && status != std::errc::no_such_file_or_directory) return FilesystemError(status);
    return sourceStatus.type() == std::filesystem::file_type::not_found ? 2 : 0;
}

extern "C" int RenameGuestPath_nid_no_patch(const char* from, const char* to) {
    if (!from) { errno = 14; return -1; }
    if (!*from) { errno = 2; return -1; }
    if (const int dot = RenameDotError_nid_no_patch(from)) { errno = dot; return -1; }
    const int destinationError = !to ? 14 : !*to ? 2 : RenameDotError_nid_no_patch(to);
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        if (destinationError) {
            const int sourceError = SourceLookupError_nid_no_patch(source);
            errno = sourceError ? sourceError : destinationError;
            return -1;
        }
        const auto destination = ResolvePath_nid_no_patch(to);
        int error = RenamePath_nid_no_patch(source, destination);
        if (error == 2) {
            std::error_code status;
            if (HasNonDirectoryPrefix_nid_no_patch(source) || (std::filesystem::exists(source, status) && HasNonDirectoryPrefix_nid_no_patch(destination))) error = 20;
        }
#ifdef _WIN32
        constexpr int GuestEloop = 62;
        if (error == 2 || error == 20) {
            if (BelowSymbolicLinkLoop(source.parent_path())) error = GuestEloop;
            else if (SourceLookupError_nid_no_patch(source) == 0 && BelowSymbolicLinkLoop(destination.parent_path())) error = GuestEloop;
        }
#endif
        if (error) { errno = error; return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    return RenameGuestPath_nid_no_patch(from, to);
}
