#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP

#include <cstddef>
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
    std::wstring wide = target.wstring();
    const std::size_t rootLength = target.root_path().wstring().size();
    const std::size_t minLength = rootLength > 0 ? rootLength : (wide.empty() ? 0 : 1);
    while (wide.size() > minLength && (wide.back() == L'/' || wide.back() == L'\\')) {
        wide.pop_back();
    }
    const DWORD attributes = GetFileAttributesW(wide.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD missing = GetLastError();
        if (missing != ERROR_FILE_NOT_FOUND && missing != ERROR_PATH_NOT_FOUND)
            error = std::error_code(static_cast<int>(missing), std::system_category());
        return;
    }
    const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const bool reparsePoint = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    if (directory && !reparsePoint) {
        std::error_code listing;
        {
            std::filesystem::directory_iterator entries(std::filesystem::path(wide), listing);
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
    if (!reparsePoint && (attributes & FILE_ATTRIBUTE_READONLY) != 0) {
        const DWORD writableAttributes = attributes & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY);
        if (SetFileAttributesW(wide.c_str(), writableAttributes != 0 ? writableAttributes : FILE_ATTRIBUTE_NORMAL) != 0) {
            if (directory ? RemoveDirectoryW(wide.c_str()) != 0 : DeleteFileW(wide.c_str()) != 0) return;
            SetFileAttributesW(wide.c_str(), attributes);
        }
    }
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
