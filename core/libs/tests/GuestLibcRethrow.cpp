#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <csetjmp>
#include <typeinfo>

thread_local int destroyed = 0;
struct Tracked {
    int value;
    ~Tracked() { ++destroyed; }
};

thread_local int terminateRan = 0;
thread_local std::jmp_buf terminateJump;

void CustomTerminate() {
    terminateRan = 1;
    std::longjmp(terminateJump, 1);
}

struct UnwindObserver {
    unsigned expectedUncaught;
    ~UnwindObserver() {
        assert(__cxa_uncaught_exceptions_nid_postfix() == expectedUncaught);
    }
};

void CheckRethrowInCatch() {
    destroyed = 0;
    const void* innerAddr = nullptr;
    try {
        UnwindObserver outerGuard{1};
        try {
            throw Tracked{42};
        } catch (const Tracked& caught) {
            assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
            innerAddr = &caught;
            __cxa_rethrow_nid_postfix();
        }
    } catch (const Tracked& outer) {
        assert(&outer == innerAddr);
        assert(destroyed == 0);
    }
    assert(destroyed == 1);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
}

void CheckRethrowNoActiveException() {
    terminateRan = 0;
    void (*oldHandler)() = _ZSt13set_terminatePFvvE_nid_postfix(CustomTerminate);
    assert(oldHandler != nullptr);

    if (setjmp(terminateJump) == 0) {
        __cxa_rethrow_nid_postfix();
        assert(false);
    }

    assert(terminateRan == 1);
    _ZSt13set_terminatePFvvE_nid_postfix(oldHandler);
}

void CheckDependentExceptionAllocateFree() {
    void* storage = __cxa_allocate_dependent_exception_nid_postfix();
    assert(storage != nullptr);

    auto* header = static_cast<LibcException::Header*>(storage);
    assert(header->type == nullptr);
    assert(header->destructor == nullptr);
    assert(header->unexpected == nullptr);
    assert(header->terminate == nullptr);
    assert(header->next == nullptr);
    assert(header->handlers == 0);
    assert(header->selector == 0);
    assert(header->_pad == 0);
    assert(header->_pad2 == 0);
    assert(header->action == nullptr);
    assert(header->lsda == nullptr);
    assert(header->landing == 0);
    assert(header->adjusted == nullptr);
    assert(header->unwind.exception_class == 0);
    assert(header->unwind.exception_cleanup == nullptr);

    __cxa_free_dependent_exception_nid_postfix(storage);
    __cxa_free_dependent_exception_nid_postfix(nullptr);
}

void CheckRethrowPrimaryException() {
    destroyed = 0;
    void* primary = nullptr;

    try {
        throw Tracked{99};
    } catch (const Tracked&) {
        primary = __cxa_current_primary_exception_nid_postfix();
        assert(primary != nullptr);
        assert(destroyed == 0);
    }

    assert(destroyed == 0);

    __cxa_increment_exception_refcount_nid_postfix(primary);
    assert(destroyed == 0);

    try {
        __cxa_rethrow_primary_exception_nid_postfix(primary);
        assert(false);
    } catch (const Tracked& caught) {
        assert(caught.value == 99);
        assert(&caught == primary);
        assert(destroyed == 0);
    }

    assert(destroyed == 0);

    __cxa_decrement_exception_refcount_nid_postfix(primary);
    assert(destroyed == 0);

    __cxa_decrement_exception_refcount_nid_postfix(primary);
    assert(destroyed == 1);

    terminateRan = 0;
    void (*oldHandler)() = _ZSt13set_terminatePFvvE_nid_postfix(CustomTerminate);
    assert(oldHandler != nullptr);
    if (setjmp(terminateJump) == 0) {
        __cxa_rethrow_primary_exception_nid_postfix(nullptr);
    } else {
        assert(false);
    }
    assert(terminateRan == 0);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    _ZSt13set_terminatePFvvE_nid_postfix(oldHandler);
}

int main() {
    CheckRethrowInCatch();
    CheckRethrowNoActiveException();
    CheckDependentExceptionAllocateFree();
    CheckRethrowPrimaryException();
    std::puts("guest libc rethrow tests passed");
    return 0;
}
