#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>

using LibcException::LibcxxAssocSubState;
using LibcException::LibcxxAssocSubStateVtable;
using LibcException::LibcxxFutureErrorObject;
using LibcException::LibcxxFutureVoid;
using LibcException::LibcxxPromiseVoid;

struct LibcxxString { unsigned char bytes[24]; };
using Name = const char* (APS5_VABI *)(const LibcException::CategoryObject*);
using Message = LibcxxString* (APS5_VABI *)(LibcxxString*, const LibcException::CategoryObject*, int);
using What = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);

struct GuestUniqueLock { PthreadMutex* mutex; bool owns; };

static_assert(sizeof(LibcxxAssocSubState) == 48);
static_assert(offsetof(LibcxxAssocSubState, sharedOwners) == 8 && offsetof(LibcxxAssocSubState, exception) == 16);
static_assert(offsetof(LibcxxAssocSubState, mutex) == 24 && offsetof(LibcxxAssocSubState, condition) == 32);
static_assert(offsetof(LibcxxAssocSubState, state) == 40);
static_assert(sizeof(LibcxxFutureErrorObject) == 32 && offsetof(LibcxxFutureErrorObject, code) == 16);
static_assert(sizeof(LibcxxPromiseVoid) == 8 && sizeof(LibcxxFutureVoid) == 8);

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "libc++ future check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

static const std::string BrokenText = "The associated promise has been destructed prior to the associated state becoming ready.";
static const std::string RetrievedText = "The future has already been retrieved from the promise or packaged_task.";
static const std::string SatisfiedText = "The state of the promise has already been set.";
static const std::string NoStateText = "Operation not permitted on an object without an associated state.";

static const void* Slot(const void* vtable, int index) { return static_cast<const void* const*>(vtable)[index]; }

static const LibcException::TypeRecord* TypeOf(const void* vtable) {
    return static_cast<const LibcException::TypeRecord*>(Slot(vtable, -1));
}

static const char* KindOf(const LibcException::TypeRecord* type) {
    return static_cast<const LibcException::TypeRecord*>(type->vtable[-1])->name;
}

static std::string Text(const LibcxxString& value) {
    if ((value.bytes[0] & 1) == 0) return std::string(reinterpret_cast<const char*>(value.bytes + 1), value.bytes[0] >> 1);
    std::uint64_t words[3] {};
    std::memcpy(words, value.bytes, sizeof(words));
    std::string result(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(words[2])), words[1]);
    _ZdlPv_nid_postfix(reinterpret_cast<void*>(static_cast<std::uintptr_t>(words[2])));
    return result;
}

static std::string CategoryMessage(const LibcException::CategoryObject* category, int value) {
    LibcxxString result;
    std::memset(&result, 0xcd, sizeof(result));
    REQUIRE(std::bit_cast<Message>(Slot(category->vtable, 6))(&result, category, value) == &result);
    return Text(result);
}

static std::string WhatOf(const LibcxxFutureErrorObject* object) {
    return std::bit_cast<What>(Slot(object->base.vtable, 2))(&object->base);
}

template <typename TAction>
static int FutureErrorCode(TAction&& action) {
    try { action(); }
    catch (const std::logic_error& error) {
        const auto* object = reinterpret_cast<const LibcxxFutureErrorObject*>(&error);
        REQUIRE(TypeOf(object->base.vtable) == &_ZTINSt3__112future_errorE_nid_postfix);
        REQUIRE(object->code.category == _ZNSt3__115future_categoryEv_nid_postfix());
        static const std::string texts[] {BrokenText, RetrievedText, SatisfiedText, NoStateText, BrokenText};
        REQUIRE(object->code.value >= 1 && object->code.value <= 4 && WhatOf(object) == texts[object->code.value]);
        return object->code.value;
    }
    return 0;
}

static std::atomic<int> deletes {0};
static std::atomic<int> executes {0};

static void APS5_VABI CountingDelete(LibcxxAssocSubState* state) {
    ++deletes;
    _ZTVNSt3__117__assoc_sub_stateE_nid_postfix.deleteObject(state);
}

static void APS5_VABI DeferredExecute(LibcxxAssocSubState* state) {
    ++executes;
    REQUIRE(pthread_mutex_trylock_nid_postfix(&state->mutex) == 0);
    REQUIRE(pthread_mutex_unlock_nid_postfix(&state->mutex) == 0);
    _ZNSt3__117__assoc_sub_state9set_valueEv_nid_postfix(state);
}

static const LibcxxAssocSubStateVtable CountingVtable {
    0, &_ZTINSt3__117__assoc_sub_stateE_nid_postfix, _ZTVNSt3__117__assoc_sub_stateE_nid_postfix.destroy, CountingDelete,
    _ZNSt3__117__assoc_sub_state16__on_zero_sharedEv_nid_postfix, DeferredExecute,
};

static LibcxxAssocSubState* NewCountingState(std::uint32_t flags) {
    void* storage = _Znwm_nid_postfix(sizeof(LibcxxAssocSubState));
    return new (storage) LibcxxAssocSubState {&CountingVtable.destroy, 0, {nullptr}, nullptr, nullptr, flags};
}

static void CheckTypes() {
    const auto& table = _ZTVNSt3__117__assoc_sub_stateE_nid_postfix;
    REQUIRE(table.offset == 0 && table.type == &_ZTINSt3__117__assoc_sub_stateE_nid_postfix);
    REQUIRE(table.destroy && table.deleteObject && table.onZeroShared && table.execute);
    deletes = 0;
    table.onZeroShared(NewCountingState(0));
    REQUIRE(deletes == 1);
    LibcxxAssocSubState* state = NewCountingState(0);
    REQUIRE(FutureErrorCode([&] { table.execute(state); }) == 3);
    table.deleteObject(state);
    REQUIRE(deletes == 1);
    const auto& type = _ZTINSt3__117__assoc_sub_stateE_nid_postfix;
    REQUIRE(std::strcmp(type.name, "NSt3__117__assoc_sub_stateE") == 0);
    REQUIRE(std::strcmp(KindOf(&type), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(type.base && std::strcmp(type.base->name, "NSt3__114__shared_countE") == 0 && !type.base->base);
    REQUIRE(std::strcmp(KindOf(type.base), "N10__cxxabiv117__class_type_infoE") == 0);
    const auto& error = _ZTINSt3__112future_errorE_nid_postfix;
    REQUIRE(std::strcmp(error.name, "NSt3__112future_errorE") == 0);
    REQUIRE(std::strcmp(KindOf(&error), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(error.base && std::strcmp(error.base->name, "St11logic_error") == 0);
}

static void CheckCategory() {
    const auto* category = _ZNSt3__115future_categoryEv_nid_postfix();
    REQUIRE(category && category == _ZNSt3__115future_categoryEv_nid_postfix());
    REQUIRE(category != _ZNSt3__116generic_categoryEv_nid_postfix());
    const auto* type = TypeOf(category->vtable);
    REQUIRE(std::strcmp(type->name, "NSt3__123__future_error_categoryE") == 0);
    REQUIRE(type->base && std::strcmp(type->base->name, "NSt3__112__do_messageE") == 0);
    REQUIRE(type->base->base == &_ZTINSt3__114error_categoryE_nid_postfix);
    REQUIRE(std::strcmp(std::bit_cast<Name>(Slot(category->vtable, 2))(category), "future") == 0);
    REQUIRE(CategoryMessage(category, 0) == BrokenText);
    REQUIRE(CategoryMessage(category, 1) == RetrievedText);
    REQUIRE(CategoryMessage(category, 2) == SatisfiedText);
    REQUIRE(CategoryMessage(category, 3) == NoStateText);
    REQUIRE(CategoryMessage(category, 4) == BrokenText);
    REQUIRE(CategoryMessage(category, 5) == "unspecified future_errc value\n");
    REQUIRE(CategoryMessage(category, -1) == "unspecified future_errc value\n");
}

static void CheckFutureError() {
    LibcxxFutureErrorObject object;
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__112future_errorC1ENS_10error_codeE_nid_postfix(&object, {3, 0, _ZNSt3__115future_categoryEv_nid_postfix()});
    REQUIRE(TypeOf(object.base.vtable) == &_ZTINSt3__112future_errorE_nid_postfix);
    REQUIRE(WhatOf(&object) == NoStateText && object.code.value == 3);
    _ZNSt3__112future_errorD1Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr);
    _ZNSt3__112future_errorC1ENS_10error_codeE_nid_postfix(&object, {2, 0, _ZNSt3__116generic_categoryEv_nid_postfix()});
    REQUIRE(WhatOf(&object) == "No such file or directory");
    _ZNSt3__112future_errorD1Ev_nid_postfix(&object);
    bool rejected = false;
    try { _ZNSt3__112future_errorC1ENS_10error_codeE_nid_postfix(&object, {1, 0, nullptr}); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static void CheckValue() {
    LibcxxPromiseVoid promise {};
    _ZNSt3__17promiseIvEC1Ev_nid_postfix(&promise);
    LibcxxAssocSubState* state = promise.state;
    REQUIRE(state && state->vtable == &_ZTVNSt3__117__assoc_sub_stateE_nid_postfix.destroy);
    REQUIRE(state->sharedOwners == 0 && state->state == 0 && !state->exception.object);
    LibcxxFutureVoid future {};
    REQUIRE(_ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&future, &promise) == &future);
    REQUIRE(future.state == state && state->sharedOwners == 1 && state->state == 2);
    LibcxxFutureVoid second {};
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&second, &promise); }) == 1);
    REQUIRE(state->sharedOwners == 1);

    std::atomic<bool> waiting {false};
    std::atomic<bool> set {false};
    std::thread worker([&] {
        while (!waiting.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        set.store(true);
        _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(&promise);
    });
    waiting.store(true);
    _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(state);
    REQUIRE(set.load());
    worker.join();
    REQUIRE(state->state == 7 && !state->exception.object);
    _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(state);

    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(&promise); }) == 2);
    GuestLibcxxExceptionPointer empty {nullptr};
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr_nid_postfix(&promise, &empty); }) == 2);
    REQUIRE(FutureErrorCode([&] { _ZNSt3__117__assoc_sub_state9set_valueEv_nid_postfix(state); }) == 2);
    REQUIRE(pthread_mutex_trylock_nid_postfix(&state->mutex) == 0);
    REQUIRE(pthread_mutex_unlock_nid_postfix(&state->mutex) == 0);

    _ZNSt3__17promiseIvED1Ev_nid_postfix(&promise);
    REQUIRE(state->sharedOwners == 0 && !state->exception.object);
    _ZNSt3__16futureIvED1Ev_nid_postfix(&future);
}

static void CheckException() {
    LibcxxPromiseVoid promise {};
    _ZNSt3__17promiseIvEC1Ev_nid_postfix(&promise);
    LibcxxFutureVoid future {};
    _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&future, &promise);
    void* object = __cxa_allocate_exception_nid_postfix(sizeof(int));
    *static_cast<int*>(object) = 41;
    auto* header = LibcException::FromObject(object);
    header->type = const_cast<std::type_info*>(&typeid(int));
    auto* allocation = LibcException::AllocationOf(header);
    GuestLibcxxExceptionPointer owner {object};
    _ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr_nid_postfix(&promise, &owner);
    REQUIRE(promise.state->exception.object == object && allocation->references.load() == 2);
    REQUIRE(promise.state->state == 6);
    _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(future.state);
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr_nid_postfix(&promise, &owner); }) == 2);
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(&promise); }) == 2);
    REQUIRE(allocation->references.load() == 2);
    _ZNSt13exception_ptrD1Ev_nid_postfix(&owner);
    int rethrown = 0;
    try { _ZSt17rethrow_exceptionSt13exception_ptr_nid_postfix(&future.state->exception); }
    catch (int value) { rethrown = value; }
    REQUIRE(rethrown == 41 && allocation->references.load() == 1);
    _ZNSt3__16futureIvED1Ev_nid_postfix(&future);
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&promise);
}

static void CheckBrokenPromise() {
    LibcxxPromiseVoid promise {};
    promise.state = NewCountingState(0);
    LibcxxFutureVoid future {};
    _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&future, &promise);
    LibcxxAssocSubState* state = future.state;
    deletes = 0;
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&promise);
    REQUIRE(deletes == 0 && state->sharedOwners == 0 && state->state == 6 && state->exception.object);
    _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(state);
    int code = 0;
    try { _ZSt17rethrow_exceptionSt13exception_ptr_nid_postfix(&state->exception); }
    catch (const std::logic_error& error) {
        const auto* object = reinterpret_cast<const LibcxxFutureErrorObject*>(&error);
        REQUIRE(TypeOf(object->base.vtable) == &_ZTINSt3__112future_errorE_nid_postfix && WhatOf(object) == BrokenText);
        REQUIRE(object->code.category == _ZNSt3__115future_categoryEv_nid_postfix());
        code = object->code.value;
    }
    REQUIRE(code == 4);
    REQUIRE(LibcException::AllocationOf(LibcException::FromObject(state->exception.object))->references.load() == 1);
    _ZNSt3__16futureIvED1Ev_nid_postfix(&future);
    REQUIRE(deletes == 1);

    LibcxxPromiseVoid alone {};
    alone.state = NewCountingState(0);
    deletes = 0;
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&alone);
    REQUIRE(deletes == 1);

    LibcxxPromiseVoid satisfied {};
    satisfied.state = NewCountingState(0);
    _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&future, &satisfied);
    _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(&satisfied);
    deletes = 0;
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&satisfied);
    REQUIRE(deletes == 0 && !future.state->exception.object && future.state->state == 7);
    _ZNSt3__16futureIvED1Ev_nid_postfix(&future);
    REQUIRE(deletes == 1);
}

static void CheckNoState() {
    LibcxxPromiseVoid promise {nullptr};
    LibcxxFutureVoid future {};
    GuestLibcxxExceptionPointer empty {nullptr};
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE10get_futureEv_nid_postfix(&future, &promise); }) == 3);
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE9set_valueEv_nid_postfix(&promise); }) == 3);
    REQUIRE(FutureErrorCode([&] { _ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr_nid_postfix(&promise, &empty); }) == 3);
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&promise);
    _ZNSt3__16futureIvED1Ev_nid_postfix(&future);
    bool rejected = false;
    try { _ZNSt3__16futureIvEC1EPNS_17__assoc_sub_stateE_nid_postfix(&future, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static void CheckDeferred() {
    LibcxxAssocSubState* state = NewCountingState(8);
    GuestUniqueLock lock {&state->mutex, true};
    _ZNSt3__15mutex4lockEv_nid_postfix(&state->mutex);
    executes = 0;
    _ZNSt3__117__assoc_sub_state10__sub_waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(state, &lock);
    REQUIRE(executes == 1 && !lock.owns && state->state == 5);
    _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(state);
    REQUIRE(executes == 1);

    state->state = 8;
    lock.owns = false;
    int code = 0;
    try { _ZNSt3__117__assoc_sub_state10__sub_waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(state, &lock); }
    catch (const std::runtime_error& error) {
        const auto* object = reinterpret_cast<const LibcException::LibcxxSystemErrorObject*>(&error);
        REQUIRE(TypeOf(object->base.vtable) == &_ZTINSt3__112system_errorE_nid_postfix);
        REQUIRE(std::string(std::bit_cast<What>(Slot(object->base.vtable, 2))(&object->base)).starts_with("unique_lock::unlock: not locked"));
        code = object->code.value;
    }
    REQUIRE(code == 1 && state->state == 0 && executes == 1);

    deletes = 0;
    _ZNSt3__117__assoc_sub_state16__on_zero_sharedEv_nid_postfix(state);
    REQUIRE(deletes == 1);

    LibcxxPromiseVoid promise {};
    _ZNSt3__17promiseIvEC1Ev_nid_postfix(&promise);
    promise.state->state |= 8;
    REQUIRE(FutureErrorCode([&] { _ZNSt3__117__assoc_sub_state4waitEv_nid_postfix(promise.state); }) == 3);
    REQUIRE((promise.state->state & 8) == 0);
    REQUIRE(pthread_mutex_trylock_nid_postfix(&promise.state->mutex) == 0);
    REQUIRE(pthread_mutex_unlock_nid_postfix(&promise.state->mutex) == 0);
    _ZNSt3__17promiseIvED1Ev_nid_postfix(&promise);
}

int main() {
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckTypes();
    CheckCategory();
    CheckFutureError();
    CheckValue();
    CheckException();
    CheckBrokenPromise();
    CheckNoState();
    CheckDeferred();
    std::puts("libc++ promise<void>, future<void> and __assoc_sub_state checks passed");
    return 0;
}
