#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <typeinfo>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void Rethrow(GuestExceptionPointer* pointer, int expected) {
    bool caught = false;
    try {
        _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(*pointer);
    } catch (int value) {
        caught = value == expected;
    }
    Require(caught, "exception_ptr did not rethrow its guest exception");
}

std::atomic<int> destructorCalls{0};

void APS5_VABI TestDestructor(void*) {
    ++destructorCalls;
}

void TestCurrentExceptionInCatch() {
    destructorCalls.store(0);
    GuestExceptionPointer outside(_ZSt17current_exceptionv_nid_postfix());
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&outside) == nullptr, "current_exception outside catch returned non-null");

    struct Counted {
        int value;
        ~Counted() { ++destructorCalls; }
    };

    void* caughtAddr = nullptr;
    GuestExceptionPointer retained(nullptr);
    try {
        throw Counted{99};
    } catch (const Counted& value) {
        caughtAddr = const_cast<Counted*>(&value);
        GuestExceptionPointer captured(_ZSt17current_exceptionv_nid_postfix());
        Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&captured) == caughtAddr, "current_exception inside catch returned wrong pointer");
        _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&retained, caughtAddr);
        _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&captured);
        Require(destructorCalls.load() == 0, "destructor ran prematurely inside catch");
    }
    Require(destructorCalls.load() == 0, "retained current_exception did not survive end of catch");

    GuestExceptionPointer extra(nullptr);
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&extra, caughtAddr);
    Require(destructorCalls.load() == 0, "extra copy did not preserve object");
    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&retained);
    Require(destructorCalls.load() == 0, "object destroyed before final reference released");

    bool rethrownSame = false;
    try {
        _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(extra);
    } catch (const Counted& value) {
        rethrownSame = (&value == caughtAddr && value.value == 99);
    }
    Require(rethrownSame, "rethrow_exception did not rethrow same object");
    Require(destructorCalls.load() == 0, "object destroyed while extra reference held");
    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&extra);
    Require(destructorCalls.load() == 1, "object not destroyed exactly once on final release");
    destructorCalls.store(0);
}

}

int main() {
    TestCurrentExceptionInCatch();

    void* object = __cxa_allocate_exception_nid_postfix(sizeof(int));
    *static_cast<int*>(object) = 42;
    auto* header = LibcException::FromObject(object);
    header->type = const_cast<std::type_info*>(&typeid(int));
    header->destructor = reinterpret_cast<decltype(header->destructor)>(TestDestructor);
    auto* allocation = LibcException::AllocationOf(header);

    GuestExceptionPointer owner(nullptr);
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&owner, object);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&owner) == object, "exception_ptr construction did not store object");
    Require(allocation->references.load() == 2, "exception_ptr construction did not increment refcount");

    GuestExceptionPointer copy(nullptr);
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&copy, object);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&copy) == object && allocation->references.load() == 3, "second exception_ptr construction did not increment refcount");

    _ZNSt15__exception_ptr13exception_ptr9_M_addrefEv_nid_postfix(&copy);
    Require(allocation->references.load() == 4, "_M_addref did not increment refcount");

    Rethrow(&copy, 42);
    Require(allocation->references.load() == 4, "rethrow leaked a guest exception reference");

    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&owner);
    Require(allocation->references.load() == 3, "_M_release did not decrement refcount");
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&owner) == nullptr, "_M_release did not nullify owner");

    Rethrow(&copy, 42);
    Require(allocation->references.load() == 3, "copy did not survive owner release");

    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&copy);
    Require(allocation->references.load() == 2, "copy _M_release did not decrement refcount");
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&copy) == nullptr, "copy _M_release did not nullify copy");

    __cxa_decrement_exception_refcount_nid_postfix(object);
    Require(allocation->references.load() == 1, "final decrement did not bring refcount to 1");
    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&copy);
    Require(allocation->references.load() == 1, "releasing null exception_ptr altered refcount");
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&copy) == nullptr, "double _M_release corrupted pointer");

    Require(destructorCalls.load() == 0, "destructor ran before final release");
    __cxa_decrement_exception_refcount_nid_postfix(object);
    Require(destructorCalls.load() == 1, "destructor not called exactly once on final release");
    std::puts("exception_ptr copy, addref, release, rethrow and destruction passed");
    return 0;
}
