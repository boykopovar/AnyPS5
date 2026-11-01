#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP

#include <filesystem>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

inline void RemoveTree(const std::filesystem::path& target, std::error_code& error) {
    error.clear();
#ifdef _WIN32
    const std::wstring wide = target.wstring();
    const DWORD attributes = GetFileAttributesW(wide.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD missing = GetLastError();
        if (missing != ERROR_FILE_NOT_FOUND && missing != ERROR_PATH_NOT_FOUND)
            error = std::error_code(static_cast<int>(missing), std::system_category());
        return;
    }
    const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (directory && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        std::error_code listing;
        {
            std::filesystem::directory_iterator entries(target, listing);
            while (!listing && entries != std::filesystem::directory_iterator()) {
                RemoveTree(entries->path(), error);
                if (error) return;
                entries.increment(listing);
            }
        }
        if (listing) {
            error = listing;
            return;
        }
    }
    if (directory ? RemoveDirectoryW(wide.c_str()) != 0 : DeleteFileW(wide.c_str()) != 0) return;
    const DWORD failed = GetLastError();
    SetFileAttributesW(wide.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (directory ? RemoveDirectoryW(wide.c_str()) != 0 : DeleteFileW(wide.c_str()) != 0) return;
    error = std::error_code(static_cast<int>(failed), std::system_category());
#else
    std::error_code removal;
    std::filesystem::remove_all(target, removal);
    if (removal) error = removal;
#endif
}

inline void RemoveTree(const std::filesystem::path& target) {
    std::error_code error;
    RemoveTree(target, error);
    if (error) throw std::filesystem::filesystem_error("RemoveTree", target, error);
}

#endif
