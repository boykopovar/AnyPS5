#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {
// FreeBSD guest ABI: PTHREAD_ONCE_INIT is {0, nullptr}; completed state is 1.
struct GuestOnce {
    std::int32_t state;
    void* mutex;
};
static_assert(offsetof(GuestOnce, mutex) == 8 && sizeof(GuestOnce) == 16);
std::mutex onceMutex;
std::condition_variable onceChanged;
}

extern "C" int APS5_VABI pthread_once_nid_postfix(
    GuestOnce* control, void (APS5_VABI *initialize)()) {
    if (!control || !initialize) return 22;
    std::unique_lock lock(onceMutex);
    onceChanged.wait(lock, [&] { return control->state != 2; });
    if (control->state == 1) return 0;
    if (control->state != 0) return 22;
    control->state = 2;
    lock.unlock();
    // Initializers may initialize other once controls. Never run guest code
    // under the shared lock. The lock publishes all initializer writes.
    try {
        initialize();
    } catch (...) {
        lock.lock();
        control->state = 0;
        lock.unlock();
        onceChanged.notify_all();
        throw;
    }
    lock.lock();
    control->state = 1;
    lock.unlock();
    onceChanged.notify_all();
    return 0;
}

using OnceRoutine = void (APS5_VABI *)();

extern "C" {

int APS5_VABI scePthreadOnce_nid_postfix(void* onceControl, OnceRoutine initRoutine) {
    (void)onceControl;
    (void)initRoutine;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
