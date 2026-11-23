#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct Normalizer2;
} // namespace Icu

extern "C" {
std::int32_t APS5_VABI _ZNK6icu_6819FilteredNormalizer211composePairEii_nid_postfix(const Icu::Normalizer2*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6819FilteredNormalizer27isInertEi_nid_postfix(const Icu::Normalizer2*, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6819FilteredNormalizer217hasBoundaryBeforeEi_nid_postfix(const Icu::Normalizer2*, std::int32_t);
}
