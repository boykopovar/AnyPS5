#pragma once

#include "prx/libc/include/GuestLocale.hpp"

extern "C" {

char APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(const GuestLocale::CtypeFacet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(
    const GuestLocale::CtypeFacet* self, char* first, const char* last);
char APS5_VABI _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(const GuestLocale::CtypeFacet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(
    const GuestLocale::CtypeFacet* self, char* first, const char* last);

}
