#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

namespace {

struct NestedExceptionLayout {
    const void* vtable;
    GuestExceptionPointer ptr;
};

}

extern "C" {

void APS5_VABI _ZNSt16nested_exceptionD2Ev_nid_postfix(void* self) {
    if (!self) {
        return;
    }
    auto* obj = static_cast<NestedExceptionLayout*>(self);
    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&obj->ptr);
}

void APS5_VABI _ZNSt16nested_exceptionD1Ev_nid_postfix(void* self) {
    _ZNSt16nested_exceptionD2Ev_nid_postfix(self);
}

void APS5_VABI _ZNSt16nested_exceptionD0Ev_nid_postfix(void* self) {
    if (!self) {
        return;
    }
    _ZNSt16nested_exceptionD1Ev_nid_postfix(self);
    _ZdlPv_nid_postfix(self);
}

}
