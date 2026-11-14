#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

char APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(const GuestLocale::Facet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(const GuestLocale::Facet* self, char* first, const char* last);
char APS5_VABI _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(const GuestLocale::Facet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(const GuestLocale::Facet* self, char* first, const char* last);
std::uint16_t APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(const GuestLocale::Facet* self, std::uint16_t character);
const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE10do_tolowerEPwPKw_nid_postfix(const GuestLocale::Facet* self, std::uint16_t* first, const std::uint16_t* last);
std::uint16_t APS5_VABI _ZNKSt5ctypeIwE8do_widenEc_nid_postfix(const GuestLocale::Facet* self, char character);
const char* APS5_VABI _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(const GuestLocale::Facet* self, const char* first, const char* last, std::uint16_t* destination);
char APS5_VABI _ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(const GuestLocale::Facet* self, std::uint16_t character, char fallback);
const std::uint16_t* APS5_VABI _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(const GuestLocale::Facet* self, const std::uint16_t* first, const std::uint16_t* last, char fallback, char* destination);

}
