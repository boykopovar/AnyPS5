#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI _ZNSt8ios_baseD1Ev(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
}

void APS5_VABI _ZNSt8ios_baseD0Ev(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
    _ZdlPv_nid_postfix(self);
}

}
