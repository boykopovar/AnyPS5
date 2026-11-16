#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EACCES = 13;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t SLEEP_NANOS = 20 * NANOS_PER_MILLISECOND;

static std::atomic<bool> cancelThreadReady{false};
static std::atomic<bool> cancelThreadRelease{false};
static std::atomic<bool> cancelSleepReturned{false};

static void Require(bool value) {
    if (!value) std::abort();
}

static void PosixRejects(const KernelTimespec* request, int error) {
    *__error_nid_postfix() = GUEST_EACCES;
    Require(__sys_nanosleep_nid_postfix(request, nullptr) == -1);
    Require(*__error_nid_postfix() == error);
}

static void TestRejectsInvalidRequests() {
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec wholeSecondOfNanos{0, NANOS_PER_SECOND};
    const KernelTimespec negativeBoth{-1, -1};
    PosixRejects(&negativeNanos, GUEST_EINVAL);
    PosixRejects(&wholeSecondOfNanos, GUEST_EINVAL);
    PosixRejects(&negativeBoth, GUEST_EINVAL);
    PosixRejects(nullptr, GUEST_EFAULT);
}

static void TestNegativeAndZeroSeconds() {
    *__error_nid_postfix() = GUEST_EACCES;
    KernelTimespec request{-1, 0};
    Require(__sys_nanosleep_nid_postfix(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);

    request = {0, 0};
    Require(__sys_nanosleep_nid_postfix(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);
}

static void TestValidSleep() {
    const KernelTimespec request{0, SLEEP_NANOS};
    KernelTimespec remaining{5, 7};
    *__error_nid_postfix() = GUEST_EACCES;
    const auto start = std::chrono::steady_clock::now();
    Require(__sys_nanosleep_nid_postfix(&request, &remaining) == SCE_OK);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(remaining.tv_sec == 5 && remaining.tv_nsec == 7);
    Require(elapsed >= std::chrono::milliseconds(10));
}

static void* APS5_VABI PendingCancelWorker(void*) {
    cancelThreadReady.store(true);
    cancelThreadReady.notify_all();
    while (!cancelThreadRelease.load()) {
        std::this_thread::yield();
    }
    const KernelTimespec request{0, 20 * NANOS_PER_MILLISECOND};
    Require(__sys_nanosleep_nid_postfix(&request, nullptr) == SCE_OK);
    cancelSleepReturned.store(true);
    return nullptr;
}

static void TestPendingCancelDoesNotFire() {
    cancelThreadReady.store(false);
    cancelThreadRelease.store(false);
    cancelSleepReturned.store(false);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, PendingCancelWorker, nullptr, "nanosleep_cancel_test") == SCE_OK);
    while (!cancelThreadReady.load()) {
        std::this_thread::yield();
    }
    Require(scePthreadCancel(thread) == SCE_OK);
    cancelThreadRelease.store(true);
    cancelThreadRelease.notify_all();

    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(cancelSleepReturned.load());
    Require(result == nullptr);
}

int main() {
    TestRejectsInvalidRequests();
    TestNegativeAndZeroSeconds();
    TestValidSleep();
    TestPendingCancelDoesNotFire();
    return 0;
}
