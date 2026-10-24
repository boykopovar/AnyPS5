#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <atomic>
#include <chrono>
#include <thread>

extern "C" void KernelTraceWait_nid_postfix(const char*, const void*, std::uint64_t, bool) {}

int main() {
    KernelSema sem = nullptr;
    if (sceKernelCreateSema(&sem, "trace", 0, 0, 1, nullptr) != KERNEL_SEMA_OK) return 1;

    std::atomic<int> result{-1};
    std::thread waiter([&] { result.store(sceKernelWaitSema(sem, 1, nullptr)); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        {
            std::lock_guard lock(sem->mutex);
            if (sem->waiterCount != 0) break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            sceKernelSignalSema(sem, 1);
            waiter.join();
            sceKernelDeleteSema(sem);
            return 2;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const int signaled = sceKernelSignalSema(sem, 1);
    waiter.join();
    const int deleted = sceKernelDeleteSema(sem);
    return signaled == KERNEL_SEMA_OK && result.load() == KERNEL_SEMA_OK && deleted == KERNEL_SEMA_OK ? 0 : 3;
}
