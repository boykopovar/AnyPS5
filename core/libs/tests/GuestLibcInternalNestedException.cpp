#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct NestedExceptionLayout {
    const void* vtable;
    GuestExceptionPointer ptr;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal nested exception: %s\n", message);
    std::abort();
}

}

int main() {
    _ZNSt16nested_exceptionD2Ev_nid_postfix(nullptr);
    _ZNSt16nested_exceptionD1Ev_nid_postfix(nullptr);
    _ZNSt16nested_exceptionD0Ev_nid_postfix(nullptr);

    NestedExceptionLayout baseObj{};
    _ZNSt16nested_exceptionD2Ev_nid_postfix(&baseObj);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&baseObj.ptr) == nullptr, "base destructor leaves pointer null");

    NestedExceptionLayout completeObj{};
    _ZNSt16nested_exceptionD1Ev_nid_postfix(&completeObj);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&completeObj.ptr) == nullptr, "complete destructor leaves pointer null");

    void* memory = _Znwm_nid_postfix(sizeof(NestedExceptionLayout));
    Require(memory != nullptr, "allocation should succeed");
    std::memset(memory, 0, sizeof(NestedExceptionLayout));
    _ZNSt16nested_exceptionD0Ev_nid_postfix(memory);

    return 0;
}
