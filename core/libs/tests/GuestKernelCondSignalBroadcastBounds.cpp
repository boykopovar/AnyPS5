#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct WaiterContext {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> waitingCount{0};
    std::atomic<int> wokenCount{0};
    bool proceed = false;
};

void* APS5_VABI WaiterThread(void* arg) {
    auto* ctx = static_cast<WaiterContext*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&ctx->mutex) == 0);
    ctx->waitingCount.fetch_add(1);
    while (!ctx->proceed) {
        Require(pthread_cond_wait_nid_postfix(&ctx->cond, &ctx->mutex) == 0);
        ctx->wokenCount.fetch_add(1);
        break;
    }
    Require(pthread_mutex_unlock_nid_postfix(&ctx->mutex) == 0);
    return nullptr;
}

void TestParameterBoundsAndNullPointers() {
    Require(pthread_cond_signal_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadCond uninit = nullptr;
    Require(pthread_cond_destroy_nid_postfix(&uninit) == 0);
}

void TestLazyCondAllocation() {
    PthreadCond lazySignal = nullptr;
    Require(pthread_cond_signal_nid_postfix(&lazySignal) == 0);
    Require(lazySignal != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&lazySignal) == 0);

    PthreadCond lazyBroadcast = nullptr;
    Require(pthread_cond_broadcast_nid_postfix(&lazyBroadcast) == 0);
    Require(lazyBroadcast != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&lazyBroadcast) == 0);
}

void TestDestroyedCondPrecedence() {
    PthreadCond destroyed = nullptr;
    Require(pthread_cond_init_nid_postfix(&destroyed, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&destroyed) == 0);

    Require(pthread_cond_signal_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_cond_broadcast_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_cond_destroy_nid_postfix(&destroyed) == PosixEinval);
}

void TestWaiterWakeSemantics() {
    WaiterContext ctx;
    Require(pthread_mutex_init_nid_postfix(&ctx.mutex, nullptr) == 0);
    Require(pthread_cond_init_nid_postfix(&ctx.cond, nullptr) == 0);

    Require(pthread_cond_signal_nid_postfix(&ctx.cond) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&ctx.cond) == 0);

    {
        ctx.waitingCount.store(0);
        ctx.wokenCount.store(0);
        ctx.proceed = false;

        Pthread waiter1 = nullptr;
        Pthread waiter2 = nullptr;
        Require(scePthreadCreate(&waiter1, nullptr, WaiterThread, &ctx, nullptr) == 0);
        Require(scePthreadCreate(&waiter2, nullptr, WaiterThread, &ctx, nullptr) == 0);

        while (ctx.waitingCount.load() != 2) {
            scePthreadYield();
        }

        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        ctx.proceed = true;
        Require(pthread_cond_signal_nid_postfix(&ctx.cond) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);

        while (ctx.wokenCount.load() != 1) {
            scePthreadYield();
        }

        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        Require(pthread_cond_signal_nid_postfix(&ctx.cond) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);

        while (ctx.wokenCount.load() != 2) {
            scePthreadYield();
        }

        Require(scePthreadJoin(waiter1, nullptr) == 0);
        Require(scePthreadJoin(waiter2, nullptr) == 0);
    }

    {
        ctx.waitingCount.store(0);
        ctx.wokenCount.store(0);
        ctx.proceed = false;

        Pthread waiters[3] = {nullptr, nullptr, nullptr};
        for (auto& w : waiters) {
            Require(scePthreadCreate(&w, nullptr, WaiterThread, &ctx, nullptr) == 0);
        }

        while (ctx.waitingCount.load() != 3) {
            scePthreadYield();
        }

        Require(pthread_mutex_lock_nid_postfix(&ctx.mutex) == 0);
        ctx.proceed = true;
        Require(pthread_cond_broadcast_nid_postfix(&ctx.cond) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);

        while (ctx.wokenCount.load() != 3) {
            scePthreadYield();
        }

        for (auto& w : waiters) {
            Require(scePthreadJoin(w, nullptr) == 0);
        }
    }

    Require(pthread_cond_destroy_nid_postfix(&ctx.cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&ctx.mutex) == 0);
}

} // namespace

int main() {
    TestParameterBoundsAndNullPointers();
    TestLazyCondAllocation();
    TestDestroyedCondPrecedence();
    TestWaiterWakeSemantics();
    return 0;
}
