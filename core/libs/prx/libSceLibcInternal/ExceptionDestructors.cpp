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

void APS5_VABI sceLibcInternalFutureErrorCategoryDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("WDKzMM-uuLE", sceLibcInternalFutureErrorCategoryDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalSystemErrorCategoryDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("Dc4ZMWmPMl8", sceLibcInternalSystemErrorCategoryDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalSystemErrorCategoryDestructor_nid_no_patch(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
}
APS5_EXPORT("hVQgfGhJz3U", sceLibcInternalSystemErrorCategoryDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalGenericErrorCategoryDestructor_nid_no_patch(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
}
APS5_EXPORT("MAalgQhejPc", sceLibcInternalGenericErrorCategoryDestructor_nid_no_patch);

void APS5_VABI _ZNSt22_Future_error_categoryD1Ev_nid_postfix(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
}

void APS5_VABI _ZNSt23_Generic_error_categoryD0Ev_nid_postfix(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZNSt18bad_variant_accessD1Ev_nid_postfix(void* self) {
    _ZNSt9exceptionD2Ev_nid_postfix(static_cast<LibcException::ExceptionObject*>(self));
}

void APS5_VABI _ZNSt18bad_variant_accessD0Ev_nid_postfix(void* self) {
    _ZNSt9exceptionD2Ev_nid_postfix(static_cast<LibcException::ExceptionObject*>(self));
    _ZdlPv_nid_postfix(self);
}

[[noreturn]] void APS5_VABI sceLibcInternalXoverflowError_nid_no_patch(const char* message) {
    LibcThrowOverflowError_nid_no_patch(message);
}
APS5_EXPORT("saSUnPWLq9E", sceLibcInternalXoverflowError_nid_no_patch);

}
