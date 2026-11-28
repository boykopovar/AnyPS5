#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include "prx/libSceAvPlayer/include/Api.hpp"
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

static void Require(bool value) { if (!value) std::abort(); }

struct CapturedLog {
    void* userData;
    int calls;
    char message[128];
};

static CapturedLog Captured;

static int APS5_VABI CaptureLog(void* user_data, const char* format, VaList* arguments) {
    CapturedLog* log = static_cast<CapturedLog*>(user_data);
    log->userData = user_data;
    ++log->calls;
#ifdef _WIN32
    LibcDetail::FormatWindows(log->message, sizeof(log->message), format, arguments);
#else
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(arguments));
    std::vsnprintf(log->message, sizeof(log->message), format, copy);
    va_end(copy);
#endif
    return 42;
}

static int APS5_VABI PrintFormatted(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
    return result;
}

static int CallbackTokenA;
static int CallbackTokenB;
static std::atomic<int> CallbackStateMismatches{0};
static std::atomic<int> ConcurrentCallbackCalls{0};

static int APS5_VABI CallbackA(void* user_data, const char*, VaList*) {
    if (user_data != &CallbackTokenA) CallbackStateMismatches.fetch_add(1, std::memory_order_relaxed);
    ConcurrentCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

static int APS5_VABI CallbackB(void* user_data, const char*, VaList*) {
    if (user_data != &CallbackTokenB) CallbackStateMismatches.fetch_add(1, std::memory_order_relaxed);
    ConcurrentCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    return 2;
}

static void CheckConcurrentCallbackReplacement() {
    Require(sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CallbackA), &CallbackTokenA) == 0);
    constexpr int readerCount = 4;
    constexpr int iterationsPerReader = 500000;
    std::atomic<bool> start{false};
    std::atomic<int> readersRemaining{readerCount};

    std::thread writer([&] {
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        int iteration = 0;
        while (readersRemaining.load(std::memory_order_acquire) != 0) {
            if ((iteration++ & 1) == 0) {
                sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CallbackA), &CallbackTokenA);
            } else {
                sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CallbackB), &CallbackTokenB);
            }
            if ((iteration & 0x3fff) == 0) std::this_thread::yield();
        }
    });

    std::array<std::thread, readerCount> readers;
    for (auto& reader : readers) {
        reader = std::thread([&] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int iteration = 0; iteration < iterationsPerReader; ++iteration) {
                const int result = sceAvPlayerPrintf("callback-state");
                Require(result == 1 || result == 2);
            }
            readersRemaining.fetch_sub(1, std::memory_order_release);
        });
    }

    start.store(true, std::memory_order_release);
    writer.join();
    for (auto& reader : readers) reader.join();

    Require(ConcurrentCallbackCalls.load(std::memory_order_relaxed) == readerCount * iterationsPerReader);
    Require(CallbackStateMismatches.load(std::memory_order_relaxed) == 0);
    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
}

static int ReentrantToken;

static int APS5_VABI ReentrantCallback(void* user_data, const char*, VaList*) {
    Require(user_data == &ReentrantToken);
    Require(sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CallbackB), &CallbackTokenB) == 0);
    return 3;
}

static void CheckReentrantCallbackReplacement() {
    Require(sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&ReentrantCallback), &ReentrantToken) == 0);
    Require(sceAvPlayerPrintf("reentrant") == 3);
    Require(sceAvPlayerPrintf("after-reentrant") == 2);
    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
}

int main() {
    CheckConcurrentCallbackReplacement();
    CheckReentrantCallbackReplacement();
    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
    Require(sceAvPlayerPrintf("fallback %d\n", 1) > 0);

    Captured = {};
    Require(sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CaptureLog), &Captured) == 0);
    Require(sceAvPlayerPrintf("callback %d", 7) == 42);
    Require(Captured.calls == 1);
    Require(Captured.userData == &Captured);
    Require(std::strcmp(Captured.message, "callback 7") == 0);

    Require(PrintFormatted("vcallback %s", "ok") == 42);
    Require(Captured.calls == 2);
    Require(Captured.userData == &Captured);
    Require(std::strcmp(Captured.message, "vcallback ok") == 0);

    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
    Require(sceAvPlayerPrintf("fallback %d\n", 2) > 0);
    Require(PrintFormatted("fallback %s\n", "ok") > 0);

    Require(sceAvPlayerPrintf(nullptr) == -1);
    Require(PrintFormatted(nullptr) == -1);
}
