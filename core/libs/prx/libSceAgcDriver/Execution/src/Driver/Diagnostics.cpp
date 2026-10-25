#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace AgcDriver::DriverDetail {

double TraceMs() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - origin).count();
}

namespace {

constexpr std::size_t MaxSkippedShaders = 256;
std::array<std::atomic<std::uint64_t>, MaxSkippedShaders> skippedShaders{};
std::atomic<std::size_t> skippedShaderCount{0};

std::optional<std::uint64_t> programAddress(std::string_view word) {
    if (word.starts_with("0x") || word.starts_with("0X")) word.remove_prefix(2);
    std::uint64_t address = 0;
    const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), address, 16);
    if (word.empty() || error != std::errc{} || end != word.data() + word.size()) return std::nullopt;
    return address;
}

void ReloadSkippedShaders(const char* path) {
    static std::mutex reload;
    static std::filesystem::file_time_type seen{};
    const std::unique_lock lock(reload, std::try_to_lock);
    if (!lock.owns_lock()) return;
    std::error_code error;
    const auto stamp = std::filesystem::last_write_time(path, error);
    if (error || stamp == seen) return;
    seen = stamp;
    std::ifstream file(path);
    std::size_t count = 0;
    skippedShaderCount.store(0, std::memory_order_release);
    for (std::string word; count < MaxSkippedShaders && file >> word;) {
        if (word.starts_with('#')) {
            std::getline(file, word);
            continue;
        }
        const auto address = programAddress(word);
        if (!address) {
            std::fprintf(stderr, "[skip-shader] ignoring '%s' in %s: not a hexadecimal program address\n", word.c_str(), path);
            continue;
        }
        if (*address != 0) skippedShaders[count++].store(*address, std::memory_order_relaxed);
    }
    skippedShaderCount.store(count, std::memory_order_release);
    std::fprintf(stderr, "[skip-shader] %zu programs listed in %s\n", count, path);
}

}

bool SkippedShader(std::uint64_t address) {
    static const std::uint64_t single = [] { const char* text = std::getenv("APS5_SKIP_SHADER"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
    static const char* const file = std::getenv("APS5_SKIP_SHADER_FILE");
    if (address == 0) return false;
    if (address == single) return true;
    if (file == nullptr) return false;
    static std::atomic<std::chrono::steady_clock::rep> nextCheck{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto due = nextCheck.load(std::memory_order_relaxed);
    if (now.count() >= due && nextCheck.compare_exchange_strong(due, std::chrono::duration_cast<std::chrono::steady_clock::duration>(now + std::chrono::milliseconds(500)).count(), std::memory_order_relaxed)) ReloadSkippedShaders(file);
    const auto count = skippedShaderCount.load(std::memory_order_acquire);
    for (std::size_t index = 0; index < count; ++index) {
        if (skippedShaders[index].load(std::memory_order_relaxed) == address) return true;
    }
    return false;
}

bool SkipRefusedDraws() {
    static const bool skip = std::getenv("APS5_SKIP_REFUSED_DRAWS") != nullptr;
    return skip;
}

void ReportRefusedDraw(const RefusedDraw& refused) {
    static std::mutex reportedMutex;
    static std::set<std::uint64_t> reported;
    {
        std::lock_guard lock(reportedMutex);
        if (!reported.insert(refused.address).second) return;
    }
    const std::string_view what = refused.what();
    const auto line = what.substr(0, what.find('\n'));
    std::fprintf(stderr, "[gpu] APS5_SKIP_REFUSED_DRAWS: skipping the draws that use shader 0x%llx: %.*s\n", static_cast<unsigned long long>(refused.address), static_cast<int>(line.size()), line.data());
}

void require(bool condition, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string("AGC driver: ") + reason);
    }
}

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    require(it != registers.end(), "required shader register has not been written");
    return it->second;
}

std::uint32_t readUserData(const Registers& shader, std::uint32_t offset) {
    const auto it = shader.find(offset);
    return it == shader.end() ? 0u : it->second;
}

}
