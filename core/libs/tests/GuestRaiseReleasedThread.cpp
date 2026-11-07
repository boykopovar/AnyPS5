#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <malloc.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

static constexpr int GUEST_SIGUSR1 = 30;
static constexpr int SCE_OK = 0;
static constexpr int DETACH_DETACHED = 1;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<void*> watched{nullptr};
static std::atomic<bool> watchedReleased{false};

static void Release(void* pointer) noexcept {
    if (pointer == nullptr) return;
    if (pointer == watched.load(std::memory_order_acquire)) {
        std::memset(pointer, 0, malloc_usable_size(pointer));
        watchedReleased.store(true, std::memory_order_release);
        return;
    }
    std::free(pointer);
}

void* operator new(std::size_t bytes) {
    if (void* pointer = std::malloc(bytes != 0 ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return operator new(bytes); }
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept { return std::malloc(bytes != 0 ? bytes : 1); }
void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept { return std::malloc(bytes != 0 ? bytes : 1); }
void operator delete(void* pointer) noexcept { Release(pointer); }
void operator delete[](void* pointer) noexcept { Release(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { Release(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { Release(pointer); }

static std::atomic<int> handled{0};

static void APS5_VABI Handler(int signum, void*) {
    Require(signum == GUEST_SIGUSR1);
    handled.fetch_add(1);
}

static void* APS5_VABI Return(void*) { return nullptr; }

static void* APS5_VABI ReturnWhenReleased(void* opaque) {
    auto* go = static_cast<std::atomic<bool>*>(opaque);
    while (!go->load(std::memory_order_acquire)) std::this_thread::yield();
    return nullptr;
}

static void Watch(Pthread thread) {
    watchedReleased.store(false, std::memory_order_relaxed);
    watched.store(thread, std::memory_order_release);
}

static void ExpectReleasedHandleRejected(Pthread released, const char* what) {
    const int before = handled.load();
    const int result = sceKernelRaiseException(released, GUEST_SIGUSR1);
    if (result != SCE_KERNEL_ERROR_ESRCH) {
        std::fprintf(stderr, "raise at %s returned 0x%x\n", what, static_cast<unsigned>(result));
        std::abort();
    }
    Require(handled.load() == before);
    watched.store(nullptr, std::memory_order_release);
}

int main() {
    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&Handler)) == SCE_OK);

    Pthread slot = nullptr;
    Require(scePthreadCreate(&slot, nullptr, Return, nullptr, "joined") == SCE_OK);
    Require(scePthreadJoin(slot, nullptr) == SCE_OK);
    const Pthread joined = slot;
    Watch(joined);
    Require(scePthreadCreate(&slot, nullptr, Return, nullptr, "successor") == SCE_OK);
    Require(watchedReleased.load(std::memory_order_acquire));
    ExpectReleasedHandleRejected(joined, "a joined thread whose handle storage was reused");
    Require(scePthreadJoin(slot, nullptr) == SCE_OK);

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetdetachstate(&attr, DETACH_DETACHED) == SCE_OK);
    for (int round = 0; round < 8; ++round) {
        std::atomic<bool> go{false};
        Pthread detached = nullptr;
        Require(scePthreadCreate(&detached, &attr, ReturnWhenReleased, &go, "detached") == SCE_OK);
        Watch(detached);
        go.store(true, std::memory_order_release);
        while (!watchedReleased.load(std::memory_order_acquire)) std::this_thread::yield();
        ExpectReleasedHandleRejected(detached, "a detached thread that exited");
    }
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == SCE_OK);
    std::printf("raises at released thread handles return ESRCH\n");
    return 0;
}
