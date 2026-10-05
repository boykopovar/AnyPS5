#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res);
int APS5_VABI sceKernelClockGettime(KernelClockid clockId, KernelTimespec* tp);
int APS5_VABI sceKernelClockGetres(KernelClockid clockId, KernelTimespec* tp);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);

static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t BURN_NANOS = 200 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t TICK_MARGIN_NANOS = 50 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t GIVE_UP_NANOS = 10 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

static std::int64_t Nanos(int clockId) {
    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(clockId, &time) == SCE_OK);
    Require(time.tv_sec >= 0);
    Require(time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    return time.tv_sec * NANOS_PER_SECOND + time.tv_nsec;
}

static void RequireResolution(int clockId) {
    KernelTimespec resolution{-1, -1};
    Require(clock_getres_nid_postfix(clockId, &resolution) == SCE_OK);
    Require(resolution.tv_sec == 0);
    Require(resolution.tv_nsec > 0 && resolution.tv_nsec < NANOS_PER_SECOND);
    KernelTimespec sceResolution{-1, -1};
    Require(sceKernelClockGetres(clockId, &sceResolution) == SCE_OK);
    Require(sceResolution.tv_sec == resolution.tv_sec && sceResolution.tv_nsec == resolution.tv_nsec);
}

static void RequireNeverDecreases(int clockId) {
    std::int64_t previous = Nanos(clockId);
    for (int i = 0; i < 10000; ++i) {
        const std::int64_t current = Nanos(clockId);
        Require(current >= previous);
        previous = current;
    }
}

static std::atomic<bool> stopBurning{false};

static void* APS5_VABI Burn(void*) {
    while (!stopBurning.load()) {}
    return nullptr;
}

static void BusyThreadAccumulatesCpuTime() {
    const std::int64_t wallStart = Nanos(GUEST_CLOCK_MONOTONIC);
    const std::int64_t cpuStart = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
    std::int64_t cpu = cpuStart;
    while (cpu - cpuStart < BURN_NANOS) {
        Require(Nanos(GUEST_CLOCK_MONOTONIC) - wallStart < GIVE_UP_NANOS);
        const std::int64_t current = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
        Require(current >= cpu);
        cpu = current;
    }
    const std::int64_t wall = Nanos(GUEST_CLOCK_MONOTONIC);
    Require(cpu - cpuStart <= wall - wallStart + TICK_MARGIN_NANOS);
}

static void SleepingThreadAccumulatesNone() {
    const std::int64_t cpuStart = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
    Require(sceKernelUsleep_nid_postfix(200000) == SCE_OK);
    Require(Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID) - cpuStart <= TICK_MARGIN_NANOS);
}

static void ProcessClockCountsOtherThreads() {
    const std::int64_t wallStart = Nanos(GUEST_CLOCK_MONOTONIC);
    const std::int64_t threadStart = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
    const std::int64_t processStart = Nanos(GUEST_CLOCK_PROCESS_CPUTIME_ID);
    Pthread burner = nullptr;
    Require(scePthreadCreate(&burner, nullptr, Burn, nullptr, nullptr) == SCE_OK);
    while (Nanos(GUEST_CLOCK_PROCESS_CPUTIME_ID) - processStart < BURN_NANOS) {
        Require(Nanos(GUEST_CLOCK_MONOTONIC) - wallStart < GIVE_UP_NANOS);
        Require(sceKernelUsleep_nid_postfix(50000) == SCE_OK);
    }
    stopBurning.store(true);
    Require(scePthreadJoin(burner, nullptr) == SCE_OK);
    Require(Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID) - threadStart <= BURN_NANOS / 2);
}

int main() {
    KernelTimespec time{-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_THREAD_CPUTIME_ID, &time) == SCE_OK);
    Require(time.tv_sec >= 0 && time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    Require(sceKernelClockGettime(GUEST_CLOCK_PROCESS_CPUTIME_ID, &time) == SCE_OK);
    Require(time.tv_sec >= 0 && time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    Require(sceKernelClockGettime(GUEST_CLOCK_THREAD_CPUTIME_ID, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGetres(GUEST_CLOCK_PROCESS_CPUTIME_ID, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    RequireResolution(GUEST_CLOCK_THREAD_CPUTIME_ID);
    RequireResolution(GUEST_CLOCK_PROCESS_CPUTIME_ID);
    RequireNeverDecreases(GUEST_CLOCK_THREAD_CPUTIME_ID);
    RequireNeverDecreases(GUEST_CLOCK_PROCESS_CPUTIME_ID);

    BusyThreadAccumulatesCpuTime();
    SleepingThreadAccumulatesNone();
    ProcessClockCountsOtherThreads();
}
