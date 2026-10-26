#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
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
static std::chrono::steady_clock::duration elapsed{};

static void* APS5_VABI Sleeper(void*) {
    if (disabled) Require(scePthreadSetcancelstate(1, nullptr) == 0);
    ready.store(true);
    ready.notify_all();
    if (pending) release.wait(false);
    const auto start = std::chrono::steady_clock::now();
    Require(sleep_nid_postfix(seconds) == 0);
    elapsed = std::chrono::steady_clock::now() - start;
    returned.store(true);
    if (disabled) {
        Require(scePthreadSetcancelstate(0, nullptr) == 0);
        scePthreadTestcancel();

        Require(false);
    }
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

static bool WaitForSleep(Pthread thread) {
    const auto limit = std::chrono::steady_clock::now() + ObservationLimit;
    for (;;) {
        {
            std::lock_guard lock(thread->cancelLock);
            if (thread->cancelWait != nullptr) return true;
        }
        if (std::chrono::steady_clock::now() >= limit) return false;
        std::this_thread::yield();
    }
}

int main(int count, char** arguments) {
    Require(count == 2);
    const std::string_view mode(arguments[1]);
    disabled = mode == "disabled";
    const bool zero = mode == "pending";
    pending = zero || disabled;
    const bool normal = mode == "normal";
    Require(mode == "blocked" || disabled || pending || normal);
    if (disabled || normal) seconds = 1;
    if (zero) seconds = 0;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Sleeper, nullptr, nullptr) == 0);
    ready.wait(false);
    bool wasSleeping = false;
    if (!normal) {
        if (!pending) wasSleeping = WaitForSleep(thread);
        Require(scePthreadCancel(thread) == 0);
        release.store(true);
        release.notify_all();
    }
    const bool completed = WaitForCompletion(thread);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == 0);
    if (!completed) std::fputs("sleep stayed blocked after cancellation\n", stderr);
    Require(completed);
    if (normal) {
        Require(result == nullptr && returned.load());
    } else {
        Require(result == reinterpret_cast<void*>(std::uintptr_t{1}));
        Require(returned.load() == disabled);
        if (!pending) Require(wasSleeping);
    }
    if (normal || disabled) Require(elapsed >= std::chrono::seconds(1));
}
