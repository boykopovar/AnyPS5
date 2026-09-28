#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#endif

namespace AgcDriver {

#ifdef _WIN32
namespace {

// Debug aid: APS5_SAMPLE_WORKER=<file> samples the calling thread every millisecond and, every 20 s,
// writes "module+offset self inclusive" lines to <file> for offline symbolization with nm.
struct Sampler {
    HANDLE target = nullptr;
    std::string path;
    std::mutex mutex;
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> counts;
    std::uint64_t samples = 0;

    void sample() {
        CONTEXT context{};
        context.ContextFlags = CONTEXT_FULL;
        if (SuspendThread(target) == static_cast<DWORD>(-1)) return;
        const bool captured = GetThreadContext(target, &context) != 0;
        std::vector<std::uint64_t> frames;
        if (captured) {
            // Unwinding reads the target's stack while it is suspended; frames without unwind data
            // (JIT/guest code) end the walk.
            for (int depth = 0; depth < 12 && context.Rip != 0; ++depth) {
                frames.push_back(context.Rip);
                DWORD64 imageBase = 0;
                auto* entry = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
                if (entry == nullptr) break;
                PVOID handler = nullptr;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, entry, &context, &handler, &establisher, nullptr);
            }
        }
        ResumeThread(target);
        std::lock_guard lock(mutex);
        ++samples;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            auto& count = counts[frames[i]];
            if (i == 0) ++count.first;
            ++count.second;
        }
    }

    void write() {
        std::lock_guard lock(mutex);
        std::FILE* file = std::fopen(path.c_str(), "w");
        if (file == nullptr) return;
        std::fprintf(file, "# %llu samples\n", static_cast<unsigned long long>(samples));
        for (const auto& [address, count] : counts) {
            HMODULE module = nullptr;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, sizeof(name));
            const char* base = std::strrchr(name, '\\');
            std::fprintf(file, "%s 0x%llx %llu %llu\n", base ? base + 1 : name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)), static_cast<unsigned long long>(count.first), static_cast<unsigned long long>(count.second));
        }
        std::fclose(file);
    }
};

// Debug aid: APS5_SAMPLE_THREADS=<file> samples every thread in the process every 2 ms and, every
// 20 s, writes each busy thread's hottest frames ("module+offset self inclusive") to <file>.
struct ProcessSampler {
    std::string path;
    DWORD self = 0;
    std::map<DWORD, Sampler> threads;
    std::uint64_t rounds = 0;

    std::chrono::steady_clock::time_point enumerated{};

    // The system-wide thread snapshot is slow (tens of ms); the thread list is refreshed every 5 s and
    // the rounds in between only sample the known threads.
    void enumerate() {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD process = GetCurrentProcessId();
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self) continue;
            auto& thread = threads[entry.th32ThreadID];
            if (thread.target == nullptr) thread.target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        }
        CloseHandle(snapshot);
        enumerated = std::chrono::steady_clock::now();
    }

    void sample() {
        if (threads.empty() || std::chrono::steady_clock::now() - enumerated > std::chrono::seconds(5)) enumerate();
        for (auto& [id, thread] : threads) {
            if (thread.target != nullptr) thread.sample();
        }
        ++rounds;
    }

    void write() {
        std::FILE* file = std::fopen(path.c_str(), "w");
        if (file == nullptr) return;
        std::fprintf(file, "# %llu rounds\n", static_cast<unsigned long long>(rounds));
        for (auto& [id, thread] : threads) {
            std::lock_guard lock(thread.mutex);
            std::vector<std::pair<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>>> hot(thread.counts.begin(), thread.counts.end());
            std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
            std::uint64_t leaves = 0;
            for (const auto& [address, count] : hot) leaves += count.first;
            if (leaves == 0) continue;
            std::fprintf(file, "thread %lu samples %llu\n", static_cast<unsigned long>(id), static_cast<unsigned long long>(thread.samples));
            for (std::size_t i = 0; i < hot.size() && i < 40; ++i) {
                const auto address = hot[i].first;
                HMODULE module = nullptr;
                char name[MAX_PATH] = "?";
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module)) GetModuleFileNameA(module, name, sizeof(name));
                const char* base = std::strrchr(name, '\\');
                std::fprintf(file, "  %s 0x%llx %llu %llu\n", base ? base + 1 : name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)), static_cast<unsigned long long>(hot[i].second.first), static_cast<unsigned long long>(hot[i].second.second));
            }
        }
        std::fclose(file);
    }
};

void StartProcessSampler() {
    const char* path = std::getenv("APS5_SAMPLE_THREADS");
    if (path == nullptr) return;
    std::thread([path = std::string(path)] {
        auto* sampler = new ProcessSampler();
        sampler->path = path;
        sampler->self = GetCurrentThreadId();
        auto flushed = std::chrono::steady_clock::now();
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            sampler->sample();
            if (std::chrono::steady_clock::now() - flushed > std::chrono::seconds(20)) {
                sampler->write();
                flushed = std::chrono::steady_clock::now();
            }
        }
    }).detach();
}

}

void StartWorkerSampler() {
    StartProcessSampler();
    const char* path = std::getenv("APS5_SAMPLE_WORKER");
    if (path == nullptr) return;
    auto* sampler = new Sampler();
    sampler->path = path;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &sampler->target, THREAD_ALL_ACCESS, FALSE, 0);
    std::thread([sampler] {
        auto flushed = std::chrono::steady_clock::now();
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sampler->sample();
            if (std::chrono::steady_clock::now() - flushed > std::chrono::seconds(20)) {
                sampler->write();
                flushed = std::chrono::steady_clock::now();
            }
        }
    }).detach();
}
#else
void StartWorkerSampler() {}
#endif

}
