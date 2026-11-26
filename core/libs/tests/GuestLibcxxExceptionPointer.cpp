#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"

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

void Rethrow(const GuestLibcxxExceptionPointer* pointer, int expected) {
    bool caught = false;
    try {
        _ZSt17rethrow_exceptionSt13exception_ptr_nid_postfix(pointer);
    } catch (int value) {
        caught = value == expected;
    }
    Require(caught, "libc++ exception_ptr did not rethrow its guest exception");
}

}

int main() {
    GuestLibcxxExceptionPointer empty{nullptr};
    GuestLibcxxExceptionPointer emptyCopy{};
    _ZNSt13exception_ptrC1ERKS__nid_postfix(&emptyCopy, &empty);
    Require(emptyCopy.object == nullptr, "libc++ exception_ptr copied a null value incorrectly");
    _ZNSt13exception_ptrD1Ev_nid_postfix(&emptyCopy);

    void* object = __cxa_allocate_exception_nid_postfix(sizeof(int));
    *static_cast<int*>(object) = 37;
    auto* header = LibcException::FromObject(object);
    header->type = const_cast<std::type_info*>(&typeid(int));
    auto* allocation = LibcException::AllocationOf(header);
    GuestLibcxxExceptionPointer owner{object};
    GuestLibcxxExceptionPointer copy{};
    _ZNSt13exception_ptrC1ERKS__nid_postfix(&copy, &owner);
    Require(copy.object == object && allocation->references.load() == 2, "libc++ exception_ptr copy did not retain the guest exception");
    Rethrow(&copy, 37);
    Require(allocation->references.load() == 2, "libc++ exception_ptr rethrow leaked a guest exception reference");
    _ZNSt13exception_ptrD1Ev_nid_postfix(&owner);
    Require(allocation->references.load() == 1, "libc++ exception_ptr destructor released the copy's reference");
    Rethrow(&copy, 37);
    Require(allocation->references.load() == 1, "libc++ exception_ptr copy did not survive owner destruction");
    _ZNSt13exception_ptrD1Ev_nid_postfix(&copy);
    std::puts("libc++ exception_ptr copy, destruction and rethrow passed");
    return 0;
}
