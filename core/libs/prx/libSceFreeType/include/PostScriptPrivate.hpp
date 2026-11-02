#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_POSTSCRIPTPRIVATE_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_POSTSCRIPTPRIVATE_HPP

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

namespace FreeType {

struct GuestPSPrivate {
    std::int32_t uniqueId;
    std::int32_t lenIV;
    std::uint8_t numBlueValues;
    std::uint8_t numOtherBlues;
    std::uint8_t numFamilyBlues;
    std::uint8_t numFamilyOtherBlues;
    std::int16_t blueValues[14];
    std::int16_t otherBlues[10];
    std::int16_t familyBlues[14];
    std::int16_t familyOtherBlues[10];
    std::int64_t blueScale;
    std::int32_t blueShift;
    std::int32_t blueFuzz;
    std::uint16_t standardWidth[1];
    std::uint16_t standardHeight[1];
    std::uint8_t numSnapWidths;
    std::uint8_t numSnapHeights;
    std::uint8_t forceBold;
    std::uint8_t roundStemUp;
    std::int16_t snapWidths[13];
    std::int16_t snapHeights[13];
    std::int64_t expansionFactor;
    std::int64_t languageGroup;
    std::int64_t password;
    std::int16_t minFeature[2];
};

static_assert(sizeof(GuestPSPrivate) == 224 && alignof(GuestPSPrivate) == 8);
static_assert(offsetof(GuestPSPrivate, uniqueId) == 0 && offsetof(GuestPSPrivate, lenIV) == 4);
static_assert(offsetof(GuestPSPrivate, numBlueValues) == 8 && offsetof(GuestPSPrivate, numOtherBlues) == 9);
static_assert(offsetof(GuestPSPrivate, numFamilyBlues) == 10 && offsetof(GuestPSPrivate, numFamilyOtherBlues) == 11);
static_assert(offsetof(GuestPSPrivate, blueValues) == 12 && offsetof(GuestPSPrivate, otherBlues) == 40);
static_assert(offsetof(GuestPSPrivate, familyBlues) == 60 && offsetof(GuestPSPrivate, familyOtherBlues) == 88);
static_assert(offsetof(GuestPSPrivate, blueScale) == 112 && offsetof(GuestPSPrivate, blueShift) == 120);
static_assert(offsetof(GuestPSPrivate, blueFuzz) == 124 && offsetof(GuestPSPrivate, standardWidth) == 128);
static_assert(offsetof(GuestPSPrivate, standardHeight) == 130 && offsetof(GuestPSPrivate, numSnapWidths) == 132);
static_assert(offsetof(GuestPSPrivate, numSnapHeights) == 133 && offsetof(GuestPSPrivate, forceBold) == 134);
static_assert(offsetof(GuestPSPrivate, roundStemUp) == 135 && offsetof(GuestPSPrivate, snapWidths) == 136);
static_assert(offsetof(GuestPSPrivate, snapHeights) == 162 && offsetof(GuestPSPrivate, expansionFactor) == 192);
static_assert(offsetof(GuestPSPrivate, languageGroup) == 200 && offsetof(GuestPSPrivate, password) == 208);
static_assert(offsetof(GuestPSPrivate, minFeature) == 216);

}

#endif
