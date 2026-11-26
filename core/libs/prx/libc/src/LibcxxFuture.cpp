#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <typeinfo>

namespace {

using LibcException::LibcxxAssocSubState;

constexpr std::uint32_t Constructed = 1;
constexpr std::uint32_t FutureAttached = 2;
constexpr std::uint32_t Ready = 4;
constexpr std::uint32_t Deferred = 8;
constexpr int FutureAlreadyRetrieved = 1;
constexpr int PromiseAlreadySatisfied = 2;
constexpr int NoState = 3;
constexpr int BrokenPromise = 4;
constexpr int OperationNotPermitted = 1;
constexpr std::size_t OnZeroSharedSlot = 2;
constexpr std::size_t DeleteSlot = 1;
constexpr std::size_t ExecuteSlot = 3;

static_assert(sizeof(LibcxxAssocSubState) == 48 && offsetof(LibcxxAssocSubState, sharedOwners) == 8);
static_assert(offsetof(LibcxxAssocSubState, exception) == 16 && offsetof(LibcxxAssocSubState, mutex) == 24);
static_assert(offsetof(LibcxxAssocSubState, condition) == 32 && offsetof(LibcxxAssocSubState, state) == 40);
static_assert(sizeof(LibcException::LibcxxAssocSubStateVtable) == 48);

struct GuestUniqueLock {
    PthreadMutex* mutex;
    bool owns;
};
static_assert(sizeof(GuestUniqueLock) == 16);

class StateLock {
public:
    explicit StateLock(LibcxxAssocSubState* state) : lock {&state->mutex, true} { _ZNSt3__15mutex4lockEv_nid_postfix(&state->mutex); }
    ~StateLock() { if (lock.owns) _ZNSt3__15mutex6unlockEv_nid_postfix(lock.mutex); }
    StateLock(const StateLock&) = delete;
    StateLock& operator=(const StateLock&) = delete;
    GuestUniqueLock lock;
};

using StateSlot = void (APS5_VABI *)(LibcxxAssocSubState*);

StateSlot Slot(const LibcxxAssocSubState* state, std::size_t index) {
    return reinterpret_cast<StateSlot>(const_cast<void*>(static_cast<const void* const*>(state->vtable)[index]));
}

std::type_info* FutureErrorType() {
    return reinterpret_cast<std::type_info*>(const_cast<LibcException::TypeRecord*>(&_ZTINSt3__112future_errorE_nid_postfix));
}

decltype(LibcException::Header::destructor) FutureErrorDestructor() {
    return reinterpret_cast<decltype(LibcException::Header::destructor)>(_ZNSt3__112future_errorD1Ev_nid_postfix);
}

LibcException::LibcxxFutureErrorObject* NewFutureError(int value) {
    auto* object = static_cast<LibcException::LibcxxFutureErrorObject*>(
        __cxa_allocate_exception_nid_postfix(sizeof(LibcException::LibcxxFutureErrorObject)));
    try {
        _ZNSt3__112future_errorC1ENS_10error_codeE_nid_postfix(object, {value, 0, _ZNSt3__115future_categoryEv_nid_postfix()});
    } catch (...) {
        __cxa_free_exception_nid_postfix(object);
        throw;
    }
    return object;
}

[[noreturn]] void ThrowFutureError(int value) {
    __cxa_throw_nid_postfix(NewFutureError(value), FutureErrorType(), FutureErrorDestructor());
}

GuestLibcxxExceptionPointer FutureErrorPointer(int value) {
    void* object = NewFutureError(value);
    __cxa_init_primary_exception_nid_postfix(object, FutureErrorType(), FutureErrorDestructor());
    __cxa_increment_exception_refcount_nid_postfix(object);
    return {object};
}

bool HasValue(const LibcxxAssocSubState* state) {
    return (state->state & Constructed) != 0 || state->exception.object != nullptr;
}

void AddShared(LibcxxAssocSubState* state) {
    std::atomic_ref<std::int64_t>(state->sharedOwners).fetch_add(1, std::memory_order_relaxed);
}

std::int64_t UseCount(LibcxxAssocSubState* state) {
    return std::atomic_ref<std::int64_t>(state->sharedOwners).load(std::memory_order_relaxed) + 1;
}

void ReleaseShared(LibcxxAssocSubState* state) {
    if (std::atomic_ref<std::int64_t>(state->sharedOwners).fetch_sub(1, std::memory_order_acq_rel) == 0)
        Slot(state, OnZeroSharedSlot)(state);
}

void UnlockGuestLock(GuestUniqueLock* lock) {
    if (!lock->owns) _ZNSt3__120__throw_system_errorEiPKc_nid_postfix(OperationNotPermitted, "unique_lock::unlock: not locked");
    _ZNSt3__15mutex6unlockEv_nid_postfix(lock->mutex);
    lock->owns = false;
}

void SubWait(LibcxxAssocSubState* state, GuestUniqueLock* lock) {
    if ((state->state & Ready) != 0) return;
    if ((state->state & Deferred) != 0) {
        state->state &= ~Deferred;
        UnlockGuestLock(lock);
        Slot(state, ExecuteSlot)(state);
        return;
    }
    while ((state->state & Ready) == 0)
        _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(&state->condition, lock);
}

void APS5_VABI DestroyState_nid_no_patch(LibcxxAssocSubState* state) {
    _ZNSt3__118condition_variableD1Ev_nid_postfix(&state->condition);
    _ZNSt3__15mutexD1Ev_nid_postfix(&state->mutex);
    _ZNSt13exception_ptrD1Ev_nid_postfix(&state->exception);
}

void APS5_VABI DeleteState_nid_no_patch(LibcxxAssocSubState* state) {
    DestroyState_nid_no_patch(state);
    _ZdlPv_nid_postfix(state);
}

[[noreturn]] void APS5_VABI ExecuteState_nid_no_patch(LibcxxAssocSubState*) {
    ThrowFutureError(NoState);
}

}

extern "C" {

constinit const LibcException::LibcxxAssocSubStateVtable _ZTVNSt3__117__assoc_sub_stateE_nid_postfix {
    0, &_ZTINSt3__117__assoc_sub_stateE_nid_postfix, DestroyState_nid_no_patch, DeleteState_nid_no_patch,
    _ZNSt3__117__assoc_sub_state16__on_zero_sharedEv_nid_postfix, ExecuteState_nid_no_patch,
};

void APS5_VABI _ZNSt3__117__assoc_sub_state16__on_zero_sharedEv_nid_postfix(LibcxxAssocSubState* self) noexcept {
    Slot(self, DeleteSlot)(self);
}

void APS5_VABI _ZNSt3__117__assoc_sub_state9set_valueEv_nid_postfix(LibcxxAssocSubState* self) {
    StateLock guard(self);
    if (HasValue(self)) ThrowFutureError(PromiseAlreadySatisfied);
    self->state |= Constructed | Ready;
    _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&self->condition);
}

void APS5_VABI _ZNSt3__117__assoc_sub_state13set_exceptionESt13exception_ptr_nid_postfix(
    LibcxxAssocSubState* self, const GuestLibcxxExceptionPointer* exception) {
    StateLock guard(self);
    if (HasValue(self)) ThrowFutureError(PromiseAlreadySatisfied);
    void* previous = self->exception.object;
    if (previous != exception->object) {
        __cxa_increment_exception_refcount_nid_postfix(exception->object);
        __cxa_decrement_exception_refcount_nid_postfix(previous);
        self->exception.object = exception->object;
    }
    self->state |= Ready;
    _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&self->condition);
}

void APS5_VABI _ZNSt3__117__assoc_sub_state10__sub_waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(LibcxxAssocSubState* self, void* lock) {
    SubWait(self, static_cast<GuestUniqueLock*>(lock));
}

void APS5_VABI _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(LibcxxAssocSubState* self) {
    StateLock guard(self);
    SubWait(self, &guard.lock);
}

void APS5_VABI _ZNSt3__16futureIvEC1EPNS_17__assoc_sub_stateE_nid_postfix(LibcException::LibcxxFutureVoid* self, LibcxxAssocSubState* state) {
    if (!state) throw std::invalid_argument("std::__1::future<void>: null shared state");
    self->state = state;
    StateLock guard(state);
    if ((state->state & FutureAttached) != 0) ThrowFutureError(FutureAlreadyRetrieved);
    AddShared(state);
    state->state |= FutureAttached;
}

void APS5_VABI _ZNSt3__16futureIvED1Ev_nid_postfix(LibcException::LibcxxFutureVoid* self) noexcept {
    if (self->state) ReleaseShared(self->state);
}

void APS5_VABI _ZNSt3__17promiseIvEC1Ev_nid_postfix(LibcException::LibcxxPromiseVoid* self) {
    void* storage = _Znwm_nid_postfix(sizeof(LibcxxAssocSubState));
    self->state = new (storage) LibcxxAssocSubState {&_ZTVNSt3__117__assoc_sub_stateE_nid_postfix.destroy, 0, {nullptr}, nullptr, nullptr, 0};
}

void APS5_VABI _ZNSt3__17promiseIvED1Ev_nid_postfix(LibcException::LibcxxPromiseVoid* self) noexcept {
    LibcxxAssocSubState* state = self->state;
    if (!state) return;
    if (!HasValue(state) && UseCount(state) > 1) {
        GuestLibcxxExceptionPointer broken = FutureErrorPointer(BrokenPromise);
        _ZNSt3__117__assoc_sub_state13set_exceptionESt13exception_ptr_nid_postfix(state, &broken);
        _ZNSt13exception_ptrD1Ev_nid_postfix(&broken);
    }
    ReleaseShared(state);
}

LibcException::LibcxxFutureVoid* APS5_VABI _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(
    LibcException::LibcxxFutureVoid* result, LibcException::LibcxxPromiseVoid* self) {
    if (!self->state) ThrowFutureError(NoState);
    _ZNSt3__16futureIvEC1EPNS_17__assoc_sub_stateE_nid_postfix(result, self->state);
    return result;
}

void APS5_VABI _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(LibcException::LibcxxPromiseVoid* self) {
    if (!self->state) ThrowFutureError(NoState);
    _ZNSt3__117__assoc_sub_state9set_valueEv_nid_postfix(self->state);
}

void APS5_VABI _ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr_nid_postfix(
    LibcException::LibcxxPromiseVoid* self, const GuestLibcxxExceptionPointer* exception) {
    if (!self->state) ThrowFutureError(NoState);
    _ZNSt3__117__assoc_sub_state13set_exceptionESt13exception_ptr_nid_postfix(self->state, exception);
}

}
