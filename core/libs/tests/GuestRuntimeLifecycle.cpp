#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <thread>
#include <source_location>
#include <pthread.h>

namespace {
struct Event { std::size_t args; const void* argp; };
std::array<Event, 8> events{};
std::atomic<unsigned> count{0};
std::atomic<bool> entered{false}, releaseEntry{false};
bool concurrent = false, reentrant = false;
thread_local bool isLifecycleWaiter = false;
std::atomic<bool> waiterObserved{false};
std::atomic<unsigned> waiterReturns{0};
const char* modulePath = nullptr;
std::int32_t reentrantHandle = -1;
void Require(bool ok, const std::source_location& where = std::source_location::current()) {
    if (ok) return;
    std::fprintf(stderr, "guest_runtime_lifecycle: failed at %u\n", where.line());
    std::abort();
}
}

extern "C" int pthread_cond_wait(pthread_cond_t* condition, pthread_mutex_t* mutex) {
    using Wait = int (*)(pthread_cond_t*, pthread_mutex_t*);
    static const auto actual = reinterpret_cast<Wait>(::dlsym(RTLD_NEXT, "pthread_cond_wait"));
    Require(actual != nullptr);
    if (isLifecycleWaiter) waiterObserved.store(true, std::memory_order_release);
    const int result = actual(condition, mutex);
    if (isLifecycleWaiter) waiterReturns.fetch_add(1, std::memory_order_relaxed);
    return result;
}

extern "C" void APS5_VABI RecordArgs(std::size_t args, const void* argp) {
    const auto index = count.fetch_add(1);
    Require(index < events.size());
    events[index] = {args, argp};
    if (index == 0 && reentrant) {
        reentrantHandle = sceKernelLoadStartModule(modulePath, 0, nullptr, 0, nullptr, nullptr);
        Require(reentrantHandle > 0 && count.load() == 1);
    }
    if (index == 0 && concurrent) {
        entered.store(true, std::memory_order_release);
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!releaseEntry.load(std::memory_order_acquire)) {
            Require(std::chrono::steady_clock::now() < limit);
            std::this_thread::yield();
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 3);
    concurrent = std::strcmp(argv[2], "concurrent") == 0;
    reentrant = std::strcmp(argv[2], "reentrant") == 0;
    modulePath = argv[1];
    void* preload = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!preload) std::fprintf(stderr, "%s\n", ::dlerror());
    Require(preload && count.load() == 0);
    if (std::strcmp(argv[2], "never-start") == 0) {
        Require(::dlclose(preload) == 0 && count.load() == 0);
        return 0;
    }
    const std::uint64_t payload = 0xfeed0123;
    const bool zeroStart = std::strcmp(argv[2], "zero-start") == 0;
    const std::size_t requestedSize = zeroStart ? 0 : sizeof(payload);
    const void* requestedPointer = zeroStart ? nullptr : &payload;
    int result = -1;
    std::int32_t first = -1, second = -1;
    if (concurrent) {
        std::thread one([&] { first = sceKernelLoadStartModule(argv[1], requestedSize, requestedPointer, 0, nullptr, &result); });
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!entered.load(std::memory_order_acquire)) {
            Require(std::chrono::steady_clock::now() < limit);
            std::this_thread::yield();
        }
        std::thread two([&] {
            isLifecycleWaiter = true;
            second = sceKernelLoadStartModule(argv[1], 0, nullptr, 0, nullptr, nullptr);
            isLifecycleWaiter = false;
        });
        while (!waiterObserved.load(std::memory_order_acquire)) {
            Require(std::chrono::steady_clock::now() < limit);
            std::this_thread::yield();
        }
        Require(count.load() == 1);
        releaseEntry.store(true, std::memory_order_release);
        one.join();
        two.join();
        Require(waiterReturns.load() > 0);
        std::fprintf(stderr, "Observed lifecycle waiter before initializer release; actual condition-wait returns=%u\n", waiterReturns.load());
    } else {
        first = sceKernelLoadStartModule(argv[1], requestedSize, requestedPointer, 0, nullptr, &result);
        second = sceKernelLoadStartModule(argv[1], 0, nullptr, 0, nullptr, nullptr);
    }
    std::fprintf(stderr, "first=%d second=%d result=%x count=%u\n", first, second, result, count.load());
    if (first < 0) std::fprintf(stderr, "loader error: %s\n", dlerror_nid_postfix());
    Require(first > 0 && second > 0 && first != second && result == 0x5a17);
    Require(count.load() == 2 && events[0].args == requestedSize && events[0].argp == requestedPointer);
    Require(events[1].args == 0xa1 && events[1].argp == nullptr);
    Require(sceKernelStopUnloadModule(first, 0, nullptr, 0, nullptr, nullptr) == 0 && count.load() == 2);
    Require(sceKernelStopUnloadModule(second, 0, nullptr, 0, nullptr, nullptr) == 0 && count.load() == 2);
    if (reentrant) {
        Require(reentrantHandle != first && reentrantHandle != second);
        Require(::dlclose(preload) == 0 && count.load() == 2);
        Require(sceKernelStopUnloadModule(reentrantHandle, 0, nullptr, 0, nullptr, nullptr) == 0 && count.load() == 4);
    } else {
        Require(::dlclose(preload) == 0 && count.load() == 4);
    }
    Require(events[2].args == 0xf1 && events[2].argp == nullptr);
    Require(events[3].args == 0xfe && events[3].argp == nullptr);
    if (std::strcmp(argv[2], "reload") == 0) {
        void* resident = ::dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
        Require(resident == nullptr);
        result = -1;
        const auto reload = sceKernelLoadStartModule(argv[1], sizeof(payload), &payload, 0, nullptr, &result);
        Require(reload > 0 && reload != first && reload != second && result == 0x5a17);
        Require(count.load() == 6 && events[4].args == sizeof(payload) && events[4].argp == &payload);
        Require(events[5].args == 0xa1 && events[5].argp == nullptr);
        Require(sceKernelStopUnloadModule(reload, 0, nullptr, 0, nullptr, nullptr) == 0 && count.load() == 8);
        Require(events[6].args == 0xf1 && events[6].argp == nullptr);
        Require(events[7].args == 0xfe && events[7].argp == nullptr);
        Require(::dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD) == nullptr);
    }
}
