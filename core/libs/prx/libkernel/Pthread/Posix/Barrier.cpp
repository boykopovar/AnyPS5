#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

namespace {

constexpr int PROCESS_PRIVATE = 0;

}

extern "C" {

int APS5_VABI pthread_barrierattr_init_nid_postfix(PthreadBarrierattrPrivate** attr) {
    if (!attr) return 22;
    try {
        *attr = new PthreadBarrierattrPrivate;
        return 0;
    } catch (const std::bad_alloc&) {
        return 12;
    }
}

int APS5_VABI pthread_barrierattr_destroy_nid_postfix(PthreadBarrierattrPrivate** attr) {
    if (!attr || !*attr) return 22;
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI pthread_barrierattr_getpshared_nid_postfix(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    if (!attr || !*attr) return 22;
    if (!pshared) APS5_INVALID_ARG_EX;
    *pshared = (*attr)->pshared;
    return 0;
}

int APS5_VABI pthread_barrierattr_setpshared_nid_postfix(PthreadBarrierattrPrivate** attr, int pshared) {
    if (!attr || !*attr) return 22;
    if (pshared != PROCESS_PRIVATE) return 22;
    (*attr)->pshared = pshared;
    return 0;
}

int APS5_VABI pthread_barrier_init_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    (void)attr;
    if (!barrier || count == 0 || count > INT_MAX) return 22;
    try {
        auto* value = new PthreadBarrierPrivate;
        value->count = count;
        *barrier = value;
        return 0;
    } catch (const std::bad_alloc&) {
        return 12;
    }
}

int APS5_VABI pthread_barrier_wait_nid_postfix(PthreadBarrierPrivate** barrier) {
    if (!barrier || !*barrier) return 22;
    auto& value = **barrier;
    std::unique_lock lock(value.mutex);
    const auto generation = value.generation;
    ++value.active;
    if (++value.arrived == value.count) {
        value.arrived = 0;
        ++value.generation;
        value.changed.notify_all();
        --value.active;
        return -1;
    }
    value.changed.wait(lock, [&] { return value.generation != generation; });
    if (--value.active == 0) value.changed.notify_all();
    return 0;
}

int APS5_VABI pthread_barrier_destroy_nid_postfix(PthreadBarrierPrivate** barrier) {
    if (!barrier || !*barrier) return 22;
    auto* value = *barrier;
    {
        std::unique_lock lock(value->mutex);
        if (value->arrived != 0) return 16;
        value->changed.wait(lock, [&] { return value->active == 0; });
    }
    delete value;
    *barrier = nullptr;
    return 0;
}

int APS5_VABI pthread_barrier_setname_np_nid_postfix(PthreadBarrierPrivate** barrier, const char* name) {
    if (!barrier) throw std::invalid_argument("pthread_barrier_setname_np: null barrier");
    if (!*barrier) throw std::invalid_argument("pthread_barrier_setname_np: barrier is not initialized or was destroyed");
    auto& value = **barrier;
    std::lock_guard lock(value.mutex);
    if (value.named) return 35;
    if (name) {
        value.name = name;
        value.named = true;
    }
    return 0;
}

}
