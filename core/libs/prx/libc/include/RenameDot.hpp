#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RENAMEDOT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RENAMEDOT_HPP

#include <filesystem>
#include <string>
#include "FilesystemError.hpp"

inline std::string TrimTrailingSeparators_nid_no_patch(std::string path) {
    const auto separator = [](char character) { return character == '/' || character == '\\'; };
    const std::size_t root = std::filesystem::path(path).root_path().native().size();
    while (path.size() > root && path.size() > 1 && separator(path.back())) path.pop_back();
    return path;
}

inline int RenameDotError_nid_no_patch(const char* path) {
    std::size_t length = std::char_traits<char>::length(path);
    const auto separator = [](char character) { return character == '/' || character == '\\'; };
    while (length > 1 && separator(path[length - 1])) --length;
    std::size_t start = length;
    while (start > 0 && !separator(path[start - 1])) --start;
    const std::size_t name = length - start;
    const bool dot = name == 1 && path[start] == '.';
    const bool dotDot = name == 2 && path[start] == '.' && path[start + 1] == '.';
    if (!dot && !dotDot) return 0;
    const std::string parent = start == 0 ? std::string(".") : TrimTrailingSeparators_nid_no_patch(std::string(path, start));
    std::filesystem::path resolved;
    if (const int error = ResolvePathError_nid_no_patch(parent.c_str(), resolved)) return error;
    std::error_code status;
    const auto type = std::filesystem::status(resolved, status).type();
    if (type == std::filesystem::file_type::not_found) return status == std::errc::not_a_directory ? 20 : 2;
    if (status) return FilesystemError(status);
    return type == std::filesystem::file_type::directory ? 22 : 20;
}

#endif
