#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EOPNOTSUPP = 45;
static constexpr int GUEST_ETIMEDOUT = 60;

static void Require(bool value) {
    if (!value) std::abort();
}

static KernelTimespec FutureDeadline(std::int64_t seconds) {
    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == SCE_OK);
    now.tv_sec += seconds;
    return now;
}

struct Gate {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
};

static void* APS5_VABI WorkerWait(void* argument) {
    auto* gate = static_cast<Gate*>(argument);
    gate->entered.store(true, std::memory_order_release);
    while (!gate->release.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    return reinterpret_cast<void*>(std::uintptr_t{0x12345678});
}

static void* APS5_VABI WorkerInstant(void* argument) {
    return argument;
}

struct JoinerTask {
    Pthread target = nullptr;
    std::atomic<bool> entered{false};
    void* result = reinterpret_cast<void*>(std::uintptr_t{0x55});
    int status = -1;
};

static void* APS5_VABI JoinerThread(void* argument) {
    auto* task = static_cast<JoinerTask*>(argument);
    task->entered.store(true, std::memory_order_release);
    const KernelTimespec deadline = FutureDeadline(5);
    task->status = pthread_timedjoin_np_nid_postfix(task->target, &task->result, &deadline);
    return nullptr;
}

struct DetachSelfContext {
    std::atomic<bool> done{false};
    int detach1 = -1;
    int detach2 = -1;
    int joinSelf = -1;
};

static void* APS5_VABI WorkerDetachSelf(void* argument) {
    auto* ctx = static_cast<DetachSelfContext*>(argument);
    ctx->detach1 = pthread_detach_nid_postfix(pthread_self_nid_postfix());
    ctx->detach2 = pthread_detach_nid_postfix(pthread_self_nid_postfix());
    const KernelTimespec future = FutureDeadline(5);
    ctx->joinSelf = pthread_timedjoin_np_nid_postfix(pthread_self_nid_postfix(), nullptr, &future);
    ctx->done.store(true, std::memory_order_release);
    return nullptr;
}

static void AuditTimedjoinParameters() {
    const KernelTimespec valid = FutureDeadline(5);
    const KernelTimespec invalidSec{-1, 0};
    const KernelTimespec invalidNsecNeg{100, -1};
    const KernelTimespec invalidNsec1B{100, 1000000000};
    const KernelTimespec invalidNsecOver{100, 1000000001};
    const KernelTimespec invalidNsecHuge{100, 2000000000};

    void* canary = reinterpret_cast<void*>(std::uintptr_t{0xbeef});
    Require(pthread_timedjoin_np_nid_postfix(nullptr, nullptr, nullptr) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(nullptr, &canary, nullptr) == GUEST_EINVAL);
    Require(canary == reinterpret_cast<void*>(std::uintptr_t{0xbeef}));
    Require(pthread_timedjoin_np_nid_postfix(pthread_self_nid_postfix(), nullptr, nullptr) == GUEST_EINVAL);

    const Pthread badThread = reinterpret_cast<Pthread>(std::uintptr_t{0xdeadbeef});
    Require(pthread_timedjoin_np_nid_postfix(badThread, nullptr, nullptr) == GUEST_EINVAL);

    for (const KernelTimespec& invalid : {invalidSec, invalidNsecNeg, invalidNsec1B, invalidNsecOver, invalidNsecHuge}) {
        Require(pthread_timedjoin_np_nid_postfix(nullptr, nullptr, &invalid) == GUEST_EINVAL);
        Require(pthread_timedjoin_np_nid_postfix(nullptr, &canary, &invalid) == GUEST_EINVAL);
        Require(canary == reinterpret_cast<void*>(std::uintptr_t{0xbeef}));
        Require(pthread_timedjoin_np_nid_postfix(pthread_self_nid_postfix(), nullptr, &invalid) == GUEST_EINVAL);
        Require(pthread_timedjoin_np_nid_postfix(badThread, nullptr, &invalid) == GUEST_EINVAL);
    }

    Gate gate;
    Pthread live = nullptr;
    Require(pthread_create_nid_postfix(&live, nullptr, WorkerWait, &gate) == 0);
    while (!gate.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, nullptr) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(live, &canary, nullptr) == GUEST_EINVAL);
    Require(canary == reinterpret_cast<void*>(std::uintptr_t{0xbeef}));
    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, &invalidSec) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, &invalidNsecNeg) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, &invalidNsec1B) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, &invalidNsecOver) == GUEST_EINVAL);
    Require(pthread_timedjoin_np_nid_postfix(live, nullptr, &invalidNsecHuge) == GUEST_EINVAL);

    gate.release.store(true, std::memory_order_release);
    void* result = nullptr;
    Require(pthread_timedjoin_np_nid_postfix(live, &result, &valid) == 0);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x12345678}));
}

static void AuditInvalidThreadIdRejection() {
    const KernelTimespec valid = FutureDeadline(5);
    void* value = reinterpret_cast<void*>(std::uintptr_t{0x77});

    Require(pthread_timedjoin_np_nid_postfix(nullptr, &value, &valid) == GUEST_EINVAL);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
    Require(pthread_join_nid_postfix(nullptr, &value) == GUEST_EINVAL);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
    Require(scePthreadJoin(nullptr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));

    for (const std::uintptr_t raw : {std::uintptr_t{1}, std::uintptr_t{0xdeadbeef}, std::uintptr_t{0xbaadf00d}, std::uintptr_t{0x12345678}}) {
        const Pthread bad = reinterpret_cast<Pthread>(raw);
        Require(pthread_timedjoin_np_nid_postfix(bad, &value, &valid) == GUEST_ESRCH);
        Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
        Require(pthread_join_nid_postfix(bad, &value) == GUEST_ESRCH);
        Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
        Require(scePthreadJoin(bad, &value) == SCE_KERNEL_ERROR_ESRCH);
        Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
        Require(pthread_detach_nid_postfix(bad) == GUEST_ESRCH);
        Require(scePthreadDetach(bad) == SCE_KERNEL_ERROR_ESRCH);
    }

    Require(pthread_timedjoin_np_nid_postfix(pthread_self_nid_postfix(), &value, &valid) == GUEST_EDEADLK);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
    Require(pthread_join_nid_postfix(pthread_self_nid_postfix(), &value) == GUEST_EDEADLK);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
    Require(scePthreadJoin(scePthreadSelf(), &value) == SCE_KERNEL_ERROR_EDEADLK);
    Require(value == reinterpret_cast<void*>(std::uintptr_t{0x77}));
}

static void AuditDetachErrorHandling() {
    Require(scePthreadDetach(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_detach_nid_postfix(nullptr) == GUEST_EINVAL);

    Gate gate;
    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, WorkerWait, &gate) == 0);
    while (!gate.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    Require(pthread_detach_nid_postfix(thread) == 0);
    Require(pthread_detach_nid_postfix(thread) == GUEST_EINVAL);
    Require(scePthreadDetach(thread) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec valid = FutureDeadline(5);
    void* result = reinterpret_cast<void*>(std::uintptr_t{0xaa});
    Require(pthread_timedjoin_np_nid_postfix(thread, &result, &valid) == GUEST_EINVAL);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0xaa}));
    Require(pthread_join_nid_postfix(thread, &result) == GUEST_EINVAL);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0xaa}));
    Require(scePthreadJoin(thread, &result) == SCE_KERNEL_ERROR_EINVAL);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0xaa}));

    gate.release.store(true, std::memory_order_release);

    Pthread joinTarget = nullptr;
    Require(pthread_create_nid_postfix(&joinTarget, nullptr, WorkerInstant, reinterpret_cast<void*>(std::uintptr_t{0x42})) == 0);
    void* joinResult = nullptr;
    Require(pthread_timedjoin_np_nid_postfix(joinTarget, &joinResult, &valid) == 0);
    Require(joinResult == reinterpret_cast<void*>(std::uintptr_t{0x42}));

    Require(pthread_detach_nid_postfix(joinTarget) == GUEST_ESRCH);
    Require(scePthreadDetach(joinTarget) == SCE_KERNEL_ERROR_ESRCH);
    Require(pthread_timedjoin_np_nid_postfix(joinTarget, &joinResult, &valid) == GUEST_ESRCH);
    Require(pthread_join_nid_postfix(joinTarget, &joinResult) == GUEST_ESRCH);
    Require(scePthreadJoin(joinTarget, &joinResult) == SCE_KERNEL_ERROR_ESRCH);

    DetachSelfContext selfCtx;
    Pthread selfThread = nullptr;
    Require(pthread_create_nid_postfix(&selfThread, nullptr, WorkerDetachSelf, &selfCtx) == 0);
    while (!selfCtx.done.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    Require(selfCtx.detach1 == 0);
    Require(selfCtx.detach2 == GUEST_EINVAL);
    Require(selfCtx.joinSelf == GUEST_EDEADLK);
}

static void AuditConcurrentJoinAndDetach() {
    Gate gate;
    Pthread target = nullptr;
    Require(pthread_create_nid_postfix(&target, nullptr, WorkerWait, &gate) == 0);
    while (!gate.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    JoinerTask task;
    task.target = target;
    Pthread joiner = nullptr;
    Require(pthread_create_nid_postfix(&joiner, nullptr, JoinerThread, &task) == 0);
    while (!task.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    const KernelTimespec past{0, 0};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        const int probe = pthread_timedjoin_np_nid_postfix(target, nullptr, &past);
        if (probe == GUEST_EOPNOTSUPP) {
            break;
        }
        Require(probe == GUEST_ETIMEDOUT);
        Require(std::chrono::steady_clock::now() < deadline);
        scePthreadYield();
    }

    Require(scePthreadDetach(target) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_detach_nid_postfix(target) == GUEST_EINVAL);

    const KernelTimespec future = FutureDeadline(5);
    Require(pthread_timedjoin_np_nid_postfix(target, nullptr, &future) == GUEST_EOPNOTSUPP);
    Require(pthread_join_nid_postfix(target, nullptr) == GUEST_EOPNOTSUPP);
    Require(scePthreadJoin(target, nullptr) == SCE_KERNEL_ERROR_EOPNOTSUPP);

    gate.release.store(true, std::memory_order_release);

    Require(pthread_join_nid_postfix(joiner, nullptr) == 0);
    Require(task.status == 0);
    Require(task.result == reinterpret_cast<void*>(std::uintptr_t{0x12345678}));

    Require(pthread_timedjoin_np_nid_postfix(target, nullptr, &future) == GUEST_ESRCH);
    Require(pthread_detach_nid_postfix(target) == GUEST_ESRCH);
    Require(scePthreadDetach(target) == SCE_KERNEL_ERROR_ESRCH);
}

static void AuditTimedjoinExpirationAndSuccess() {
    Gate gate;
    Pthread target = nullptr;
    Require(pthread_create_nid_postfix(&target, nullptr, WorkerWait, &gate) == 0);
    while (!gate.entered.load(std::memory_order_acquire)) {
        scePthreadYield();
    }

    const KernelTimespec past{0, 0};
    void* result = reinterpret_cast<void*>(std::uintptr_t{0x99});
    Require(pthread_timedjoin_np_nid_postfix(target, &result, &past) == GUEST_ETIMEDOUT);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x99}));

    gate.release.store(true, std::memory_order_release);

    const KernelTimespec distant{std::numeric_limits<std::int64_t>::max() / 2, 999999999};
    Require(pthread_timedjoin_np_nid_postfix(target, &result, &distant) == 0);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x12345678}));

    Pthread instant = nullptr;
    Require(pthread_create_nid_postfix(&instant, nullptr, WorkerInstant, reinterpret_cast<void*>(std::uintptr_t{0xabcd})) == 0);
    for (int wait = 0; wait < 20; ++wait) {
        scePthreadYield();
    }
    const KernelTimespec future = FutureDeadline(5);
    void* instantResult = nullptr;
    Require(pthread_timedjoin_np_nid_postfix(instant, &instantResult, &future) == 0);
    Require(instantResult == reinterpret_cast<void*>(std::uintptr_t{0xabcd}));

    Pthread instantNullVal = nullptr;
    Require(pthread_create_nid_postfix(&instantNullVal, nullptr, WorkerInstant, nullptr) == 0);
    Require(pthread_timedjoin_np_nid_postfix(instantNullVal, nullptr, &future) == 0);
}

int main() {
    AuditTimedjoinParameters();
    AuditInvalidThreadIdRejection();
    AuditDetachErrorHandling();
    AuditConcurrentJoinAndDetach();
    AuditTimedjoinExpirationAndSuccess();
    return 0;
}
