#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {

void KernelWakeSleepers_nid_no_patch();
void KernelNoteGpuSubmit_nid_no_patch();
}

static constexpr KernelUseconds WAKEABLE_MICROS = 1000;
static constexpr KernelUseconds LONG_MICROS = 2000;
static constexpr unsigned EARLY_WAKES_PER_RUN = 8;
static constexpr unsigned TRIES = 20;
static constexpr auto EARLY = std::chrono::microseconds(900);

static void Require(bool value) { if (!value) std::abort(); }

static std::chrono::microseconds Usleep(KernelUseconds microseconds) {
    const auto start = std::chrono::steady_clock::now();
    Require(sceKernelUsleep_nid_postfix(microseconds) == 0);
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
}

static void Idle(std::chrono::microseconds length) {
    const auto until = std::chrono::steady_clock::now() + length;
    while (std::chrono::steady_clock::now() < until) std::this_thread::yield();
}

static void Spin(std::chrono::microseconds length) {
    const auto until = std::chrono::steady_clock::now() + length;
    while (std::chrono::steady_clock::now() < until) {}
}

class Waker {
public:
    Waker() : thread([this] {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif
        while (!stop.load()) {
            KernelWakeSleepers_nid_no_patch();
            started.store(true);
            Spin(std::chrono::microseconds(50));
        }
    }) {
        while (!started.load()) std::this_thread::yield();
    }
    ~Waker() {
        stop.store(true);
        thread.join();
    }

private:
    std::atomic<bool> stop{false};
    std::atomic<bool> started{false};
    std::thread thread;
};

#ifdef _WIN32
static void CoarseWaitsKeepEverySleepFull() {
    Waker waker;
    for (unsigned i = 0; i < 12; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}
#endif

static void SleepWithoutWakeIsFull() {
    for (unsigned i = 0; i < 4; ++i) Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
}

static void WakeEndsShortSleep() {
    Waker waker;
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void WakeLeavesLongSleep() {
    Waker waker;
    for (unsigned i = 0; i < 4; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        Require(Usleep(LONG_MICROS) >= std::chrono::microseconds(LONG_MICROS));
    }
}

static void RunOfSleepsEndsEarlyAtMostEightTimes() {
    Waker waker;
    KernelNoteGpuSubmit_nid_no_patch();
    unsigned early = 0;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 8; ++i) {
        if (Usleep(WAKEABLE_MICROS) < std::chrono::microseconds(WAKEABLE_MICROS)) ++early;
    }
    Require(early <= EARLY_WAKES_PER_RUN);
    for (unsigned i = 0; i < 4; ++i) Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
}

static void SubmitStartsNewRun() {
    Waker waker;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 4; ++i) Usleep(WAKEABLE_MICROS);
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void GapStartsNewRun() {
    Waker waker;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 4; ++i) Usleep(WAKEABLE_MICROS);
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        Idle(std::chrono::microseconds(WAKEABLE_MICROS + 200));
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void ExhaustRun() {
    KernelNoteGpuSubmit_nid_no_patch();
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 8; ++i) Usleep(WAKEABLE_MICROS);
}

static void LongSleepKeepsRunLimit() {
    Waker waker;
    ExhaustRun();
    for (unsigned i = 0; i < 4; ++i) {
        Require(Usleep(LONG_MICROS) >= std::chrono::microseconds(LONG_MICROS));
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}

static void ZeroSleepCountsAsSleep() {
    Waker waker;
    ExhaustRun();
    for (unsigned i = 0; i < 4; ++i) {
        Idle(std::chrono::microseconds(WAKEABLE_MICROS + 200));
        Require(sceKernelUsleep_nid_postfix(0) == 0);
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}

int main() {
#ifdef _WIN32
    if (std::getenv("APS5_COARSE_TIMED_WAITS") != nullptr) {
        CoarseWaitsKeepEverySleepFull();
        return 0;
    }
#endif
    SleepWithoutWakeIsFull();
    WakeEndsShortSleep();
    WakeLeavesLongSleep();
    RunOfSleepsEndsEarlyAtMostEightTimes();
    SubmitStartsNewRun();
    GapStartsNewRun();
    LongSleepKeepsRunLimit();
    ZeroSleepCountsAsSleep();
    return 0;
}
