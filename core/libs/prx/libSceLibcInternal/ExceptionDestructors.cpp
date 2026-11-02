#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI sceLibcInternalFutureErrorBaseDestructor_nid_no_patch(void* self) {
    _ZNSt12future_errorD1Ev_nid_postfix(static_cast<LibcException::FutureErrorObject*>(self));
}
APS5_EXPORT("XFh0C66aEms", sceLibcInternalFutureErrorBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalOverflowErrorBaseDestructor_nid_no_patch(void* self) {
    _ZNSt14overflow_errorD1Ev_nid_postfix(static_cast<LibcException::ExceptionObject*>(self));
}
APS5_EXPORT("OypvNf3Uq3c", sceLibcInternalOverflowErrorBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalUnderflowErrorBaseDestructor_nid_no_patch(void* self) {
    _ZNSt15underflow_errorD1Ev_nid_postfix(static_cast<LibcException::ExceptionObject*>(self));
}
APS5_EXPORT("xiqd+QkuYXc", sceLibcInternalUnderflowErrorBaseDestructor_nid_no_patch);

}
