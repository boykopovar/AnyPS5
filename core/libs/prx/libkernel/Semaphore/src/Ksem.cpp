#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Ksem.hpp"
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEbusy = 16;
constexpr int GuestEinval = 22;
constexpr int GuestEnospc = 28;
constexpr int GuestEagain = 35;
constexpr int GuestEtimedout = 60;
constexpr int GuestEoverflow = 84;
constexpr unsigned SemValueMax = 0x7fffffffu;
constexpr std::size_t SemMax = 30;

struct Ksem {
    std::mutex lock;
    std::condition_variable available;
    unsigned count = 0;
    unsigned waiters = 0;
};

std::mutex g_ksemLock;
std::map<int, std::shared_ptr<Ksem>> g_ksems;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

std::shared_ptr<Ksem> Find(std::intptr_t id) {
    if (id < 0 || id > INT_MAX) return {};
    std::lock_guard lock(g_ksemLock);
    const auto found = g_ksems.find(static_cast<int>(id));
    return found != g_ksems.end() ? found->second : std::shared_ptr<Ksem>{};
}

int ReserveDescriptor() {
#ifdef _WIN32
    return ::_open("NUL", _O_RDONLY | _O_BINARY);
#else
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
#endif
}

void DescriptorClosed(int descriptor) {
    std::lock_guard lock(g_ksemLock);
    g_ksems.erase(descriptor);
}

const bool g_observerRegistered = [] {
    RegisterDescriptorCloseObserver_nid_no_patch(DescriptorClosed);
    return true;
}();

int Wait(std::intptr_t id, bool once, const KernelTimespec* abstime) {
    const auto ksem = Find(id);
    if (!ksem) return Fail(GuestEinval);
    std::unique_lock lock(ksem->lock);
    if (ksem->count == 0) {
        if (once) return Fail(GuestEagain);
        ++ksem->waiters;
        bool ready = true;
        if (abstime == nullptr) {
            ksem->available.wait(lock, [&] { return ksem->count > 0; });
        } else {
            const auto deadline = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::seconds(abstime->tv_sec) + std::chrono::nanoseconds(abstime->tv_nsec)));
            ready = ksem->available.wait_until(lock, deadline, [&] { return ksem->count > 0; });
        }
        --ksem->waiters;
        if (!ready) return Fail(GuestEtimedout);
    }
    --ksem->count;
    return 0;
}

}

bool IsKsemDescriptor(int descriptor) {
    std::lock_guard lock(g_ksemLock);
    return g_ksems.count(descriptor) != 0;
}

extern "C" {

int APS5_VABI ksem_init_nid_postfix(std::intptr_t* idp, unsigned int value) {
    if (value > SemValueMax) return Fail(GuestEinval);
    const int descriptor = ReserveDescriptor();
    if (descriptor < 0) return Fail(GuestEnospc);
    if (idp == nullptr) {
        close_nid_postfix(descriptor);
        return Fail(GuestEfault);
    }
    *idp = descriptor;
    auto ksem = std::make_shared<Ksem>();
    ksem->count = value;
    {
        std::lock_guard lock(g_ksemLock);
        if (g_ksems.size() < SemMax) {
            g_ksems[descriptor] = std::move(ksem);
            return 0;
        }
    }
    close_nid_postfix(descriptor);
    return Fail(GuestEnospc);
}

int APS5_VABI ksem_post_nid_postfix(std::intptr_t id) {
    const auto ksem = Find(id);
    if (!ksem) return Fail(GuestEinval);
    std::lock_guard lock(ksem->lock);
    if (ksem->count == SemValueMax) return Fail(GuestEoverflow);
    ++ksem->count;
    ksem->available.notify_one();
    return 0;
}

int APS5_VABI ksem_wait_nid_postfix(std::intptr_t id) {
    return Wait(id, false, nullptr);
}

int APS5_VABI ksem_trywait_nid_postfix(std::intptr_t id) {
    return Wait(id, true, nullptr);
}

int APS5_VABI ksem_timedwait_nid_postfix(std::intptr_t id, const KernelTimespec* abstime) {
    if (abstime != nullptr && (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000)) return Fail(GuestEinval);
    return Wait(id, false, abstime);
}

int APS5_VABI ksem_getvalue_nid_postfix(std::intptr_t id, int* value) {
    const auto ksem = Find(id);
    if (!ksem) return Fail(GuestEinval);
    if (value == nullptr) return Fail(GuestEfault);
    std::lock_guard lock(ksem->lock);
    *value = static_cast<int>(ksem->count);
    return 0;
}

int APS5_VABI ksem_destroy_nid_postfix(std::intptr_t id) {
    const auto ksem = Find(id);
    if (!ksem) return Fail(GuestEinval);
    {
        std::lock_guard lock(ksem->lock);
        if (ksem->waiters != 0) return Fail(GuestEbusy);
    }
    return close_nid_postfix(static_cast<int>(id));
}

}
