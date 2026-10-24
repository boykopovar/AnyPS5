#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct Xtime {
    std::int64_t sec;
    std::int64_t nsec;
};

extern "C" {
int APS5_VABI _Mtx_init_nid_postfix(void** handle, int type);
void APS5_VABI _Mtx_destroy_nid_postfix(void** handle);
int APS5_VABI _Mtx_lock_nid_postfix(void** handle);
int APS5_VABI _Mtx_unlock_nid_postfix(void** handle);
int APS5_VABI _Cnd_init_nid_postfix(void** handle);
void APS5_VABI _Cnd_destroy_nid_postfix(void** handle);
int APS5_VABI _Cnd_wait_nid_postfix(void** condition, void** mutex);
int APS5_VABI _Cnd_broadcast_nid_postfix(void** handle);
int APS5_VABI _Cnd_timedwait_nid_postfix(void** condition, void** mutex, const Xtime* target);
}

static void Require(bool value) { if (!value) std::abort(); }

#ifdef _WIN32
struct TimerResolution {
    using SetTimerResolution = LONG(WINAPI*)(ULONG, BOOLEAN, PULONG);
    SetTimerResolution set = nullptr;
    ULONG current = 0;

    TimerResolution() {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll)
            set = reinterpret_cast<SetTimerResolution>(GetProcAddress(ntdll, "NtSetTimerResolution"));
        Require(set != nullptr && set(5000, TRUE, &current) >= 0);
    }

    ~TimerResolution() { set(5000, FALSE, &current); }
};
#endif

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int success = 0;
constexpr int timedout = 2;
constexpr int plain = 0x01;

int main() {
    void* mutex = nullptr;
    void* condition = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    Require(_Cnd_init_nid_postfix(&condition) == success);

    Require(_Mtx_lock_nid_postfix(&mutex) == success);

    const auto now = std::chrono::system_clock::now();
    const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto nowNsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() % 1000000000;

    Xtime pastTime{nowSec - 1, nowNsec};
    Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &pastTime) == timedout);

    Xtime unnormalizedPastTime{nowSec - 2, nowNsec + 1000000000};
    Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &unnormalizedPastTime) == timedout);

    Xtime negativePastTime{nowSec, nowNsec - 2000000000};
    Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &negativePastTime) == timedout);

    const auto timeoutDeadline = std::chrono::system_clock::now() + std::chrono::milliseconds(20);
    const auto timeoutSec = std::chrono::duration_cast<std::chrono::seconds>(timeoutDeadline.time_since_epoch()).count();
    const auto timeoutNsec = std::chrono::duration_cast<std::chrono::nanoseconds>(timeoutDeadline.time_since_epoch()).count() % 1000000000;
    const Xtime timeoutTime{timeoutSec, timeoutNsec};
    while (true) {
        const int result = _Cnd_timedwait_nid_postfix(&condition, &mutex, &timeoutTime);
        if (result == timedout) {
            Require(std::chrono::system_clock::now() >= timeoutDeadline);
            break;
        }
        Require(result == success);
    }

#ifdef _WIN32
    TimerResolution timerResolution;
    std::vector<std::chrono::steady_clock::duration> shortWaits;
    for (int i = 0; i < 31; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(1);
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(deadline.time_since_epoch()).count();
        const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count() % 1000000000;
        const Xtime shortTimeout{seconds, nanoseconds};
        Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &shortTimeout) == timedout);
        shortWaits.push_back(std::chrono::steady_clock::now() - start);
    }
    std::sort(shortWaits.begin(), shortWaits.end());
    Require(shortWaits[shortWaits.size() / 2] < std::chrono::milliseconds(12));
#endif

    const auto unnormalizedDeadline = std::chrono::system_clock::now() + std::chrono::milliseconds(20);
    const auto unnormalizedSec = std::chrono::duration_cast<std::chrono::seconds>(unnormalizedDeadline.time_since_epoch()).count();
    const auto unnormalizedNsec = std::chrono::duration_cast<std::chrono::nanoseconds>(unnormalizedDeadline.time_since_epoch()).count() % 1000000000;
    const Xtime unnormalizedTimeoutTime{unnormalizedSec - 1, unnormalizedNsec + 1000000000};
    while (true) {
        const int result = _Cnd_timedwait_nid_postfix(&condition, &mutex, &unnormalizedTimeoutTime);
        if (result == timedout) {
            Require(std::chrono::system_clock::now() >= unnormalizedDeadline);
            break;
        }
        Require(result == success);
    }

    Xtime futureTime{nowSec + 5, nowNsec};
    bool ready = false;
    std::thread signaler([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(_Mtx_lock_nid_postfix(&mutex) == success);
        ready = true;
        Require(_Cnd_broadcast_nid_postfix(&condition) == success);
        Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    });
    while (!ready) {
        Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &futureTime) == success);
    }
    signaler.join();

    ready = false;
    std::thread untimedSignaler([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(_Mtx_lock_nid_postfix(&mutex) == success);
        ready = true;
        Require(_Cnd_broadcast_nid_postfix(&condition) == success);
        Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    });
    Require(_Cnd_wait_nid_postfix(&condition, &mutex) == success);
    Require(ready);
    untimedSignaler.join();

    constexpr std::int64_t farSeconds = 10000000000;
    const Xtime farTime{farSeconds, 999999999};
    constexpr auto clockSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::duration::max()).count();
    if (farSeconds < clockSeconds) {
        bool farReady = false;
        std::thread farSignaler([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            Require(_Mtx_lock_nid_postfix(&mutex) == success);
            farReady = true;
            Require(_Cnd_broadcast_nid_postfix(&condition) == success);
            Require(_Mtx_unlock_nid_postfix(&mutex) == success);
        });
        while (!farReady) {
            Require(_Cnd_timedwait_nid_postfix(&condition, &mutex, &farTime) == success);
        }
        farSignaler.join();
    } else {
        RequireThrows<std::overflow_error>([&] { _Cnd_timedwait_nid_postfix(&condition, &mutex, &farTime); });
    }

    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    RequireThrows<std::invalid_argument>([&] { _Cnd_timedwait_nid_postfix(&condition, &mutex, nullptr); });
    const Xtime overflowCarryTime{std::numeric_limits<std::int64_t>::max(), 1000000000};
    RequireThrows<std::overflow_error>([&] { _Cnd_timedwait_nid_postfix(&condition, &mutex, &overflowCarryTime); });
    const Xtime overflowSecTime{std::numeric_limits<std::int64_t>::max(), 0};
    RequireThrows<std::overflow_error>([&] { _Cnd_timedwait_nid_postfix(&condition, &mutex, &overflowSecTime); });
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
}
