#include "InputConfiguration.hpp"

#include <fstream>
#include <stdexcept>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

void InputConfig::WriteAtomic(const std::filesystem::path& path, std::string_view text) {
    static std::atomic<unsigned long> sequence{};
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
    try {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Input: cannot create '" + temporary.string() + "'");
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.flush();
        if (!file) throw std::runtime_error("Input: cannot write '" + temporary.string() + "'");
        file.close();
        if (!file) throw std::runtime_error("Input: cannot close '" + temporary.string() + "'");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("Input: cannot replace '" + path.string() + "': " + std::to_string(GetLastError()));
        }
#else
        std::filesystem::rename(temporary, path);
#endif
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
}
