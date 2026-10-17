#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <json/json.h>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ShaderArchive {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
using Clock = std::chrono::steady_clock;
inline void Check(bool ok, std::string_view message) {
    if (!ok)
        throw std::runtime_error(std::string(message));
}
inline double Seconds(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}
View Slice(View data, std::size_t offset, std::size_t size);
std::uint64_t ReadInt(View data, std::size_t offset, unsigned width, bool big = false);
inline std::uint32_t U32(View d, std::size_t at) {
    return static_cast<std::uint32_t>(ReadInt(d, at, 4));
}
inline std::uint64_t U64(View d, std::size_t at) {
    return ReadInt(d, at, 8);
}
inline std::uint16_t U16(View d, std::size_t at) {
    return static_cast<std::uint16_t>(ReadInt(d, at, 2));
}
std::string Hex(View data);
std::string Sha256(View data);
Bytes ReadFile(const fs::path &path);
void WriteFile(const fs::path &path, View bytes);
void WriteAtomic(const fs::path &path, View bytes);
void PublishAtomic(const fs::path &temporary, const fs::path &path);
void WriteText(const fs::path &path, const std::string &text);
std::string JsonText(const Json::Value &value);
Json::Value ParseJson(std::string_view text);
Json::Value ReadJson(const fs::path &path);
void WriteJson(const fs::path &path, const Json::Value &value);
Json::Value ProcessUsage();
void Parallel(std::size_t count, unsigned workers, const std::function<void(std::size_t, unsigned)> &fn);

class MappedFile {
  public:
    explicit MappedFile(const fs::path &path);
    ~MappedFile();
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;
    View Data() const {
        return {data, size};
    }

  private:
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    void *file = nullptr;
    void *mapping = nullptr;
    int fd = -1;
};
struct Reader {
    View data;
    std::size_t at = 0;
    View Take(std::size_t n) {
        auto v = Slice(data, at, n);
        at += n;
        return v;
    }
    std::uint32_t Word() {
        auto v = Take(4);
        return U32(v, 0);
    }
    View Array(std::size_t width) {
        const auto n = Word();
        Check(n <= 0x7fffffff && n <= (data.size() - at) / width, "invalid array length");
        return Take(n * width);
    }
    std::string String();
};
} // namespace ShaderArchive
