#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;

namespace Icu {
struct Normalizer2;
} // namespace Icu

extern "C" {
Icu::Normalizer2* APS5_VABI unorm2_openFiltered_nid_postfix(const Icu::Normalizer2*, const USet*, std::int32_t*);
void APS5_VABI unorm2_close_nid_postfix(Icu::Normalizer2*);
const Icu::Normalizer2* APS5_VABI unorm2_getNFKCInstance(std::int32_t*);
const Icu::Normalizer2* APS5_VABI unorm2_getNFKDInstance(std::int32_t*);
const Icu::Normalizer2* APS5_VABI unorm2_getNFKCCasefoldInstance_nid_postfix(std::int32_t*);
std::int32_t APS5_VABI u_getFC_NFKC_Closure_nid_postfix(std::int32_t, char16_t*, std::int32_t, std::int32_t*);
const Icu::Normalizer2* APS5_VABI unorm2_getNFCInstance_nid_postfix(std::int32_t*);
const Icu::Normalizer2* APS5_VABI unorm2_getNFDInstance_nid_postfix(std::int32_t*);
const Icu::Normalizer2* APS5_VABI unorm2_getInstance_nid_postfix(const char*, const char*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI unorm2_normalize_nid_postfix(const Icu::Normalizer2*, const char16_t*, std::int32_t, char16_t*, std::int32_t,
                                                    std::int32_t*);
std::int32_t APS5_VABI unorm2_normalizeSecondAndAppend_nid_postfix(const Icu::Normalizer2*, char16_t*, std::int32_t, std::int32_t,
                                                                   const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI unorm2_append_nid_postfix(const Icu::Normalizer2*, char16_t*, std::int32_t, std::int32_t, const char16_t*,
                                                 std::int32_t, std::int32_t*);
std::int32_t APS5_VABI unorm2_getDecomposition_nid_postfix(const Icu::Normalizer2*, std::int32_t, char16_t*, std::int32_t,
                                                           std::int32_t*);
std::int32_t APS5_VABI unorm2_getRawDecomposition_nid_postfix(const Icu::Normalizer2*, std::int32_t, char16_t*, std::int32_t,
                                                              std::int32_t*);
std::int32_t APS5_VABI unorm2_composePair_nid_postfix(const Icu::Normalizer2*, std::int32_t, std::int32_t);
std::uint8_t APS5_VABI unorm2_getCombiningClass_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int8_t APS5_VABI unorm2_isNormalized_nid_postfix(const Icu::Normalizer2*, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI unorm2_quickCheck_nid_postfix(const Icu::Normalizer2*, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI unorm2_spanQuickCheckYes_nid_postfix(const Icu::Normalizer2*, const char16_t*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI unorm2_hasBoundaryBefore_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int8_t APS5_VABI unorm2_hasBoundaryAfter_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int8_t APS5_VABI unorm2_isInert_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int32_t APS5_VABI unorm_compare_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::uint32_t,
                                                 std::int32_t*);
}
