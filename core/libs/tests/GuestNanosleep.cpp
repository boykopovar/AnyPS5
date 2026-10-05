#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI _nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int* APS5_VABI __error_nid_postfix();
}

using Nanosleep = int (APS5_VABI *)(const KernelTimespec*, KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

static constexpr int GUEST_EACCES = 13;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t SLEEP_NANOS = 50 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t EARLY_WAKE_MARGIN_NANOS = 5 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t NO_SLEEP_LIMIT_NANOS = 2 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

static std::int64_t MonotonicNanos() {
    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &time) == SCE_OK);
    return time.tv_sec * NANOS_PER_SECOND + time.tv_nsec;
}

static void PosixRejects(Nanosleep sleep, const KernelTimespec* request, int error) {
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(request, nullptr) == -1);
    Require(*__error_nid_postfix() == error);
}

static void PosixRejectsInvalidRequests(Nanosleep sleep) {
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec wholeSecondOfNanos{0, NANOS_PER_SECOND};
    const KernelTimespec negativeBoth{-1, -1};
    PosixRejects(sleep, &negativeNanos, GUEST_EINVAL);
    PosixRejects(sleep, &wholeSecondOfNanos, GUEST_EINVAL);
    PosixRejects(sleep, &negativeBoth, GUEST_EINVAL);
    PosixRejects(sleep, nullptr, GUEST_EFAULT);
}

static void SceRejectsInvalidRequests() {
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec wholeSecondOfNanos{0, NANOS_PER_SECOND};
    const KernelTimespec negativeBoth{-1, -1};
    Require(sceKernelNanosleep(&negativeNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&wholeSecondOfNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negativeBoth, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void ReturnsAtOnce(Nanosleep sleep, KernelTimespec request) {
    const std::int64_t start = MonotonicNanos();
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(MonotonicNanos() - start < NO_SLEEP_LIMIT_NANOS);
}

static void NegativeSecondsDoNotSleep(Nanosleep sleep) {
    ReturnsAtOnce(sleep, {-1, 0});
    ReturnsAtOnce(sleep, {-1, NANOS_PER_SECOND - 1});
    ReturnsAtOnce(sleep, {INT64_MIN, 0});
    ReturnsAtOnce(sleep, {0, 0});
}

static void SleepsForTheRequest(Nanosleep sleep) {
    const KernelTimespec request{0, SLEEP_NANOS};
    const std::int64_t start = MonotonicNanos();
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(MonotonicNanos() - start >= SLEEP_NANOS - EARLY_WAKE_MARGIN_NANOS);
}

int main() {
    PosixRejectsInvalidRequests(nanosleep_nid_postfix);
    PosixRejectsInvalidRequests(_nanosleep_nid_postfix);
    SceRejectsInvalidRequests();

    NegativeSecondsDoNotSleep(nanosleep_nid_postfix);
    NegativeSecondsDoNotSleep(_nanosleep_nid_postfix);
    NegativeSecondsDoNotSleep(sceKernelNanosleep);

    SleepsForTheRequest(nanosleep_nid_postfix);
    SleepsForTheRequest(_nanosleep_nid_postfix);
    SleepsForTheRequest(sceKernelNanosleep);
}
