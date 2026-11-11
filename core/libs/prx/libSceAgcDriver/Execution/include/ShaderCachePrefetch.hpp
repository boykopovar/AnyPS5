#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERCACHEPREFETCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERCACHEPREFETCH_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace AgcDriver {

struct ShaderCachePrefetchResult {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t skipped = 0;
    double milliseconds = 0;
};

inline void LowerPrefetchPriority() {
#ifdef _WIN32
    if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#else
    sched_param parameter{};
    pthread_setschedparam(pthread_self(), SCHED_IDLE, &parameter);
#endif
}

inline ShaderCachePrefetchResult PrefetchShaderCacheEntries(const std::filesystem::path& directory, std::uint64_t byteLimit, unsigned threads, bool background) {
    struct Entry {
        std::filesystem::path path;
        std::uint64_t size;
        std::filesystem::file_time_type time;
    };
    const auto start = std::chrono::steady_clock::now();
    ShaderCachePrefetchResult result;
    std::vector<Entry> entries;
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
        std::error_code entryError;
        if (it->path().extension() != ".bin" || !it->is_regular_file(entryError) || entryError) continue;
        const auto size = it->file_size(entryError);
        if (entryError) continue;
        const auto time = it->last_write_time(entryError);
        if (entryError) continue;
        entries.push_back({it->path(), size, time});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.time != b.time ? a.time > b.time : a.path < b.path; });
    std::vector<const Entry*> chosen;
    std::uint64_t chosenBytes = 0;
    for (const auto& entry : entries) {
        if (entry.size > byteLimit - std::min(byteLimit, chosenBytes)) {
            ++result.skipped;
            continue;
        }
        chosen.push_back(&entry);
        chosenBytes += entry.size;
    }
    std::atomic<std::size_t> next{0};
    std::atomic<std::uint64_t> files{0};
    std::atomic<std::uint64_t> bytes{0};
    const auto work = [&] {
        if (background) LowerPrefetchPriority();
        std::vector<char> buffer(1u << 20u);
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < chosen.size(); index = next.fetch_add(1, std::memory_order_relaxed)) {
            std::ifstream file(chosen[index]->path, std::ios::binary);
            if (!file) continue;
            std::uint64_t read = 0;
            while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || file.gcount() > 0) read += static_cast<std::uint64_t>(file.gcount());
            files.fetch_add(1, std::memory_order_relaxed);
            bytes.fetch_add(read, std::memory_order_relaxed);
        }
    };
    const auto count = std::max(1u, std::min<unsigned>(threads, static_cast<unsigned>(std::max<std::size_t>(chosen.size(), 1u))));
    std::vector<std::thread> workers;
    for (unsigned worker = 1; worker < count; ++worker) workers.emplace_back(work);
    work();
    for (auto& worker : workers) worker.join();
    result.files = files.load(std::memory_order_relaxed);
    result.bytes = bytes.load(std::memory_order_relaxed);
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}

inline void StartShaderCachePrefetch(std::filesystem::path directory, std::uint64_t byteLimit, unsigned threads) {
    static std::atomic<bool> started{false};
    if (byteLimit == 0 || started.exchange(true, std::memory_order_acq_rel)) return;
    std::thread([directory = std::move(directory), byteLimit, threads] {
        const auto result = PrefetchShaderCacheEntries(directory, byteLimit, threads, true);
        std::fprintf(stderr, "[shader-cache-prefetch] read %llu entries (%.1f MiB) into the file cache in %.1f ms; %llu left out by the %llu MiB limit\n", static_cast<unsigned long long>(result.files), static_cast<double>(result.bytes) / (1024.0 * 1024.0), result.milliseconds, static_cast<unsigned long long>(result.skipped), static_cast<unsigned long long>(byteLimit >> 20u));
    }).detach();
}

}

#endif
