#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int PROCESS_PRIVATE = 0;
constexpr std::size_t MAX_BARRIER_NAME = 1024;

}

extern "C" {

int APS5_VABI pthread_barrierattr_init_nid_postfix(PthreadBarrierattrPrivate** attr) {
    if (!attr) return 22;
    try {
        auto* value = new PthreadBarrierattrPrivate;
        if (!GuestCopyOut_nid_no_patch(attr, &value, sizeof(value))) {
            delete value;
            return 14;
        }
        return 0;
    } catch (const std::bad_alloc&) {
        return 12;
    }
}

int APS5_VABI pthread_barrierattr_destroy_nid_postfix(PthreadBarrierattrPrivate** attr) {
    if (!attr) return 22;
    PthreadBarrierattrPrivate* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, attr, sizeof(value)) || !value) return 22;
    PthreadBarrierattrPrivate* cleared = nullptr;
    if (!GuestCopyOut_nid_no_patch(attr, &cleared, sizeof(cleared))) return 14;
    delete value;
    return 0;
}

int APS5_VABI pthread_barrierattr_getpshared_nid_postfix(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    if (!attr) return 22;
    PthreadBarrierattrPrivate* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, attr, sizeof(value)) || !value) return 22;
    if (!pshared) APS5_INVALID_ARG_EX;
    if (!GuestCopyOut_nid_no_patch(pshared, &value->pshared, sizeof(value->pshared))) return 14;
    return 0;
}

int APS5_VABI pthread_barrierattr_setpshared_nid_postfix(PthreadBarrierattrPrivate** attr, int pshared) {
    if (!attr) return 22;
    PthreadBarrierattrPrivate* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, attr, sizeof(value)) || !value) return 22;
    if (pshared != PROCESS_PRIVATE) return 22;
    value->pshared = pshared;
    return 0;
}

int APS5_VABI pthread_barrier_init_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    (void)attr;
    if (!barrier || count == 0 || count > INT_MAX) return 22;
    try {
        auto* value = new PthreadBarrierPrivate;
        value->count = count;
        if (!GuestCopyOut_nid_no_patch(barrier, &value, sizeof(value))) {
            delete value;
            return 14;
        }
        return 0;
    } catch (const std::bad_alloc&) {
        return 12;
    }
}

int APS5_VABI pthread_barrier_wait_nid_postfix(PthreadBarrierPrivate** barrier) {
    if (!barrier) return 22;
    PthreadBarrierPrivate* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, barrier, sizeof(value)) || !value) return 22;
    std::unique_lock lock(value->mutex);
    const auto generation = value->generation;
    ++value->active;
    if (++value->arrived == value->count) {
        value->arrived = 0;
        ++value->generation;
        value->changed.notify_all();
        --value->active;
        return -1;
    }
    value->changed.wait(lock, [&] { return value->generation != generation; });
    if (--value->active == 0) value->changed.notify_all();
    return 0;
}

int APS5_VABI pthread_barrier_destroy_nid_postfix(PthreadBarrierPrivate** barrier) {
    if (!barrier) return 22;
    PthreadBarrierPrivate* value = nullptr;
    if (!GuestCopyIn_nid_no_patch(&value, barrier, sizeof(value)) || !value) return 22;
    {
        std::unique_lock lock(value->mutex);
        if (value->arrived != 0) return 16;
        value->changed.wait(lock, [&] { return value->active == 0; });
    }
    PthreadBarrierPrivate* cleared = nullptr;
    if (!GuestCopyOut_nid_no_patch(barrier, &cleared, sizeof(cleared))) return 14;
    delete value;
    return 0;
}

int APS5_VABI pthread_barrier_setname_np_nid_postfix(PthreadBarrierPrivate** barrier, const char* name) {
    if (!barrier) throw std::invalid_argument("pthread_barrier_setname_np: null barrier");
    PthreadBarrierPrivate* handle = nullptr;
    if (!GuestCopyIn_nid_no_patch(&handle, barrier, sizeof(handle))) return 14;
    if (!handle) throw std::invalid_argument("pthread_barrier_setname_np: barrier is not initialized or was destroyed");
    auto& value = *handle;
    std::lock_guard lock(value.mutex);
    if (value.named) return 35;
    if (name) {
        std::string copy;
        if (const int error = CopyGuestString_nid_no_patch(name, MAX_BARRIER_NAME, copy)) return error;
        value.name = std::move(copy);
        value.named = true;
    }
    return 0;
}

}
