#include "prx/libc/include/CallTrace.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <thread>
#include <vector>

struct Block {
    std::uint64_t words[4];
};

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Call trace check failed at line %u\n", location.line());
        std::abort();
    }
}

static std::uint64_t APS5_VABI Integers(std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t d, std::uint64_t e, std::uint64_t f, std::uint64_t g, std::uint64_t h) {
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h;
}

static double APS5_VABI Doubles(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i + 10 * j;
}

static double APS5_VABI Mixed(int a, double b, const char* c, float d, std::uint64_t e) {
    return a + b + static_cast<double>(std::strlen(c)) + d + static_cast<double>(e);
}

static double APS5_VABI Variadic(int count, ...) {
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, count);
    double sum = 0;
    for (int index = 0; index < count; ++index) {
        sum += static_cast<double>(__builtin_va_arg(arguments, std::uint64_t)) * (index + 1);
        sum += __builtin_va_arg(arguments, double) * (index + 1);
    }
    __builtin_sysv_va_end(arguments);
    return sum;
}

static Block APS5_VABI Blocks(std::uint64_t seed) {
    return {{seed, seed + 1, seed + 2, seed + 3}};
}

static std::uint64_t CurrentThread() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return static_cast<std::uint64_t>(gettid());
#endif
}

static const void* ImageOf(const void* address) {
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory)) return nullptr;
    return memory.AllocationBase;
#else
    Dl_info info{};
    if (dladdr(address, &info) == 0) return nullptr;
    return info.dli_fbase;
#endif
}

template <typename TFunction>
static TFunction* Traced(TFunction* function, const char* name, const char* library) {
    return reinterpret_cast<TFunction*>(TraceImport_nid_no_patch(reinterpret_cast<void*>(function), name, library));
}

int main(int argc, char** argv) {
#ifdef _WIN32
    Require(_putenv_s("APS5_TRACE_CALLS", "libTraced,libOther.prx") == 0);
#else
    Require(setenv("APS5_TRACE_CALLS", "libTraced,libOther.prx", 1) == 0);
#endif
    if (argc > 1 && (std::strcmp(argv[1], "abort") == 0 || std::strcmp(argv[1], "abort-race") == 0)) {
#ifndef _WIN32
        Require(std::signal(SIGABRT, [](int) { _exit(3); }) != SIG_ERR);
#endif
        const auto first = Traced(&Integers, "Integers", "libTraced.prx");
        const auto second = Traced(&Blocks, "Blocks", "libOther.prx");
        Require(first(1, 2, 3, 4, 5, 6, 7, 8) == 204 && second(7).words[3] == 10);
#ifdef _WIN32
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
        std::atomic<bool> locked{false};
        std::thread([&locked] {
            flockfile(stderr);
            locked.store(true);
            for (;;) pause();
        }).detach();
        while (!locked.load()) std::this_thread::yield();
        if (std::strcmp(argv[1], "abort-race") == 0) {
            for (int call = 0; call < 100; ++call) Require(first(1, 2, 3, 4, 5, 6, 7, 8) == 204);
            static std::atomic<bool> go{false};
            std::thread([] {
                while (!go.load()) {
                }
                std::abort();
            }).detach();
            go.store(true);
        }
#endif
        std::abort();
    }
    CallTraceEntry entries[16];
    CallTraceCount counts[8];
    std::uint64_t total = 1;
    Require(CopyCallTrace_nid_no_patch(entries, 16) == 0);
    Require(CopyCallCounts_nid_no_patch(counts, 8, &total) == 0 && total == 0);
    Require(Traced(&Integers, "Integers", "libUnselected.prx") == &Integers);
    Require(TraceImport_nid_no_patch(nullptr, "Integers", "libTraced.prx") == nullptr);
    Require(CopyCallTrace_nid_no_patch(entries, 16) == 0);

    const auto integers = Traced(&Integers, "Integers", "libTraced.prx");
    const auto doubles = Traced(&Doubles, "Doubles", "libTraced");
    const auto mixed = Traced(&Mixed, "Mixed", "libOther.prx");
    const auto variadic = Traced(&Variadic, "Variadic", "libOther");
    const auto blocks = Traced(&Blocks, "Blocks", "libTraced.prx");
    Require(integers != &Integers && doubles != &Doubles && mixed != &Mixed && variadic != &Variadic && blocks != &Blocks);
    Require(Traced(&Integers, "Alias", "libOther.prx") == integers);

    Require(integers(1, 2, 3, 4, 5, 6, 7, 8) == Integers(1, 2, 3, 4, 5, 6, 7, 8));
    Require(doubles(1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5) == Doubles(1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5));
    Require(mixed(-3, 0.25, "trace", 1.5f, 1ull << 40) == Mixed(-3, 0.25, "trace", 1.5f, 1ull << 40));
    const double expected = Variadic(8, 1ull, 0.5, 2ull, 1.5, 3ull, 2.5, 4ull, 3.5, 5ull, 4.5, 6ull, 5.5, 7ull, 6.5, 8ull, 7.5);
    Require(variadic(8, 1ull, 0.5, 2ull, 1.5, 3ull, 2.5, 4ull, 3.5, 5ull, 4.5, 6ull, 5.5, 7ull, 6.5, 8ull, 7.5) == expected);
    const Block block = blocks(40);
    Require(block.words[0] == 40 && block.words[1] == 41 && block.words[2] == 42 && block.words[3] == 43);

    const char* const names[] = {"Integers", "Doubles", "Mixed", "Variadic", "Blocks"};
    const char* const libraries[] = {"libTraced.prx", "libTraced", "libOther.prx", "libOther", "libTraced.prx"};
    const void* image = ImageOf(reinterpret_cast<const void*>(&main));
    Require(image != nullptr);
    Require(CopyCallTrace_nid_no_patch(entries, 16) == 5);
    for (std::uint64_t index = 0; index < 5; ++index) {
        Require(entries[index].sequence == index + 1 && entries[index].thread == CurrentThread());
        Require(std::strcmp(entries[index].name, names[index]) == 0 && std::strcmp(entries[index].library, libraries[index]) == 0);
        Require(ImageOf(reinterpret_cast<const void*>(entries[index].returnAddress)) == image);
    }
    Require(CopyCallTrace_nid_no_patch(entries, 2) == 2 && entries[0].sequence == 4 && entries[1].sequence == 5);
    Require(CopyCallCounts_nid_no_patch(counts, 8, &total) == 5 && total == 5);
    for (std::size_t index = 0; index < 5; ++index) {
        Require(counts[index].calls == 1 && std::strcmp(counts[index].name, names[index]) == 0 && std::strcmp(counts[index].library, libraries[index]) == 0);
    }
    Require(doubles(1, 1, 1, 1, 1, 1, 1, 1, 1, 1) == 55 && blocks(1).words[0] == 1 && blocks(2).words[0] == 2);
    Require(CopyCallCounts_nid_no_patch(counts, 2, nullptr) == 2);
    Require(counts[0].calls == 3 && std::strcmp(counts[0].name, "Blocks") == 0 && counts[1].calls == 2 && std::strcmp(counts[1].name, "Doubles") == 0);
    Require(CopyCallCounts_nid_no_patch(nullptr, 8, &total) == 0 && total == 8);
    Require(CopyThreadCallTrace_nid_no_patch(CurrentThread(), entries, 16) == 8);
    for (std::uint64_t index = 0; index < 8; ++index) Require(entries[index].sequence == index + 1 && entries[index].thread == CurrentThread());
    Require(CopyThreadCallTrace_nid_no_patch(CurrentThread(), entries, 2) == 2 && entries[0].sequence == 7 && entries[1].sequence == 8);
    Require(std::strcmp(entries[1].name, "Blocks") == 0);
    Require(CopyThreadCallTrace_nid_no_patch(CurrentThread() + 1, entries, 16) == 0);
    Require(CopyThreadCallTrace_nid_no_patch(CurrentThread(), nullptr, 16) == 0 && CopyThreadCallTrace_nid_no_patch(CurrentThread(), entries, 0) == 0);

    constexpr std::uint64_t Workers = 4;
    constexpr std::uint64_t Calls = 20000;
    std::vector<std::thread> workers;
    std::uint64_t workerThreads[Workers]{};
    for (std::uint64_t worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([integers, worker, &workerThreads] {
            workerThreads[worker] = CurrentThread();
            for (std::uint64_t call = 0; call < Calls; ++call) Require(integers(worker, call, 1, 2, 3, 4, 5, 6) == Integers(worker, call, 1, 2, 3, 4, 5, 6));
        });
    }
    for (auto& worker : workers) worker.join();
    constexpr std::size_t Window = 65536;
    const auto recent = std::make_unique<CallTraceEntry[]>(Window);
    Require(CopyCallTrace_nid_no_patch(recent.get(), Window) == Window);
    Require(recent[Window - 1].sequence == 8 + Workers * Calls && recent[0].sequence == 8 + Workers * Calls - Window + 1);
    for (std::size_t index = 0; index < Window; ++index) Require(std::strcmp(recent[index].name, "Integers") == 0);
    Require(CopyThreadCallTrace_nid_no_patch(CurrentThread(), entries, 16) == 0);
    const auto own = std::make_unique<CallTraceEntry[]>(Window);
    std::size_t owned = 0;
    for (const std::uint64_t thread : workerThreads) {
        std::size_t expected = 0;
        for (std::size_t index = 0; index < Window; ++index) expected += recent[index].thread == thread;
        const std::size_t count = CopyThreadCallTrace_nid_no_patch(thread, own.get(), Window);
        Require(count == expected);
        for (std::size_t index = 0; index < count; ++index) Require(own[index].thread == thread && (index == 0 || own[index - 1].sequence < own[index].sequence));
        if (count != 0) Require(CopyThreadCallTrace_nid_no_patch(thread, entries, 1) == 1 && entries[0].sequence == own[count - 1].sequence);
        owned += count;
    }
    Require(owned == Window);
    Require(CopyCallCounts_nid_no_patch(counts, 1, &total) == 1 && total == 8 + Workers * Calls);
    Require(counts[0].calls == 1 + Workers * Calls && std::strcmp(counts[0].name, "Integers") == 0);
}
