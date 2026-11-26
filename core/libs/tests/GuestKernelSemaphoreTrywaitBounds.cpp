#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr unsigned int GuestSemValueMax = 0x7fffffffu;
constexpr int GuestOCreat = 0x200;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestPosixSemaphoreTrywaitBounds() {
    Require(sem_trywait_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == PosixEinval);

    std::uintptr_t uninit = 0;
    Require(sem_trywait_nid_postfix(&uninit) == -1 && *__error_nid_postfix() == PosixEinval);

    std::uintptr_t semSlot = 0;
    Require(sem_init_nid_postfix(&semSlot, 0, 0) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_post_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_destroy_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(sem_init_nid_postfix(&semSlot, 0, 2) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_destroy_nid_postfix(&semSlot) == 0);

    Require(sem_init_nid_postfix(&semSlot, 0, GuestSemValueMax) == 0);
    Require(sem_trywait_nid_postfix(&semSlot) == 0);
    int value = 0;
    Require(sem_getvalue_nid_postfix(&semSlot, &value) == 0 && value == static_cast<int>(GuestSemValueMax - 1));
    Require(sem_destroy_nid_postfix(&semSlot) == 0);

    void* namedSem = sem_open_nid_postfix("/trywait_named_bounds", GuestOCreat, 0644, 2);
    Require(namedSem != nullptr);
    Require(sem_trywait_nid_postfix(namedSem) == 0);
    Require(sem_trywait_nid_postfix(namedSem) == 0);
    Require(sem_trywait_nid_postfix(namedSem) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_post_nid_postfix(namedSem) == 0);
    Require(sem_trywait_nid_postfix(namedSem) == 0);
    Require(sem_trywait_nid_postfix(namedSem) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(sem_close_nid_postfix(namedSem) == 0);
    Require(sem_unlink_nid_postfix("/trywait_named_bounds") == 0);
}

void TestKsemTrywaitBounds() {
    Require(ksem_trywait_nid_postfix(-1) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(ksem_trywait_nid_postfix(-100) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(ksem_trywait_nid_postfix(0) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(ksem_trywait_nid_postfix(4096) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(ksem_trywait_nid_postfix(INT_MIN) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(ksem_trywait_nid_postfix(INT_MAX) == -1 && *__error_nid_postfix() == PosixEinval);

    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    Require(id >= 0);
    Require(ksem_trywait_nid_postfix(id) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(ksem_trywait_nid_postfix(id) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(ksem_post_nid_postfix(id) == 0);
    Require(ksem_trywait_nid_postfix(id) == 0);
    Require(ksem_trywait_nid_postfix(id) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(ksem_destroy_nid_postfix(id) == 0);
    Require(ksem_trywait_nid_postfix(id) == -1 && *__error_nid_postfix() == PosixEinval);

    std::intptr_t multiId = -1;
    Require(ksem_init_nid_postfix(&multiId, 3) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(ksem_post_nid_postfix(multiId) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == 0);
    Require(ksem_trywait_nid_postfix(multiId) == -1 && *__error_nid_postfix() == PosixEagain);
    Require(ksem_destroy_nid_postfix(multiId) == 0);

    std::intptr_t maxId = -1;
    Require(ksem_init_nid_postfix(&maxId, GuestSemValueMax) == 0);
    Require(ksem_trywait_nid_postfix(maxId) == 0);
    int value = 0;
    Require(ksem_getvalue_nid_postfix(maxId, &value) == 0 && value == static_cast<int>(GuestSemValueMax - 1));
    Require(ksem_destroy_nid_postfix(maxId) == 0);
}

void TestKernelPollSemaBounds() {
    Require(sceKernelPollSema(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollSema(nullptr, 0) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollSema(nullptr, -1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollSema(nullptr, INT_MIN) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollSema(nullptr, INT_MAX) == SCE_KERNEL_ERROR_ESRCH);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "poll_sema_bounds", 0, 2, 5, nullptr) == KERNEL_SEMA_OK);
    Require(sem != nullptr);

    Require(sceKernelPollSema(sem, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, -100) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, INT_MIN) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelPollSema(sem, 6) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 1000) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, INT_MAX) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelPollSema(sem, 3) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelPollSema(sem, 4) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelPollSema(sem, 5) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelPollSema(sem, 2) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelSignalSema(sem, 5) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 3) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 2) == KERNEL_SEMA_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelDeleteSema(sem) == KERNEL_SEMA_OK);
}

struct PosixConcurrentContext {
    std::uintptr_t semSlot = 0;
    std::atomic<bool> start{false};
    std::atomic<int> successCount{0};
    std::atomic<int> againCount{0};
};

void* APS5_VABI PosixConcurrentWorker(void* arg) {
    auto* ctx = static_cast<PosixConcurrentContext*>(arg);
    while (!ctx->start.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    if (sem_trywait_nid_postfix(&ctx->semSlot) == 0) {
        ctx->successCount.fetch_add(1, std::memory_order_relaxed);
    } else if (*__error_nid_postfix() == PosixEagain) {
        ctx->againCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::abort();
    }
    return nullptr;
}

struct KsemConcurrentContext {
    std::intptr_t id = -1;
    std::atomic<bool> start{false};
    std::atomic<int> successCount{0};
    std::atomic<int> againCount{0};
};

void* APS5_VABI KsemConcurrentWorker(void* arg) {
    auto* ctx = static_cast<KsemConcurrentContext*>(arg);
    while (!ctx->start.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    if (ksem_trywait_nid_postfix(ctx->id) == 0) {
        ctx->successCount.fetch_add(1, std::memory_order_relaxed);
    } else if (*__error_nid_postfix() == PosixEagain) {
        ctx->againCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::abort();
    }
    return nullptr;
}

struct PthreadSemConcurrentContext {
    PthreadSem sem = nullptr;
    std::atomic<bool> start{false};
    std::atomic<int> successCount{0};
    std::atomic<int> busyCount{0};
};

void* APS5_VABI PthreadSemConcurrentWorker(void* arg) {
    auto* ctx = static_cast<PthreadSemConcurrentContext*>(arg);
    while (!ctx->start.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    const int res = scePthreadSemTrywait(&ctx->sem);
    if (res == 0) {
        ctx->successCount.fetch_add(1, std::memory_order_relaxed);
    } else if (res == SCE_KERNEL_ERROR_EBUSY) {
        ctx->busyCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::abort();
    }
    return nullptr;
}

struct KernelSemaConcurrentContext {
    KernelSema sem = nullptr;
    std::atomic<bool> start{false};
    std::atomic<int> successCount{0};
    std::atomic<int> busyCount{0};
};

void* APS5_VABI KernelSemaConcurrentWorker(void* arg) {
    auto* ctx = static_cast<KernelSemaConcurrentContext*>(arg);
    while (!ctx->start.load(std::memory_order_acquire)) {
        scePthreadYield();
    }
    const int res = sceKernelPollSema(ctx->sem, 1);
    if (res == KERNEL_SEMA_OK) {
        ctx->successCount.fetch_add(1, std::memory_order_relaxed);
    } else if (res == SCE_KERNEL_ERROR_EBUSY) {
        ctx->busyCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::abort();
    }
    return nullptr;
}

void TestConcurrentTrywaitContention() {
    constexpr unsigned ThreadCount = 4;
    constexpr int Permits = 2;

    {
        PosixConcurrentContext ctx;
        Require(sem_init_nid_postfix(&ctx.semSlot, 0, Permits) == 0);
        std::array<Pthread, ThreadCount> threads{};
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadCreate(&threads[i], nullptr, PosixConcurrentWorker, &ctx, "posix_worker") == 0);
        }
        ctx.start.store(true, std::memory_order_release);
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadJoin(threads[i], nullptr) == 0);
        }
        Require(ctx.successCount.load(std::memory_order_relaxed) == Permits);
        Require(ctx.againCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount - Permits));
        Require(sem_destroy_nid_postfix(&ctx.semSlot) == 0);
    }

    {
        KsemConcurrentContext ctx;
        Require(ksem_init_nid_postfix(&ctx.id, Permits) == 0);
        std::array<Pthread, ThreadCount> threads{};
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadCreate(&threads[i], nullptr, KsemConcurrentWorker, &ctx, "ksem_worker") == 0);
        }
        ctx.start.store(true, std::memory_order_release);
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadJoin(threads[i], nullptr) == 0);
        }
        Require(ctx.successCount.load(std::memory_order_relaxed) == Permits);
        Require(ctx.againCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount - Permits));
        Require(ksem_destroy_nid_postfix(ctx.id) == 0);
    }

    {
        PthreadSemConcurrentContext ctx;
        Require(scePthreadSemInit(&ctx.sem, 0, Permits, "mt_sem") == 0);
        std::array<Pthread, ThreadCount> threads{};
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadCreate(&threads[i], nullptr, PthreadSemConcurrentWorker, &ctx, "psem_worker") == 0);
        }
        ctx.start.store(true, std::memory_order_release);
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadJoin(threads[i], nullptr) == 0);
        }
        Require(ctx.successCount.load(std::memory_order_relaxed) == Permits);
        Require(ctx.busyCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount - Permits));
        Require(scePthreadSemDestroy(&ctx.sem) == 0);
    }

    {
        KernelSemaConcurrentContext ctx;
        Require(sceKernelCreateSema(&ctx.sem, "mt_poll", 0, Permits, Permits, nullptr) == KERNEL_SEMA_OK);
        std::array<Pthread, ThreadCount> threads{};
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadCreate(&threads[i], nullptr, KernelSemaConcurrentWorker, &ctx, "ksema_worker") == 0);
        }
        ctx.start.store(true, std::memory_order_release);
        for (unsigned i = 0; i < ThreadCount; ++i) {
            Require(scePthreadJoin(threads[i], nullptr) == 0);
        }
        Require(ctx.successCount.load(std::memory_order_relaxed) == Permits);
        Require(ctx.busyCount.load(std::memory_order_relaxed) == static_cast<int>(ThreadCount - Permits));
        Require(sceKernelDeleteSema(ctx.sem) == KERNEL_SEMA_OK);
    }
}

} // namespace

int main() {
    TestPosixSemaphoreTrywaitBounds();
    TestKsemTrywaitBounds();
    TestKernelPollSemaBounds();
    TestConcurrentTrywaitContention();
    void* semSlot = nullptr;
    Require(sem_init_nid_postfix(reinterpret_cast<void*>(0x10), 0, 1) == -1 && errno == 22);
    Require(sem_init_nid_postfix(&semSlot, 0, 2) == 0);
    int semValue = -1;
    Require(sem_getvalue_nid_postfix(&semSlot, &semValue) == 0 && semValue == 2);
    Require(sem_getvalue_nid_postfix(&semSlot, reinterpret_cast<int*>(0x10)) == -1 && errno == 22);
    Require(sem_destroy_nid_postfix(reinterpret_cast<void*>(0x10)) == -1 && errno == 22);
    Require(sem_destroy_nid_postfix(&semSlot) == 0 && semSlot == nullptr);
    return 0;
}
