#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {
// FreeBSD guest ABI: PTHREAD_ONCE_INIT is {0, nullptr}; completed state is 1.

static_assert(offsetof(GuestOnce, mutex) == 8 && sizeof(GuestOnce) == 16);
std::mutex onceMutex;
std::condition_variable onceChanged;

bool ReadControlState(const GuestOnce* control, std::int32_t& state) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(
        &state, reinterpret_cast<const std::uint8_t*>(control) + offsetof(GuestOnce, state), sizeof(state));
    errno = savedErrno;
    return copied;
}

bool WriteControlState(GuestOnce* control, std::int32_t state) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(
        reinterpret_cast<std::uint8_t*>(control) + offsetof(GuestOnce, state), &state, sizeof(state));
    errno = savedErrno;
    return copied;
}

void APS5_VABI ResetOnce(void* control) {
    {
        std::lock_guard lock(onceMutex);
        WriteControlState(static_cast<GuestOnce*>(control), 0);
    }
    onceChanged.notify_all();
}
}

extern "C" int APS5_VABI pthread_once_nid_postfix(
    GuestOnce* control, void (APS5_VABI *initialize)()) {
    if (!control || !initialize) return 22;
    std::unique_lock lock(onceMutex);
    std::int32_t state = 0;
    bool readable = true;
    onceChanged.wait(lock, [&] {
        readable = ReadControlState(control, state);
        return !readable || state != 2;
    });
    if (!readable) return 14;
    if (state == 1) return 0;
    if (state != 0) return 22;
    if (!WriteControlState(control, 2)) return 14;
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
    const bool updated = WriteControlState(control, 1);
    lock.unlock();
    onceChanged.notify_all();
    return updated ? 0 : 14;
}
