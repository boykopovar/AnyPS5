#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI sceLibcInternalPadCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt4_PadD2Ev_nid_postfix(static_cast<GuestPad*>(self));
}
APS5_EXPORT("XyJPhPqpzMw", sceLibcInternalPadCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPadConstructorWithName_nid_no_patch(void* self, const char* name) {
    _ZNSt4_PadC2EPKc_nid_postfix(static_cast<GuestPad*>(self), name);
}
APS5_EXPORT("uhnb6dnXOnc", sceLibcInternalPadConstructorWithName_nid_no_patch);

void APS5_VABI sceLibcInternalPadLaunchWithName_nid_no_patch(void* self, const char* name, void** thread) {
    _ZNSt4_Pad7_LaunchEPKcPP7pthread_nid_postfix(static_cast<GuestPad*>(self), name, thread);
}
APS5_EXPORT("PBbZjsL6nfc", sceLibcInternalPadLaunchWithName_nid_no_patch);

void APS5_VABI sceLibcInternalPadLaunchWithNameAttr_nid_no_patch(void* self, const char* name, void** attrSlot, void** thread) {
    _ZNSt4_Pad7_LaunchEPKcPP12pthread_attrPP7pthread_nid_postfix(static_cast<GuestPad*>(self), name, attrSlot, thread);
}
APS5_EXPORT("H7-7Z3ixv-w", sceLibcInternalPadLaunchWithNameAttr_nid_no_patch);

}
