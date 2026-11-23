#include "prx/libSceIcu/FilteredNormalizerQueries.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/normalizer2.h"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

const icu::FilteredNormalizer2* Filtered(const Icu::Normalizer2* normalizer, const char* caller) {
    if (!normalizer) throw std::invalid_argument(std::string(caller) + ": null normalizer");
    const auto* filtered = dynamic_cast<const icu::FilteredNormalizer2*>(reinterpret_cast<const icu::Normalizer2*>(normalizer));
    if (!filtered) NotImplemented_nid_no_patch("FilteredNormalizer2: normalizer that is not filtered");
    return filtered;
}

}

extern "C" {

std::int32_t APS5_VABI _ZNK6icu_6819FilteredNormalizer211composePairEii_nid_postfix(const Icu::Normalizer2* normalizer,
                                                                                    std::int32_t first, std::int32_t second) {
    return Filtered(normalizer, __func__)->icu::FilteredNormalizer2::composePair(first, second);
}

std::int8_t APS5_VABI _ZNK6icu_6819FilteredNormalizer27isInertEi_nid_postfix(const Icu::Normalizer2* normalizer,
                                                                            std::int32_t character) {
    return static_cast<std::int8_t>(Filtered(normalizer, __func__)->icu::FilteredNormalizer2::isInert(character));
}

std::int8_t APS5_VABI _ZNK6icu_6819FilteredNormalizer217hasBoundaryBeforeEi_nid_postfix(const Icu::Normalizer2* normalizer,
                                                                                      std::int32_t character) {
    return static_cast<std::int8_t>(Filtered(normalizer, __func__)->icu::FilteredNormalizer2::hasBoundaryBefore(character));
}

}
