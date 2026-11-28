#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int POSIX_EINVAL = 22;

void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

struct WaiterContext {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    int waitingCount = 0;
    int wokenCount = 0;
    int allowedWakes = 0;
};

void* APS5_VABI WaiterThread(void* arg) {
    auto* ctx = static_cast<WaiterContext*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&ctx->mutex) == 0);
    ctx->waitingCount++;
    while (ctx->wokenCount >= ctx->allowedWakes) {
        Require(pthread_cond_wait_nid_postfix(&ctx->cond, &ctx->mutex) == 0);
    }
    ctx->wokenCount++;
    Require(pthread_mutex_unlock_nid_postfix(&ctx->mutex) == 0);
    return nullptr;
}

void TestOneAtATimeWake() {
    WaiterContext ctx;
    Require(pthread_mutex_init_nid_postfix(&ctx.mutex, nullptr) == 0);
    Require(pthread_cond_init_nid_postfix(&ctx.cond, nullptr) == 0);

    Pthread waiter1 = nullptr;
    Pthread waiter2 = nullptr;
    Require(scePthreadCreate(&waiter1, nullptr, WaiterThread, &ctx, nullptr) == 0);
    Require(scePthreadCreate(&waiter2, nullptr, WaiterThread, &ctx, nullptr) == 0);

    for (;;) {
        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        const bool ready = (ctx.waitingCount == 2);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);
        if (ready) break;
        scePthreadYield();
    }

    Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
    ctx.allowedWakes = 1;
    Require(pthread_cond_signal_nid_postfix(&ctx.cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);

    for (;;) {
        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        const bool firstWoken = (ctx.wokenCount == 1);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);
        if (firstWoken) break;
        scePthreadYield();
    }

    Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
    ctx.allowedWakes = 2;
    Require(pthread_cond_signal_nid_postfix(&ctx.cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);

    for (;;) {
        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        const bool bothWoken = (ctx.wokenCount == 2);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);
        if (bothWoken) break;
        scePthreadYield();
    }

    Require(scePthreadJoin(waiter1, nullptr) == 0);
    Require(scePthreadJoin(waiter2, nullptr) == 0);

    Require(pthread_cond_destroy_nid_postfix(&ctx.cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&ctx.mutex) == 0);
}

} // namespace

int main() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    PthreadCondattr attr = nullptr;
    const KernelTimespec abstime{100, 0};

    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_init_nid_postfix(nullptr, &attr) == POSIX_EINVAL);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signal_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(nullptr, &mutex) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(&cond, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &abstime) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &abstime) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, &mutex, 1000) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 1000) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, nullptr, 1000) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(nullptr, reinterpret_cast<Pthread>(1)) == POSIX_EINVAL);
    Require(pthread_condattr_init_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == POSIX_EINVAL);

    PthreadCond uninitCond = nullptr;
    Require(pthread_cond_destroy_nid_postfix(&uninitCond) == 0);

    PthreadCond zeroCond = nullptr;
    Require(pthread_cond_signal_nid_postfix(&zeroCond) == 0);
    Require(zeroCond != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&zeroCond) == 0);

    PthreadCond zeroBroadcastCond = nullptr;
    Require(pthread_cond_broadcast_nid_postfix(&zeroBroadcastCond) == 0);
    Require(zeroBroadcastCond != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&zeroBroadcastCond) == 0);

    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(pthread_cond_init_nid_postfix(&cond, &attr) == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);

    TestOneAtATimeWake();
    return 0;
}
