#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct BiDi;
using BiDiClassCallback = std::int32_t (APS5_VABI*)(const void*, std::int32_t);
} // namespace Icu

extern "C" {
Icu::BiDi* APS5_VABI ubidi_open_nid_postfix();
Icu::BiDi* APS5_VABI ubidi_openSized_nid_postfix(std::int32_t, std::int32_t, std::int32_t*);
void APS5_VABI ubidi_close_nid_postfix(Icu::BiDi*);
void APS5_VABI ubidi_setPara_nid_postfix(Icu::BiDi*, const char16_t*, std::int32_t, std::uint8_t, std::uint8_t*, std::int32_t*);
std::uint8_t APS5_VABI ubidi_getParaLevel_nid_postfix(const Icu::BiDi*);
std::int32_t APS5_VABI ubidi_countRuns_nid_postfix(Icu::BiDi*, std::int32_t*);
std::int32_t APS5_VABI ubidi_getVisualRun_nid_postfix(Icu::BiDi*, std::int32_t, std::int32_t*, std::int32_t*);
void APS5_VABI ubidi_getLogicalRun_nid_postfix(const Icu::BiDi*, std::int32_t, std::int32_t*, std::uint8_t*);
std::int32_t APS5_VABI ubidi_getBaseDirection_nid_postfix(const char16_t*, std::int32_t);
void APS5_VABI ubidi_reorderVisual_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t*);
void APS5_VABI ubidi_setClassCallback_nid_postfix(Icu::BiDi*, Icu::BiDiClassCallback, const void*, Icu::BiDiClassCallback*,
                                                  const void**, std::int32_t*);
std::int32_t APS5_VABI ubidi_getClass_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ubidi_getDirection_nid_postfix(const Icu::BiDi*);
std::int32_t APS5_VABI ubidi_getLength_nid_postfix(const Icu::BiDi*);
std::int32_t APS5_VABI ubidi_getProcessedLength_nid_postfix(const Icu::BiDi*);
std::int32_t APS5_VABI ubidi_getResultLength_nid_postfix(const Icu::BiDi*);
std::uint8_t APS5_VABI ubidi_getLevelAt_nid_postfix(const Icu::BiDi*, std::int32_t);
std::int32_t APS5_VABI ubidi_getCustomizedClass_nid_postfix(Icu::BiDi*, std::int32_t);
std::int32_t APS5_VABI ubidi_getReorderingMode_nid_postfix(Icu::BiDi*);
std::uint32_t APS5_VABI ubidi_getReorderingOptions_nid_postfix(Icu::BiDi*);
std::int8_t APS5_VABI ubidi_isInverse_nid_postfix(Icu::BiDi*);
std::int8_t APS5_VABI ubidi_isOrderParagraphsLTR_nid_postfix(Icu::BiDi*);
std::int32_t APS5_VABI ubidi_countParagraphs_nid_postfix(Icu::BiDi*);
void APS5_VABI ubidi_orderParagraphsLTR_nid_postfix(Icu::BiDi*, std::int8_t);
void APS5_VABI ubidi_setInverse_nid_postfix(Icu::BiDi*, std::int8_t);
void APS5_VABI ubidi_setReorderingMode_nid_postfix(Icu::BiDi*, std::int32_t);
void APS5_VABI ubidi_setReorderingOptions_nid_postfix(Icu::BiDi*, std::uint32_t);
void APS5_VABI ubidi_setContext_nid_postfix(Icu::BiDi*, const char16_t*, std::int32_t, const char16_t*, std::int32_t,
                                            std::int32_t*);
void APS5_VABI ubidi_setLine_nid_postfix(const Icu::BiDi*, std::int32_t, std::int32_t, Icu::BiDi*, std::int32_t*);
std::int32_t APS5_VABI ubidi_getParagraph_nid_postfix(const Icu::BiDi*, std::int32_t, std::int32_t*, std::int32_t*,
                                                      std::uint8_t*, std::int32_t*);
void APS5_VABI ubidi_getParagraphByIndex_nid_postfix(const Icu::BiDi*, std::int32_t, std::int32_t*, std::int32_t*,
                                                     std::uint8_t*, std::int32_t*);
void APS5_VABI ubidi_getClassCallback_nid_postfix(Icu::BiDi*, Icu::BiDiClassCallback*, const void**);
const char16_t* APS5_VABI ubidi_getText_nid_postfix(const Icu::BiDi*);
const std::uint8_t* APS5_VABI ubidi_getLevels_nid_postfix(Icu::BiDi*, std::int32_t*);
std::int32_t APS5_VABI ubidi_getLogicalIndex_nid_postfix(Icu::BiDi*, std::int32_t, std::int32_t*);
void APS5_VABI ubidi_getLogicalMap_nid_postfix(Icu::BiDi*, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI ubidi_getVisualIndex_nid_postfix(Icu::BiDi*, std::int32_t, std::int32_t*);
void APS5_VABI ubidi_getVisualMap_nid_postfix(Icu::BiDi*, std::int32_t*, std::int32_t*);
void APS5_VABI ubidi_invertMap_nid_postfix(const std::int32_t*, std::int32_t*, std::int32_t);
void APS5_VABI ubidi_reorderLogical_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ubidi_writeReordered_nid_postfix(Icu::BiDi*, char16_t*, std::int32_t, std::uint16_t,
                                                        std::int32_t*);
}
