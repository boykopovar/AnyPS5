#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "font backoff check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

std::uint64_t ThreadCpuNanos() {
#ifdef _WIN32
    FILETIME creation{}, exit{}, kernel{}, user{};
    Require(GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) != 0);
    const auto ticks = [](FILETIME value) {
        return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    return (ticks(kernel) + ticks(user)) * 100ULL;
#else
    timespec value{};
    Require(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0);
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(value.tv_nsec);
#endif
}

void RequireCpuBudget(std::uint64_t cpuNanos, std::uint64_t wallNanos) {
    Require(wallNanos > 0);
    std::printf("format query calling-thread CPU %.1f%% (limit <= 40%%)\n", 100.0 * static_cast<double>(cpuNanos) / static_cast<double>(wallNanos));
    Require(cpuNanos <= wallNanos * 40 / 100);
}

}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    FontLibrary library = nullptr;
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    auto* native = static_cast<Font::FontLibNative*>(library);
    auto word = std::atomic_ref<std::uint32_t>(native->lock_word);
    const auto previous = word.fetch_or(Font::LOCK_BIT, std::memory_order_acq_rel);
    Require((previous & Font::LOCK_BIT) == 0);
    std::atomic<bool> entered{false};
    std::atomic<bool> completed{false};
    int result = -1;
    std::uint64_t cpuNanos = 0;
    std::uint64_t wallNanos = 0;
    std::thread query([&] {
        const auto cpuStart = ThreadCpuNanos();
        const auto wallStart = std::chrono::steady_clock::now();
        entered.store(true, std::memory_order_release);
        result = sceFontFtSupportTrueType(library);
        cpuNanos = ThreadCpuNanos() - cpuStart;
        wallNanos = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - wallStart).count());
        completed.store(true, std::memory_order_release);
    });
    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const bool blocked = !completed.load(std::memory_order_acquire);
    word.store(previous, std::memory_order_release);
    query.join();
    Require(blocked && result == SCE_FONT_OK);
    Require(word.load(std::memory_order_acquire) == previous);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    RequireCpuBudget(cpuNanos, wallNanos);
    std::puts("font format query waits for the library lock and restores its state");
}
