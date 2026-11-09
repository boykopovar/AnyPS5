#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {
// FreeBSD guest ABI: PTHREAD_ONCE_INIT is {0, nullptr}; completed state is 1.

static_assert(offsetof(GuestOnce, mutex) == 8 && sizeof(GuestOnce) == 16);
std::mutex onceMutex;
std::condition_variable onceChanged;

void APS5_VABI ResetOnce(void* control) {
    {
        std::lock_guard lock(onceMutex);
        static_cast<GuestOnce*>(control)->state = 0;
    }
    onceChanged.notify_all();
}
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
    CleanupHandler reset{};
    void* const mark = ThreadCancel::CleanupMark();
    __pthread_cleanup_push_imp_nid_postfix(ResetOnce, control, &reset);

    try {
        initialize();
    } catch (...) {
        ThreadCancel::CleanupRestore(mark);
        ResetOnce(control);
        throw;
    }
    ThreadCancel::CleanupRestore(mark);
    lock.lock();
    control->state = 1;
    lock.unlock();
    onceChanged.notify_all();
    return 0;
}
