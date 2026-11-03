#ifndef CORE_LIBS_TESTS_TEMPORARYFILE_HPP
#define CORE_LIBS_TESTS_TEMPORARYFILE_HPP

#include <cstdio>

#ifdef _WIN32
#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <process.h>
#include <string>
#include <system_error>
#endif

namespace TemporaryFile {

inline std::FILE* Open() {
#ifdef _WIN32
    static std::atomic<unsigned> serial{0};
    std::error_code error;
    const auto directory = std::filesystem::temp_directory_path(error);
    if (error) return nullptr;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto name = directory / ("anyps5-test-" + std::to_string(_getpid()) + "-" + std::to_string(tick) + "-" + std::to_string(serial++) + ".tmp");
        if (std::FILE* file = _wfopen(name.c_str(), L"w+bxD")) return file;
        if (errno != EEXIST) return nullptr;
    }
    return nullptr;
#else
    return std::tmpfile();
#endif
}

}

#endif
