#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

struct GuestBarrier;
struct GuestBarrierattr;

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadBarrierattrInit(GuestBarrierattr** attr);
int APS5_VABI scePthreadBarrierattrDestroy(GuestBarrierattr** attr);
int APS5_VABI scePthreadBarrierattrGetpshared(GuestBarrierattr* const* attr, int* pshared);
int APS5_VABI scePthreadBarrierattrSetpshared(GuestBarrierattr** attr, int pshared);
int APS5_VABI scePthreadBarrierInit(GuestBarrier** barrier, GuestBarrierattr* const* attr, unsigned count, const char* name);
int APS5_VABI scePthreadBarrierWait(GuestBarrier** barrier);
int APS5_VABI scePthreadBarrierDestroy(GuestBarrier** barrier);
int APS5_VABI scePthreadGetcpuclockid(Pthread thread, int* clockId);
int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId);
int APS5_VABI scePthreadGetconcurrency();
int APS5_VABI pthread_setconcurrency_nid_postfix(int level);
int APS5_VABI sceKernelMsync(void* address, std::size_t length, int flags);
}

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
static constexpr int SCE_KERNEL_ERROR_ENOMEM = static_cast<int>(0x8002000C);
static constexpr int PROCESS_PRIVATE = 0;
static constexpr int PROCESS_SHARED = 1;
static constexpr int BARRIER_SERIAL_THREAD = -1;
static constexpr int MS_ASYNC = 0x1;
static constexpr int MS_INVALIDATE = 0x2;
static constexpr unsigned WAITERS = 4;

static void Require(bool value) { if (!value) std::abort(); }

struct Context {
    GuestBarrier* barrier = nullptr;
    std::atomic<int> serial{0};
    std::atomic<int> released{0};
};

static void* APS5_VABI Waiter(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    const int result = scePthreadBarrierWait(&context.barrier);
    if (result == BARRIER_SERIAL_THREAD) ++context.serial;
    else if (result == 0) ++context.released;
    else std::abort();
    return nullptr;
}

int main() {
    GuestBarrierattr* attr = nullptr;
    Require(scePthreadBarrierattrInit(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadBarrierattrInit(&attr) == 0 && attr != nullptr);
    int pshared = -1;
    Require(scePthreadBarrierattrGetpshared(&attr, &pshared) == 0 && pshared == PROCESS_PRIVATE);
    Require(scePthreadBarrierattrSetpshared(&attr, PROCESS_SHARED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadBarrierattrSetpshared(&attr, PROCESS_PRIVATE) == 0);

    Context context;
    Require(scePthreadBarrierInit(&context.barrier, &attr, 0, "zero") == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadBarrierInit(&context.barrier, &attr, 0x80000000u, "over") == SCE_KERNEL_ERROR_EINVAL && context.barrier == nullptr);
    Require(scePthreadBarrierInit(nullptr, &attr, WAITERS, "null") == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadBarrierInit(&context.barrier, &attr, WAITERS, "barrier") == 0 && context.barrier != nullptr);
    Require(scePthreadBarrierattrDestroy(&attr) == 0 && attr == nullptr);
    Require(scePthreadBarrierattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);

    for (int round = 0; round < 2; ++round) {
        context.serial = 0;
        context.released = 0;
        std::array<Pthread, WAITERS> threads{};
        for (auto& thread : threads) Require(scePthreadCreate(&thread, nullptr, Waiter, &context, nullptr) == 0);
        for (auto& thread : threads) Require(scePthreadJoin(thread, nullptr) == 0);
        Require(context.serial == 1 && context.released == static_cast<int>(WAITERS) - 1);
    }
    Require(scePthreadBarrierDestroy(&context.barrier) == 0 && context.barrier == nullptr);
    Require(scePthreadBarrierDestroy(&context.barrier) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadBarrierWait(nullptr) == SCE_KERNEL_ERROR_EINVAL);

    GuestBarrier* single = nullptr;
    Require(scePthreadBarrierInit(&single, nullptr, 1, nullptr) == 0);
    Require(scePthreadBarrierWait(&single) == BARRIER_SERIAL_THREAD);
    Require(scePthreadBarrierDestroy(&single) == 0);

    int clock = 0;
    int posixClock = 0;
    Require(scePthreadGetcpuclockid(scePthreadSelf(), &clock) == 0);
    Require(pthread_getcpuclockid_nid_postfix(scePthreadSelf(), &posixClock) == 0 && clock == posixClock);
    Require(scePthreadGetcpuclockid(scePthreadSelf(), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadGetcpuclockid(nullptr, &clock) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_setconcurrency_nid_postfix(3) == 0 && scePthreadGetconcurrency() == 3);
    Require(pthread_setconcurrency_nid_postfix(0) == 0 && scePthreadGetconcurrency() == 0);

    auto* unmapped = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4000));
    Require(sceKernelMsync(unmapped, 0x4000, 0) == SCE_KERNEL_ERROR_ENOMEM);
    Require(sceKernelMsync(unmapped, 0x4000, MS_ASYNC | MS_INVALIDATE) == SCE_KERNEL_ERROR_EINVAL);
    return 0;
}
