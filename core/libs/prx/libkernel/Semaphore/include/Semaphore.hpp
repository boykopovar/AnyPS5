#ifndef CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_SEMAPHORE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_SEMAPHORE_HPP

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

constexpr int KERNEL_SEMA_OK = 0;
constexpr int KERNEL_SEMA_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int KERNEL_SEMA_ERROR_EACCES = static_cast<int>(0x8002000D);
constexpr int KERNEL_SEMA_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int KERNEL_SEMA_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

struct KernelSemaPrivate {
    KernelSemaPrivate(std::int32_t initCount, std::int32_t maxCount, std::string name, bool isFifo);

    std::mutex mutex;
    TimedWait::Condition condition;
    std::string name;
    std::int32_t tokenCount;
    std::int32_t maxCount;
    bool isFifo;
    bool deleted = false;
    std::int32_t waiterCount = 0;
};

extern "C" {

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt);
int APS5_VABI sceKernelDeleteSema(KernelSema sem);
int APS5_VABI sceKernelPollSema(KernelSema sem, int need);
int APS5_VABI sceKernelSignalSema(KernelSema sem, int count);
int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time);

}

#endif
