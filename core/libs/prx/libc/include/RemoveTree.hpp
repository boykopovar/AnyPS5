#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_REMOVETREE_HPP

#include <filesystem>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <string_view>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace File {

template<typename TVisitor>
inline void ForEachDirectoryEntry(const std::filesystem::path& directory, std::error_code& error, TVisitor visit) {
#ifdef _WIN32
    error.clear();
    WIN32_FIND_DATAW data{};
    const auto handle = FindFirstFileW((directory / L"*").c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto code = GetLastError();
        if (code != ERROR_FILE_NOT_FOUND) error = std::error_code(code, std::system_category());
        return;
    }
    struct Search { HANDLE handle; ~Search() { FindClose(handle); } } search{handle};
    do {
        const std::wstring_view name(data.cFileName);
        if (name == L"." || name == L"..") continue;
        visit(directory / data.cFileName);
        if (error) return;
    } while (FindNextFileW(handle, &data));
    const auto code = GetLastError();
    if (code != ERROR_NO_MORE_FILES) error = std::error_code(code, std::system_category());
#else
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
        visit(it->path());
        if (error) return;
    }
#endif
}

}

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
        File::ForEachDirectoryEntry(target, error, [&](const auto& child) { RemoveTree(child, error); });
        if (error) return;
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
