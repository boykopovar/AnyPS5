#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId);
Pthread APS5_VABI pthread_self_nid_postfix(void);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
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
}

int main() {
    int clockId = 0;
    Require(pthread_getcpuclockid_nid_postfix(nullptr, &clockId) == GUEST_EINVAL);
    Require(pthread_getcpuclockid_nid_postfix(pthread_self_nid_postfix(), nullptr) == GUEST_EFAULT);
    OwnClockFollowsThreadCpuTime();
    OtherThreadClockReadsThatThread();
}
