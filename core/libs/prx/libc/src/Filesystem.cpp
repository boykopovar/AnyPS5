#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static std::filesystem::path FollowedPath(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return path;
    const HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {};
    std::wstring target(MAX_PATH, L'\0');
    DWORD length = GetFinalPathNameByHandleW(handle, target.data(), static_cast<DWORD>(target.size()), FILE_NAME_NORMALIZED);
    if (length >= target.size()) {
        target.resize(length);
        length = GetFinalPathNameByHandleW(handle, target.data(), length, FILE_NAME_NORMALIZED);
    }
    const DWORD error = GetLastError();
    CloseHandle(handle);
    if (length == 0 || length >= target.size()) {
        SetLastError(length == 0 ? error : ERROR_FILENAME_EXCED_RANGE);
        return {};
    }
    target.resize(length);
    return target;
}
#endif

extern "C" int APS5_VABI access_nid_postfix(const char* path, int mode) {
    if (mode < 0 || (mode & ~7)) { errno = 22; return -1; }
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        const auto resolved = ResolvePath_nid_no_patch(path);
#ifdef _WIN32
        const auto target = FollowedPath(resolved);
        const DWORD attributes = target.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(target.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD failure = GetLastError();
            errno = failure == ERROR_CANT_RESOLVE_FILENAME ? 62 : FilesystemError(std::error_code(static_cast<int>(failure), std::system_category()));
            if (errno == 2) errno = NotFoundError_nid_no_patch(resolved);
            return -1;
        }
        if (mode == 0) return 0;
        if ((mode & 2) && !(attributes & FILE_ATTRIBUTE_DIRECTORY) && (attributes & FILE_ATTRIBUTE_READONLY)) {
            errno = 13; return -1;
        }
        DWORD size = 0;
        constexpr auto information = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
        GetFileSecurityW(target.c_str(), information, nullptr, 0, &size);
        if (!size) { errno = 13; return -1; }
        std::vector<unsigned char> descriptor(size);
        if (!GetFileSecurityW(target.c_str(), information, descriptor.data(), size, &size)) {
            errno = 13; return -1;
        }
        struct Token {
            HANDLE value = nullptr;
            ~Token() { if (value) CloseHandle(value); }
        } source, impersonation;
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, TRUE, &source.value)) {
            if (GetLastError() != ERROR_NO_TOKEN ||
                !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &source.value)) {
                errno = 13; return -1;
            }
        }
        if (!DuplicateToken(source.value, SecurityImpersonation, &impersonation.value)) { errno = 13; return -1; }
        DWORD desired = 0;
        if (mode & 4) desired |= FILE_READ_DATA;
        if (mode & 2) desired |= FILE_WRITE_DATA;
        if (mode & 1) desired |= FILE_EXECUTE;
        GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
        PRIVILEGE_SET privileges{};
        DWORD privilegeSize = sizeof(privileges), granted = 0;
        BOOL allowed = FALSE;
        if (!AccessCheck(descriptor.data(), impersonation.value, desired, &mapping,
                         &privileges, &privilegeSize, &granted, &allowed) || !allowed) {
            errno = 13; return -1;
        }
        return 0;
#else
        if (::access(resolved.c_str(), mode) == 0) return 0;
        errno = FilesystemError(std::error_code(errno, std::generic_category()));
        return -1;
#endif
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code()); return -1;
    }
}

extern "C" bool ExecutePermission_nid_no_patch(const std::filesystem::path& native) {
#ifdef _WIN32
    const auto target = FollowedPath(native);
    if (target.empty() || GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    DWORD size = 0;
    constexpr auto information = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    GetFileSecurityW(target.c_str(), information, nullptr, 0, &size);
    if (!size) return false;
    std::vector<unsigned char> descriptor(size);
    if (!GetFileSecurityW(target.c_str(), information, descriptor.data(), size, &size)) return false;
    struct Token {
        HANDLE value = nullptr;
        ~Token() { if (value) CloseHandle(value); }
    } source, impersonation;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, TRUE, &source.value)) {
        if (GetLastError() != ERROR_NO_TOKEN ||
            !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &source.value)) {
            return false;
        }
    }
    if (!DuplicateToken(source.value, SecurityImpersonation, &impersonation.value)) return false;
    GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
    PRIVILEGE_SET privileges{};
    DWORD privilegeSize = sizeof(privileges), granted = 0;
    BOOL allowed = FALSE;
    if (!AccessCheck(descriptor.data(), impersonation.value, FILE_EXECUTE, &mapping,
                     &privileges, &privilegeSize, &granted, &allowed) || !allowed) return false;
    return true;
#else
    return ::access(native.c_str(), X_OK) == 0;
#endif
}

extern "C" int APS5_VABI remove_nid_postfix(const char* path) {
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        const auto resolved = ResolvePath_nid_no_patch(path);
        if (FinalPathComponentIsDot_nid_no_patch(path)) {
            std::error_code statusError;
            const auto status = std::filesystem::status(resolved, statusError);
            if (statusError) { errno = FilesystemError(statusError); return -1; }
            if (!std::filesystem::exists(status)) { errno = 2; return -1; }
            if (!std::filesystem::is_directory(status)) { errno = 20; return -1; }
            errno = 22;
            return -1;
        }
        std::error_code error;
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(resolved.c_str());
        bool removed = false;
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            const DWORD writable = attributes & ~FILE_ATTRIBUTE_READONLY;
            const bool unlocked = writable != attributes &&
                SetFileAttributesW(resolved.c_str(), writable ? writable : FILE_ATTRIBUTE_NORMAL);
            removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ?
                RemoveDirectoryW(resolved.c_str()) != 0 : DeleteFileW(resolved.c_str()) != 0;
            if (!removed && unlocked) {
                const DWORD nativeError = GetLastError();
                SetFileAttributesW(resolved.c_str(), attributes);
                SetLastError(nativeError);
            }
        }
        if (!removed) {
            const DWORD nativeError = GetLastError();
            if (nativeError == ERROR_DIR_NOT_EMPTY) { errno = 66; return -1; }
            error = std::error_code(static_cast<int>(nativeError), std::system_category());
        }
#else
        const bool removed = std::filesystem::remove(resolved, error);
#endif
        if (error || !removed) {
            errno = error ? FilesystemError(error) : 2;
            if (errno == 2) errno = NotFoundError_nid_no_patch(resolved);
            return -1;
        }
        RecordWrittenPath_nid_no_patch(resolved);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
