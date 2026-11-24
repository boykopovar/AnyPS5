#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>

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

void ReloadSkippedShaders(const std::filesystem::path& path) {
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
            std::fprintf(stderr, "[skip-shader] ignoring '%s' in %s: not a hexadecimal program address\n", word.c_str(), path.string().c_str());
            continue;
        }
        if (*address != 0) skippedShaders[count++].store(*address, std::memory_order_relaxed);
    }
    skippedShaderCount.store(count, std::memory_order_release);
    std::fprintf(stderr, "[skip-shader] %zu programs listed in %s\n", count, path.string().c_str());
}

}

bool SkippedShader(std::uint64_t address) {
    static const std::uint64_t single = [] { const char* text = std::getenv("APS5_SKIP_SHADER"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
    static const std::optional<std::filesystem::path> file = [] {
#ifdef _WIN32
        const wchar_t* configured = _wgetenv(L"APS5_SKIP_SHADER_FILE");
#else
        const char* configured = std::getenv("APS5_SKIP_SHADER_FILE");
#endif
        return configured != nullptr ? std::optional<std::filesystem::path>(configured) : std::nullopt;
    }();
    if (address == 0) return false;
    if (address == single) return true;
    if (!file) return false;
    static std::atomic<std::chrono::steady_clock::rep> nextCheck{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto due = nextCheck.load(std::memory_order_relaxed);
    if (now.count() >= due && nextCheck.compare_exchange_strong(due, std::chrono::duration_cast<std::chrono::steady_clock::duration>(now + std::chrono::milliseconds(500)).count(), std::memory_order_relaxed)) ReloadSkippedShaders(*file);
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

bool AsyncDrawCompile() {
    static const bool enabled = [] {
        const char* text = std::getenv("APS5_ASYNC_DRAW_COMPILE");
        return text != nullptr && std::strcmp(text, "0") != 0;
    }();
    return enabled;
}

std::size_t AsyncComputePipelineWords() {
    static const std::size_t words = [] {
        const char* text = std::getenv("APS5_ASYNC_COMPUTE_WORDS");
        return text != nullptr ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{32768};
    }();
    return words;
}

std::chrono::milliseconds AsyncDispatchWait() {
    static const std::chrono::milliseconds wait = [] {
        const char* text = std::getenv("APS5_ASYNC_DISPATCH_WAIT_MS");
        return std::chrono::milliseconds(text != nullptr ? std::strtoll(text, nullptr, 10) : 1000);
    }();
    return wait;
}

std::chrono::milliseconds AsyncComputeWait() {
    static const std::chrono::milliseconds wait = [] {
        const char* text = std::getenv("APS5_ASYNC_COMPUTE_WAIT_MS");
        return std::chrono::milliseconds(text != nullptr ? std::max<long long>(std::strtoll(text, nullptr, 10), 0) : 500);
    }();
    return wait;
}

SlidingWaitBudget::SlidingWaitBudget(Clock::duration perWait, Clock::duration window, Clock::duration total) : perWait(perWait), window(window), total(total) {}

SlidingWaitBudget::Grant SlidingWaitBudget::Take(Clock::time_point now) {
    std::lock_guard lock(mutex);
    const auto from = now - window;
    std::erase_if(waits, [from](const auto& wait) { return wait.second.second <= from; });
    Clock::duration used{};
    for (const auto& [id, wait] : waits) used += wait.second - std::max(wait.first, from);
    if (used >= total) return {};
    const auto allowed = std::min(perWait, total - used);
    const auto id = nextId++;
    waits.emplace(id, std::make_pair(now, now + allowed));
    return {id, allowed};
}

void SlidingWaitBudget::Finish(const Grant& grant, Clock::time_point end) {
    std::lock_guard lock(mutex);
    const auto found = waits.find(grant.id);
    if (found == waits.end()) return;
    if (end <= found->second.first) waits.erase(found);
    else found->second.second = end;
}

SlidingWaitBudget& ComputePipelineWaitBudget() {
    static auto* budget = new SlidingWaitBudget(AsyncComputeWait(), std::chrono::seconds(10), 2 * AsyncComputeWait());
    return *budget;
}

namespace {

struct BackgroundWaitTag;

struct BackgroundWaitBudget {
    std::mutex mutex;
    std::chrono::steady_clock::time_point windowStart{};
    std::chrono::steady_clock::duration spent{};
};

BackgroundWaitBudget& backgroundWaitBudget() {
    static auto* budget = new BackgroundWaitBudget();
    return *budget;
}

constexpr auto BackgroundWaitWindow = std::chrono::seconds(10);

std::chrono::steady_clock::duration takeBackgroundWait() {
    auto& budget = backgroundWaitBudget();
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(budget.mutex);
    if (now - budget.windowStart >= BackgroundWaitWindow) {
        budget.windowStart = now;
        budget.spent = {};
    }
    const std::chrono::steady_clock::duration allowance = 2 * AsyncDispatchWait();
    return budget.spent < allowance ? std::min<std::chrono::steady_clock::duration>(AsyncDispatchWait(), allowance - budget.spent) : std::chrono::steady_clock::duration{};
}

void spendBackgroundWait(std::chrono::steady_clock::duration waited) {
    auto& budget = backgroundWaitBudget();
    std::lock_guard lock(budget.mutex);
    budget.spent += waited;
}

struct DroppedDispatches {
    std::mutex mutex;
    std::map<std::uint64_t, std::uint64_t> dropped;
};

DroppedDispatches& droppedDispatches() {
    static auto* dropped = new DroppedDispatches();
    return *dropped;
}

}

BackgroundWaitScope::BackgroundWaitScope() : previous(HostThreadSlot<bool, BackgroundWaitTag>::Get()) {
    HostThreadSlot<bool, BackgroundWaitTag>::Set(true);
}

BackgroundWaitScope::~BackgroundWaitScope() {
    HostThreadSlot<bool, BackgroundWaitTag>::Set(previous);
}

bool AwaitBackgroundJob(std::mutex& mutex, const bool& finished, bool& waited) {
    {
        std::lock_guard lock(mutex);
        if (finished) return true;
        if (waited || !HostThreadSlot<bool, BackgroundWaitTag>::Get() || AsyncDispatchWait().count() <= 0) return false;
        waited = true;
    }
    const auto allowed = takeBackgroundWait();
    if (allowed <= std::chrono::steady_clock::duration{}) return false;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + allowed;
    bool done = false;
    while (true) {
        {
            std::lock_guard lock(mutex);
            done = finished;
        }
        if (done || std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    spendBackgroundWait(std::chrono::steady_clock::now() - start);
    return done;
}

void NoteDispatchDropped(std::uint64_t address) {
    auto& dropped = droppedDispatches();
    std::lock_guard lock(dropped.mutex);
    if (dropped.dropped[address]++ == 0) std::fprintf(stderr, "[dispatch] dropping the dispatches of compute shader 0x%llx while its shader is prepared in the background (APS5_ASYNC_DRAW_COMPILE)\n", static_cast<unsigned long long>(address));
}

void NoteDispatchRecorded(std::uint64_t address) {
    auto& dropped = droppedDispatches();
    std::lock_guard lock(dropped.mutex);
    if (dropped.dropped.empty()) return;
    const auto found = dropped.dropped.find(address);
    if (found == dropped.dropped.end()) return;
    std::fprintf(stderr, "[dispatch] compute shader 0x%llx: its shader is prepared after %llu dropped dispatches\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(found->second));
    dropped.dropped.erase(found);
}

namespace {

struct RefusedShaders {
    std::mutex mutex;
    std::set<std::uint64_t> reported;
};

RefusedShaders& refusedShaders() {
    static RefusedShaders shaders;
    return shaders;
}

}

void ReportRefusedDraw(const RefusedDraw& refused) {
    auto& shaders = refusedShaders();
    {
        std::lock_guard lock(shaders.mutex);
        if (!shaders.reported.insert(refused.address).second) return;
    }
    const std::string_view what = refused.what();
    const auto line = what.substr(0, what.find('\n'));
    std::fprintf(stderr, "[gpu] APS5_SKIP_REFUSED_DRAWS: skipping the draws that use shader 0x%llx: %.*s\n", static_cast<unsigned long long>(refused.address), static_cast<int>(line.size()), line.data());
}

bool SkipRefusedDispatches() {
    static const bool skip = std::getenv("APS5_SKIP_REFUSED_DISPATCHES") != nullptr;
    return skip;
}

void ReportRefusedDispatch(std::uint64_t address, std::string_view reason) {
    auto& shaders = refusedShaders();
    {
        std::lock_guard lock(shaders.mutex);
        if (!shaders.reported.insert(address).second) return;
    }
    const auto line = reason.substr(0, reason.find('\n'));
    std::fprintf(stderr, "[gpu] APS5_SKIP_REFUSED_DISPATCHES: skipping the dispatches that use shader 0x%llx: %.*s\n", static_cast<unsigned long long>(address), static_cast<int>(line.size()), line.data());
}

bool RefusedDispatch(std::uint64_t address) {
    if (!SkipRefusedDispatches()) return false;
    auto& shaders = refusedShaders();
    std::lock_guard lock(shaders.mutex);
    return shaders.reported.contains(address);
}

std::size_t MaxComputeSpirvWords() {
    static const std::size_t words = [] {
        const char* text = std::getenv("APS5_MAX_COMPUTE_SPIRV_WORDS");
        return text != nullptr ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{0};
    }();
    return words;
}

std::optional<std::size_t> RefusedShaderCount() {
    if (!SkipRefusedDraws() && !SkipRefusedDispatches()) return std::nullopt;
    auto& shaders = refusedShaders();
    std::lock_guard lock(shaders.mutex);
    return shaders.reported.size();
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
