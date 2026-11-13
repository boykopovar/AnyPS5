#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceLibcInternalWideCollateDoCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char16_t* first1, const char16_t* last1, const char16_t* first2, const char16_t* last2);
GuestLocale::String* APS5_VABI sceLibcInternalWideCollateDoTransform_nid_no_patch(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char16_t* first, const char16_t* last);
std::int64_t APS5_VABI sceLibcInternalWideCollateDoHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char16_t* first, const char16_t* last);
int APS5_VABI sceLibcInternalWideCollateCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char16_t* first1, const char16_t* last1, const char16_t* first2, const char16_t* last2);
GuestLocale::String* APS5_VABI sceLibcInternalWideCollateTransform_nid_no_patch(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char16_t* first, const char16_t* last);
std::int64_t APS5_VABI sceLibcInternalWideCollateHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char16_t* first, const char16_t* last);

}
