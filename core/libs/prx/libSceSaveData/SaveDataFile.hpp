#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <random>
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

namespace savedata {

template <typename ReplaceOperation>
inline bool replace_file_with(const std::filesystem::path& path, const void* data, std::size_t size,
                              ReplaceOperation&& replace_operation) {
    if (size != 0 && data == nullptr) return false;
    const auto parent = path.parent_path();
    std::error_code error;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    std::filesystem::path temporary;
    std::FILE* file = nullptr;
    std::random_device random;
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        temporary = path;
        temporary += ".tmp." + std::to_string(random()) + "." + std::to_string(random());
#ifdef _WIN32
        file = _wfopen(temporary.c_str(), L"wbx");
#else
        file = std::fopen(temporary.c_str(), "wbx");
#endif
        if (file) break;
        if (errno != EEXIST) return false;
    }
    if (!file) return false;

    struct Cleanup {
        const std::filesystem::path& path;
        std::FILE*& file;
        bool remove = true;
        ~Cleanup() {
            if (file) std::fclose(file);
            std::error_code ignored;
            if (remove) std::filesystem::remove(path, ignored);
        }
    } cleanup{temporary, file};

    const bool written = size == 0 || std::fwrite(data, 1, size, file) == size;
    const bool closed = std::fclose(file) == 0;
    file = nullptr;
    if (!written || !closed) return false;
    const bool replaced = replace_operation(temporary, path);
    cleanup.remove = !replaced;
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
