#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
}
APS5_EXPORT("X9D8WWSG3As", sceLibcInternalIosBaseCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("I5jcbATyIWo", sceLibcInternalIosBaseDeletingDestructor_nid_no_patch);

}
