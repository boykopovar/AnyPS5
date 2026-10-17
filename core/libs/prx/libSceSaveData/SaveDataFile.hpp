#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
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

namespace savedata {

template <typename ReplaceOperation>
inline bool replace_file_with(const std::filesystem::path& path, const void* data, std::size_t size,
                              ReplaceOperation&& replace_operation) {
    const auto parent = path.parent_path();
    std::error_code error;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    auto temporary = path;
    temporary += ".tmp";
    const auto remove_temporary = [&temporary]() {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    };

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            remove_temporary();
            return false;
        }
        if (size != 0) {
            if (data == nullptr) {
                file.close();
                remove_temporary();
                return false;
            }
            file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }
        file.flush();
        if (!file) {
            file.close();
            remove_temporary();
            return false;
        }
        file.close();
        if (!file) {
            remove_temporary();
            return false;
        }
    }

    const bool replaced = replace_operation(temporary, path);
    if (!replaced) {
        remove_temporary();
    }
    return replaced;
}

inline bool move_replacing(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    return ::MoveFileExW(source.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error;
#endif
}

inline bool replace_file(const std::filesystem::path& path, const void* data, std::size_t size) {
    return replace_file_with(path, data, size, [](const auto& temporary, const auto& destination) {
        return move_replacing(temporary, destination);
    });
}

}
