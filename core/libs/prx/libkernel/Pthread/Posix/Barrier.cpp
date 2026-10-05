#include "prx/libc/include/General.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

struct PthreadBarrierPrivate {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned count;
    unsigned arrived = 0;
    unsigned active = 0;
    std::uint64_t generation = 0;
};

namespace {

constexpr int PROCESS_PRIVATE = 0;
constexpr int PROCESS_SHARED = 1;

}

struct PthreadBarrierattrPrivate {
    int pshared = PROCESS_PRIVATE;
};

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
    if (pshared == PROCESS_SHARED) NotImplemented_nid_no_patch("pthread_barrierattr_setpshared: PTHREAD_PROCESS_SHARED");
    if (pshared != PROCESS_PRIVATE) return 22;
    (*attr)->pshared = pshared;
    return 0;
}

int APS5_VABI pthread_barrier_init_nid_postfix(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count) {
    (void)attr;
    if (!barrier || count == 0) return 22;
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
    --value.active;
    return 0;
}

int APS5_VABI pthread_barrier_destroy_nid_postfix(PthreadBarrierPrivate** barrier) {
    if (!barrier || !*barrier) return 22;
    auto* value = *barrier;
    {
        std::lock_guard lock(value->mutex);
        if (value->active != 0) return 16;
    }
    delete value;
    *barrier = nullptr;
    return 0;
}

}
