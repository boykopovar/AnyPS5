#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct CaseMap;
} // namespace Icu

extern "C" {
std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*);
std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);
std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);
std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*);
std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t*, const char16_t*, std::uint32_t);
std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t, std::uint32_t);
std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t, std::uint32_t);
void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t, std::int32_t*, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_687CaseMap7toLowerEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(const char*, std::uint32_t, const char16_t*, std::int32_t, char16_t*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_687CaseMap7toUpperEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(const char*, std::uint32_t, const char16_t*, std::int32_t, char16_t*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI _ZN6icu_687CaseMap4foldEjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(std::uint32_t, const char16_t*, std::int32_t, char16_t*, std::int32_t, void*, std::int32_t*);
Icu::CaseMap* APS5_VABI ucasemap_open_nid_postfix(const char*, std::uint32_t, std::int32_t*);
std::int32_t APS5_VABI ucasemap_utf8FoldCase_nid_postfix(const Icu::CaseMap*, char*, std::int32_t, const char*,
                                                         std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ucasemap_utf8ToUpper_nid_postfix(const Icu::CaseMap*, char*, std::int32_t, const char*,
                                                        std::int32_t, std::int32_t*);
}
