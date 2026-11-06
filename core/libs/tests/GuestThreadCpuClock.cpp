#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <thread>

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;
static constexpr std::uint32_t PROCESS_CPU_CLOCK = 0xC0000000u;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t BURN_NANOS = 200 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t TICK_MARGIN_NANOS = 50 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t GIVE_UP_NANOS = 10 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

static std::int64_t Nanos(int clockId) {
    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(clockId, &time) == 0);
    Require(time.tv_sec >= 0);
    Require(time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    return time.tv_sec * NANOS_PER_SECOND + time.tv_nsec;
}

static int CpuClock(Pthread thread) {
    int clockId = 0;
    Require(pthread_getcpuclockid_nid_postfix(thread, &clockId) == 0);
    return clockId;
}

static void BurnOwnCpu(std::int64_t nanos) {
    const std::int64_t wallStart = Nanos(GUEST_CLOCK_MONOTONIC);
    const std::int64_t cpuStart = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
    while (Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID) - cpuStart < nanos)
        Require(Nanos(GUEST_CLOCK_MONOTONIC) - wallStart < GIVE_UP_NANOS);
}

static std::atomic<int> workerClock{0};
static std::atomic<bool> workerBurned{false};
static std::atomic<bool> releaseWorker{false};

static void* APS5_VABI Worker(void*) {
    workerClock.store(CpuClock(pthread_self_nid_postfix()));
    BurnOwnCpu(BURN_NANOS);
    workerBurned.store(true);
    while (!releaseWorker.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return nullptr;
}

static void OwnClockFollowsThreadCpuTime() {
    const int own = CpuClock(pthread_self_nid_postfix());
    Require((static_cast<std::uint32_t>(own) & 0xC0000000u) == 0x80000000u);
    Require(CpuClock(pthread_self_nid_postfix()) == own);
    const std::int64_t start = Nanos(own);
    BurnOwnCpu(BURN_NANOS);
    const std::int64_t before = Nanos(own);
    const std::int64_t reference = Nanos(GUEST_CLOCK_THREAD_CPUTIME_ID);
    const std::int64_t after = Nanos(own);
    Require(before - start >= BURN_NANOS - TICK_MARGIN_NANOS);
    Require(before <= reference + TICK_MARGIN_NANOS && reference <= after + TICK_MARGIN_NANOS);
    KernelTimespec ownResolution{-1, -1};
    KernelTimespec threadResolution{-1, -1};
    Require(clock_getres_nid_postfix(own, &ownResolution) == 0);
    Require(clock_getres_nid_postfix(GUEST_CLOCK_THREAD_CPUTIME_ID, &threadResolution) == 0);
    Require(ownResolution.tv_sec == threadResolution.tv_sec && ownResolution.tv_nsec == threadResolution.tv_nsec);
}

static void OtherThreadClockReadsThatThread() {
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Worker, nullptr, "cpu clock worker") == 0);
    const std::int64_t wallStart = Nanos(GUEST_CLOCK_MONOTONIC);
    while (!workerBurned.load()) {
        Require(Nanos(GUEST_CLOCK_MONOTONIC) - wallStart < GIVE_UP_NANOS);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const int clockId = workerClock.load();
    Require(clockId == CpuClock(worker));
    Require(clockId != CpuClock(pthread_self_nid_postfix()));
    const std::int64_t burned = Nanos(clockId);
    Require(burned >= BURN_NANOS - TICK_MARGIN_NANOS);
    BurnOwnCpu(BURN_NANOS);
    Require(Nanos(clockId) - burned <= TICK_MARGIN_NANOS);
    releaseWorker.store(true);
    Require(scePthreadJoin(worker, nullptr) == 0);
    KernelTimespec time{-1, -1};
    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(clockId, &time) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);
    *__error_nid_postfix() = 0;
    Require(sceKernelClockGettime(clockId, &time) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == GUEST_EINVAL);
}

static void ProcessClocksNameOnlyTheGuest() {
    const int self = getpid_nid_postfix();
    const int own = static_cast<int>(PROCESS_CPU_CLOCK | static_cast<std::uint32_t>(self));
    const int other = static_cast<int>(PROCESS_CPU_CLOCK | static_cast<std::uint32_t>(self + 1));
    const std::int64_t start = Nanos(own);
    BurnOwnCpu(BURN_NANOS);
    const std::int64_t before = Nanos(own);
    const std::int64_t reference = Nanos(GUEST_CLOCK_PROCESS_CPUTIME_ID);
    const std::int64_t after = Nanos(own);
    Require(before - start >= BURN_NANOS - TICK_MARGIN_NANOS);
    Require(before <= reference + TICK_MARGIN_NANOS && reference <= after + TICK_MARGIN_NANOS);
    KernelTimespec time{-1, -1};
    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(other, &time) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(time.tv_sec == -1 && time.tv_nsec == -1);
    Require(sceKernelClockGettime(other, &time) == SCE_KERNEL_ERROR_EINVAL);
    KernelTimespec threadResolution{-1, -1};
    Require(clock_getres_nid_postfix(GUEST_CLOCK_THREAD_CPUTIME_ID, &threadResolution) == 0);
    for (const int clockId : {own, other}) {
        KernelTimespec resolution{-1, -1};
        Require(clock_getres_nid_postfix(clockId, &resolution) == 0);
        Require(resolution.tv_sec == threadResolution.tv_sec && resolution.tv_nsec == threadResolution.tv_nsec);
    }
}

int main() {
    int clockId = 0;
    Require(pthread_getcpuclockid_nid_postfix(nullptr, &clockId) == GUEST_EINVAL);
    Require(pthread_getcpuclockid_nid_postfix(pthread_self_nid_postfix(), nullptr) == GUEST_EFAULT);
    OwnClockFollowsThreadCpuTime();
    OtherThreadClockReadsThatThread();
    ProcessClocksNameOnlyTheGuest();
}
