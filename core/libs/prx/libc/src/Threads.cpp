#include "prx/libc/include/General.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>

extern "C" {

double APS5_VABI __powidf2_nid_postfix(double base, int exponent) {
    if (exponent < 0) {
        if (exponent == -2147483648) throw std::overflow_error("powi exponent overflow");
        return 1.0 / __powidf2_nid_postfix(base, -exponent);
    }
    double result = 1.0;
    while (exponent-- > 0) result *= base;
    return result;
}

unsigned long APS5_VABI _WStoul_nid_postfix(const wchar_t* text, wchar_t** endptr, int base) {
    return std::wcstoul(text, endptr, base);
}

int APS5_VABI _Iswctype_nid_postfix(wint_t character, wctype_t description) {
    return std::iswctype(character, description) ? 1 : 0;
}

long APS5_VABI abs_nid_postfix(long value) {
    return value < 0 ? -value : value;
}

struct MtxPrivate {
    std::recursive_mutex recursive;
    std::mutex plain;
    bool isRecursive;
};

struct CndPrivate {
    std::condition_variable signal;
};

namespace {

constexpr std::uint64_t mtxMagic = 0x4d5458315f314150ull;

bool mtxValid(const void* mutex) {
    return mutex != nullptr;
}

MtxPrivate* mtxState(void* mutex) {
    if (mutex == nullptr) throw std::invalid_argument("Mutex pointer is null");
    const auto slot = static_cast<std::atomic<void*>*>(mutex);
    auto* state = static_cast<MtxPrivate*>(slot->load(std::memory_order_acquire));
    if (state == nullptr) throw std::runtime_error("Mutex is not initialized");
    return state;
}

CndPrivate* cndState(void* cond) {
    if (cond == nullptr) throw std::invalid_argument("Condition variable pointer is null");
    const auto slot = static_cast<std::atomic<void*>*>(cond);
    auto* state = static_cast<CndPrivate*>(slot->load(std::memory_order_acquire));
    if (state == nullptr) throw std::runtime_error("Condition variable is not initialized");
    return state;
}

}

int APS5_VABI _Mtx_init_nid_postfix(void* mutex, int type) {
    if (mutex == nullptr) throw std::invalid_argument("Mutex pointer is null");
    if (type < 0 || (type & ~0x9) != 0)
        throw std::invalid_argument("Invalid C11 mutex type");
    auto* state = new MtxPrivate{std::recursive_mutex(), std::mutex(), (type & 1) != 0};
    static_cast<std::atomic<void*>*>(mutex)->store(state, std::memory_order_release);
    return 0;
}

int APS5_VABI _Mtx_lock_nid_postfix(void* mutex) {
    auto* state = mtxState(mutex);
    if (state->isRecursive)
        state->recursive.lock();
    else
        state->plain.lock();
    return 0;
}

int APS5_VABI _Mtx_trylock_nid_postfix(void* mutex) {
    auto* state = mtxState(mutex);
    if (state->isRecursive)
        return state->recursive.try_lock() ? 0 : 1;
    return state->plain.try_lock() ? 0 : 1;
}

int APS5_VABI _Mtx_unlock_nid_postfix(void* mutex) {
    auto* state = mtxState(mutex);
    if (state->isRecursive)
        state->recursive.unlock();
    else
        state->plain.unlock();
    return 0;
}

void APS5_VABI _Mtx_destroy_nid_postfix(void* mutex) {
    auto* state = mtxState(mutex);
    delete state;
    static_cast<std::atomic<void*>*>(mutex)->store(nullptr, std::memory_order_release);
}

int APS5_VABI _Cnd_init_nid_postfix(void* cond) {
    if (cond == nullptr) throw std::invalid_argument("Condition variable pointer is null");
    auto* state = new CndPrivate{std::condition_variable()};
    static_cast<std::atomic<void*>*>(cond)->store(state, std::memory_order_release);
    return 0;
}

int APS5_VABI _Cnd_wait_nid_postfix(void* cond, void* mutex) {
    auto* condState = cndState(cond);
    auto* mutexState = mtxState(mutex);
    if (mutexState->isRecursive)
        throw std::runtime_error("C11 condition wait requires a non-recursive mutex");
    std::unique_lock<std::mutex> lock(mutexState->plain, std::adopt_lock);
    condState->signal.wait(lock);
    lock.release();
    return 0;
}

int APS5_VABI _Cnd_broadcast_nid_postfix(void* cond) {
    cndState(cond)->signal.notify_all();
    return 0;
}

void APS5_VABI _Cnd_destroy_nid_postfix(void* cond) {
    auto* state = cndState(cond);
    delete state;
    static_cast<std::atomic<void*>*>(cond)->store(nullptr, std::memory_order_release);
}

}
