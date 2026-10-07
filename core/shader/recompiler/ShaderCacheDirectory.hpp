#ifndef CORE_SHADER_RECOMPILER_SHADERCACHEDIRECTORY_HPP
#define CORE_SHADER_RECOMPILER_SHADERCACHEDIRECTORY_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace ShaderRecompiler {

std::filesystem::path ShaderCacheDirectory();

inline bool WriteFileAtomically(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    static std::atomic<std::uint64_t> serial{0};
    const auto unique = std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
                        static_cast<std::size_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    auto temporary = path;
    temporary +=
        ".tmp." + std::to_string(unique) + "." + std::to_string(serial.fetch_add(1, std::memory_order_relaxed));
    bool written = false;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        written = !file.fail();
    }
    if (!written) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

inline bool ReadWholeFile(const std::filesystem::path& path, std::vector<std::byte>& bytes) {
    bytes.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    const auto size = static_cast<std::streamoff>(file.tellg());
    if (size < 0)
        return false;
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        bytes.clear();
        return false;
    }
    return true;
}

inline std::uint64_t HashBytes(std::span<const std::byte> bytes, std::uint64_t seed = 0) {
    constexpr std::uint64_t multiplier = 0x9e3779b97f4a7c15ull;
    std::uint64_t hash = seed ^ (static_cast<std::uint64_t>(bytes.size()) * multiplier);
    const auto mix = [&](std::uint64_t chunk) {
        chunk *= 0xff51afd7ed558ccdull;
        chunk ^= chunk >> 32u;
        hash = (hash ^ chunk) * multiplier;
        hash = (hash << 27u) | (hash >> 37u);
    };
    std::size_t index = 0;
    for (; index + 8 <= bytes.size(); index += 8) {
        std::uint64_t chunk;
        std::memcpy(&chunk, bytes.data() + index, sizeof(chunk));
        mix(chunk);
    }
    if (index < bytes.size()) {
        std::uint64_t chunk = 0;
        std::memcpy(&chunk, bytes.data() + index, bytes.size() - index);
        mix(chunk ^ 0x8000000000000000ull);
    }
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33u;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33u;
    return hash;
}

}

#endif
