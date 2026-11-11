#include "prx/libSceAgcDriver/Execution/include/ShaderCachePrefetch.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::PrefetchShaderCacheEntries;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void Write(const std::filesystem::path& path, std::size_t bytes, int ageSeconds) {
    std::ofstream file(path, std::ios::binary);
    const std::vector<char> data(bytes, 'x');
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.close();
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() - std::chrono::seconds(ageSeconds));
}

void CheckSelection(const std::filesystem::path& directory) {
    Write(directory / "oldest.bin", 1000, 300);
    Write(directory / "middle.bin", 2000, 200);
    Write(directory / "newest.bin", 3u << 20u, 100);
    Write(directory / "note.txt", 500, 50);
    std::filesystem::create_directory(directory / "nested.bin");

    const auto all = PrefetchShaderCacheEntries(directory, 1ull << 30u, 4, false);
    Require(all.files == 3 && all.skipped == 0, "prefetch did not read every entry: " + std::to_string(all.files) + " read, " + std::to_string(all.skipped) + " skipped");
    Require(all.bytes == 1000u + 2000u + (3u << 20u), "prefetch counted " + std::to_string(all.bytes) + " bytes, including a file larger than its read buffer");

    const auto limited = PrefetchShaderCacheEntries(directory, (3u << 20u) + 2500u, 2, false);
    Require(limited.files == 2 && limited.skipped == 1, "a limit holding the newest entry and one more read " + std::to_string(limited.files) + " and left out " + std::to_string(limited.skipped));
    Require(limited.bytes == (3u << 20u) + 2000u, "the limit kept " + std::to_string(limited.bytes) + " bytes instead of the newest entry and the next newest that fits");

    const auto tight = PrefetchShaderCacheEntries(directory, 3000u, 1, false);
    Require(tight.files == 2 && tight.skipped == 1 && tight.bytes == 3000u, "a limit below the newest entry did not fall back to the older entries that fit");

    const auto none = PrefetchShaderCacheEntries(directory, 0, 4, false);
    Require(none.files == 0 && none.bytes == 0 && none.skipped == 3, "a zero limit read entries");
}

void CheckDegenerate(const std::filesystem::path& directory) {
    const auto missing = PrefetchShaderCacheEntries(directory / "missing", 1ull << 30u, 4, false);
    Require(missing.files == 0 && missing.bytes == 0 && missing.skipped == 0, "a missing directory read entries");
    const auto zeroThreads = PrefetchShaderCacheEntries(directory, 1ull << 30u, 0, false);
    Require(zeroThreads.files == 3, "zero threads read " + std::to_string(zeroThreads.files) + " entries instead of running on the caller");
    const auto manyThreads = PrefetchShaderCacheEntries(directory, 1ull << 30u, 64, true);
    Require(manyThreads.files == 3 && manyThreads.bytes == 1000u + 2000u + (3u << 20u), "more threads than entries changed what was read");
}

}

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / ("aps5_shader_cache_prefetch_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        try {
            CheckSelection(directory);
            CheckDegenerate(directory);
        } catch (...) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            throw;
        }
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
