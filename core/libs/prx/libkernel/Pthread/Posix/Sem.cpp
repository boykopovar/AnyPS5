#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "Common.hpp"
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>

// The guest sem_t is at least 8 bytes; its first word holds a pointer to the host semaphore.
struct PosixSemaphore {
    std::mutex lock;
    std::condition_variable available;
    unsigned int count;
};

static constexpr int GUEST_EAGAIN = 35;
static constexpr int GUEST_REALTIME_CLOCK = 0;

static int Fail(int error) {
    errno = error;
    return -1;
}

static PosixSemaphore* Get(void* sem) {
    if (!sem) return nullptr;
    PosixSemaphore* semaphore = nullptr;
    std::memcpy(&semaphore, sem, sizeof(semaphore));
    return semaphore;
}

static int WaitFor(PosixSemaphore* semaphore, const KernelUseconds* usec) {
    std::unique_lock lock(semaphore->lock);
    const auto ready = [&] { return semaphore->count > 0; };
    if (usec) {
        if (!semaphore->available.wait_for(lock, std::chrono::microseconds(*usec), ready)) return Fail(PosixThread::GUEST_ETIMEDOUT);
    } else {
        semaphore->available.wait(lock, ready);
    }
    --semaphore->count;
    return 0;
}

extern "C" {

int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem) return Fail(PosixThread::GUEST_EINVAL);
    auto* semaphore = new PosixSemaphore();
    semaphore->count = value;
    std::memcpy(sem, &semaphore, sizeof(semaphore));
    return 0;
}

int APS5_VABI sem_destroy_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    delete semaphore;
    std::memset(sem, 0, sizeof(semaphore));
    return 0;
}

int APS5_VABI sem_wait_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    return WaitFor(semaphore, nullptr);
}

int APS5_VABI sem_trywait_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    std::lock_guard lock(semaphore->lock);
    if (semaphore->count == 0) return Fail(GUEST_EAGAIN);
    --semaphore->count;
    return 0;
}

int APS5_VABI sem_timedwait_nid_postfix(void* sem, const KernelTimespec* abstime) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GUEST_REALTIME_CLOCK, abstime, &usec)) return Fail(PosixThread::GUEST_EINVAL);
    return WaitFor(semaphore, &usec);
}

int APS5_VABI sem_reltimedwait_np_nid_postfix(void* sem, uint32_t usec) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    const KernelUseconds timeout = usec;
    return WaitFor(semaphore, &timeout);
}

int APS5_VABI sem_post_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    {
        std::lock_guard lock(semaphore->lock);
        ++semaphore->count;
    }
    semaphore->available.notify_one();
    return 0;
}

int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value) {
    auto* semaphore = Get(sem);
    if (!semaphore || !value) return Fail(PosixThread::GUEST_EINVAL);
    std::lock_guard lock(semaphore->lock);
    *value = static_cast<int>(semaphore->count);
    return 0;
}

}
