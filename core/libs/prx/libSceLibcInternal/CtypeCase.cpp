#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

char APS5_VABI sceLibcInternalCtypeDoTolower_nid_no_patch(const GuestLocale::CtypeFacet* self, char character) {
    return _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(self, character);
}
APS5_EXPORT("WXOoCK+kqwY", sceLibcInternalCtypeDoTolower_nid_no_patch);

const char* APS5_VABI sceLibcInternalCtypeDoTolowerRange_nid_no_patch(const GuestLocale::CtypeFacet* self, char* first, const char* last) {
    return _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(self, first, last);
}
APS5_EXPORT("2w+4Mo2DPro", sceLibcInternalCtypeDoTolowerRange_nid_no_patch);

char APS5_VABI sceLibcInternalCtypeDoToupper_nid_no_patch(const GuestLocale::CtypeFacet* self, char character) {
    return _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(self, character);
}
APS5_EXPORT("mnq3tbhCs34", sceLibcInternalCtypeDoToupper_nid_no_patch);

const char* APS5_VABI sceLibcInternalCtypeDoToupperRange_nid_no_patch(const GuestLocale::CtypeFacet* self, char* first, const char* last) {
    return _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(self, first, last);
}
APS5_EXPORT("7glioH0t9HM", sceLibcInternalCtypeDoToupperRange_nid_no_patch);

void APS5_VABI sceLibcInternalCtypeDestroy_nid_no_patch(GuestLocale::CtypeFacet* self) {
    _ZNSt5ctypeIcED1Ev_nid_postfix(self);
}
APS5_EXPORT("tPsGA6EzNKA", sceLibcInternalCtypeDestroy_nid_no_patch);

void APS5_VABI sceLibcInternalCtypeDelete_nid_no_patch(GuestLocale::CtypeFacet* self) {
    _ZNSt5ctypeIcED0Ev_nid_postfix(self);
}
APS5_EXPORT("p8-44cVeO84", sceLibcInternalCtypeDelete_nid_no_patch);

}
