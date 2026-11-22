#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct BiDi;
using BiDiClassCallback = std::int32_t (APS5_VABI*)(const void*, std::int32_t);
} // namespace Icu

extern "C" {
Icu::BiDi* APS5_VABI ubidi_open_nid_postfix();
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
}
