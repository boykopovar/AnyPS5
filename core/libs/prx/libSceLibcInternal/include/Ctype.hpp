#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

char APS5_VABI sceLibcInternalCtypeDoTolower_nid_no_patch(const GuestLocale::CtypeFacet* self, char character);
const char* APS5_VABI sceLibcInternalCtypeDoTolowerRange_nid_no_patch(const GuestLocale::CtypeFacet* self, char* first, const char* last);
char APS5_VABI sceLibcInternalCtypeDoToupper_nid_no_patch(const GuestLocale::CtypeFacet* self, char character);
const char* APS5_VABI sceLibcInternalCtypeDoToupperRange_nid_no_patch(const GuestLocale::CtypeFacet* self, char* first, const char* last);
void APS5_VABI sceLibcInternalCtypeDestroy_nid_no_patch(GuestLocale::CtypeFacet* self);
void APS5_VABI sceLibcInternalCtypeDelete_nid_no_patch(GuestLocale::CtypeFacet* self);
std::uint16_t APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(const GuestLocale::Facet* self, std::uint16_t character);
const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEPwPKw_nid_postfix(const GuestLocale::Facet* self, std::uint16_t* first, const std::uint16_t* last);
std::uint16_t APS5_VABI _ZNKSt5ctypeIwE8do_widenEc_nid_postfix(const GuestLocale::Facet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(const GuestLocale::Facet* self, const char* first, const char* last, std::uint16_t* destination);
char APS5_VABI _ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(const GuestLocale::Facet* self, std::uint16_t character, char fallback);
const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(const GuestLocale::Facet* self, const std::uint16_t* first, const std::uint16_t* last, char fallback, char* destination);

}
