#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;

namespace Icu {
struct Normalizer2;
struct Normalizer2Impl;
} // namespace Icu

extern "C" {
const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer214getNFCInstanceER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer214getNFDInstanceER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer223getNFKCCasefoldInstanceER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2* APS5_VABI _ZN6icu_6818Normalizer2Factory11getInstanceE18UNormalizationModeR10UErrorCode_nid_postfix(
    std::int32_t, std::int32_t*);
const Icu::Normalizer2* APS5_VABI _ZN6icu_6818Normalizer2Factory15getNoopInstanceER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory11getNFKCImplER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory14getNFKC_CFImplER10UErrorCode_nid_postfix(std::int32_t*);
const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(const Icu::Normalizer2*);
std::int32_t APS5_VABI _ZNK6icu_6811Normalizer211composePairEii_nid_postfix(const Icu::Normalizer2*, std::int32_t, std::int32_t);
std::uint8_t APS5_VABI _ZNK6icu_6811Normalizer217getCombiningClassEi_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(const Icu::Normalizer2*,
                                                                                                         const char*, std::int32_t,
                                                                                                         std::int32_t*);
std::int32_t APS5_VABI _ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(const Icu::Normalizer2Impl*, std::int32_t,
                                                                               std::int32_t);
const char16_t* APS5_VABI _ZNK6icu_6815Normalizer2Impl16getDecompositionEiPDsRi_nid_postfix(const Icu::Normalizer2Impl*,
                                                                                          std::int32_t, char16_t*,
                                                                                          std::int32_t*);
const char16_t* APS5_VABI _ZNK6icu_6815Normalizer2Impl19getRawDecompositionEiPDsRi_nid_postfix(const Icu::Normalizer2Impl*,
                                                                                             std::int32_t, char16_t*,
                                                                                             std::int32_t*);
std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl23hasDecompBoundaryBeforeEi_nid_postfix(const Icu::Normalizer2Impl*, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl29norm16HasDecompBoundaryBeforeEt_nid_postfix(const Icu::Normalizer2Impl*,
                                                                                              std::uint16_t);
std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl19ensureCanonIterDataER10UErrorCode_nid_postfix(const Icu::Normalizer2Impl*,
                                                                                                 std::int32_t*);
void APS5_VABI _ZNK6icu_6815Normalizer2Impl12addLcccCharsERNS_10UnicodeSetE_nid_postfix(const Icu::Normalizer2Impl*, USet*);
}
