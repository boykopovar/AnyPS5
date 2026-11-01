#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

static constexpr auto ObservationLimit = std::chrono::seconds(3);
static constexpr unsigned LongSleep = 10;
static std::atomic<bool> ready{false};
static std::atomic<bool> release{false};
static std::atomic<bool> returned{false};
static bool disabled = false;
static bool pending = false;
static unsigned seconds = LongSleep;
static std::string_view api = "sleep";
static unsigned cleanupCalls = 0;
static std::chrono::steady_clock::time_point sleepStart{};
static std::chrono::steady_clock::duration elapsed{};

static void APS5_VABI Cleanup(void*) {
    elapsed = std::chrono::steady_clock::now() - sleepStart;
    ++cleanupCalls;
}

static int Sleep() {
    if (api == "sleep") return static_cast<int>(sleep_nid_postfix(seconds));
    if (api == "sce_sleep") return static_cast<int>(sceKernelSleep(seconds));
    if (api == "usleep") return usleep_nid_postfix(seconds * 1000000U);
    if (api == "sce_usleep") return sceKernelUsleep_nid_postfix(seconds * 1000000U);
    const KernelTimespec request{seconds, 0};
    if (api == "nanosleep") return nanosleep_nid_postfix(&request, nullptr);
    if (api == "_nanosleep") return _nanosleep_nid_postfix(&request, nullptr);
    Require(api == "sce_nanosleep");
    return sceKernelNanosleep(&request, nullptr);
}

static void* APS5_VABI Sleeper(void*) {
    if (disabled) Require(scePthreadSetcancelstate(1, nullptr) == 0);
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, nullptr, &cleanup);
    ready.store(true);
    ready.notify_all();
    if (pending) release.wait(false);
    sleepStart = std::chrono::steady_clock::now();
    Require(Sleep() == 0);
    elapsed = std::chrono::steady_clock::now() - sleepStart;
    returned.store(true);
    if (disabled) {
        Require(scePthreadSetcancelstate(0, nullptr) == 0);
        scePthreadTestcancel();

        Require(false);
    }
    __pthread_cleanup_pop_imp_nid_postfix(0);
    return nullptr;
}

static bool WaitForCompletion(Pthread thread) {
    const auto limit = std::chrono::steady_clock::now() + ObservationLimit;
    while (!thread->_finished.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= limit) return false;
        std::this_thread::yield();
    }
    return true;
}

static bool WaitForSleep(Pthread thread, bool hostSleep) {
    const auto limit = std::chrono::steady_clock::now() + ObservationLimit;
    for (;;) {
        {
            std::lock_guard lock(thread->cancelLock);
            if (thread->cancelWait != nullptr) return true;
        }
        if (hostSleep && thread->waitCount.load(std::memory_order_seq_cst) > 0) return true;
        if (thread->_finished.load(std::memory_order_acquire)) return false;
        if (std::chrono::steady_clock::now() >= limit) return false;
        std::this_thread::yield();
    }
}

int main(int count, char** arguments) {
    Require(count == 2 || count == 3);
    if (count == 3) api = arguments[2];
    const std::string_view mode(arguments[1]);
    disabled = mode == "disabled";
    const bool zero = mode == "pending";
    pending = zero || disabled || mode == "pending_long";
    const bool normal = mode == "normal";
    const bool deferred = mode == "deferred";
    Require(mode == "blocked" || disabled || pending || normal || deferred);
    Require(!deferred || api == "sce_usleep" || api == "sce_nanosleep" || api == "sce_sleep");
    if (disabled || normal || deferred) seconds = 1;
    if (zero) seconds = 0;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Sleeper, nullptr, nullptr) == 0);
    const auto readyLimit = std::chrono::steady_clock::now() + ObservationLimit;
    while (!ready.load()) {
        Require(std::chrono::steady_clock::now() < readyLimit);
        std::this_thread::yield();
    }
    bool wasSleeping = false;
    if (!normal) {
        if (!pending) {
            wasSleeping = WaitForSleep(thread, deferred);
            Require(wasSleeping);
        }
        Require(scePthreadCancel(thread) == 0);
        release.store(true);
        release.notify_all();
    }
    const bool completed = WaitForCompletion(thread);
    if (!completed) std::fputs("sleep stayed blocked after cancellation\n", stderr);
    Require(completed);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == 0);
    if (normal) {
        Require(result == nullptr && returned.load());
        Require(cleanupCalls == 0);
    } else {
        Require(result == reinterpret_cast<void*>(std::uintptr_t{1}));
        Require(cleanupCalls == 1);
        Require(returned.load() == disabled);
        if (!pending) Require(wasSleeping);
    }
    if (normal || disabled || deferred) Require(elapsed >= std::chrono::seconds(1));
}
