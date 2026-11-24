// Derived from icu4c/source/common/normalizer2impl.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/normalizer2impl.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2009-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "Normalization.hpp"
#include <stdexcept>

namespace {

constexpr std::uint16_t Comp1LastTuple = 0x8000;
constexpr std::uint16_t Comp1Triple = 1;
constexpr std::int32_t Comp1TrailLimit = 0x3400;
constexpr std::uint16_t Comp1TrailMask = 0x7ffe;
constexpr std::uint32_t Comp1TrailShift = 9;
constexpr std::uint32_t Comp2TrailShift = 6;
constexpr std::uint16_t Comp2TrailMask = 0xffc0;

} // namespace

extern "C" {

std::int32_t APS5_VABI _ZN6icu_6815Normalizer2Impl7combineEPKti_nid_postfix(const std::uint16_t* list, std::int32_t trail) {
    if (!list) throw std::invalid_argument("Normalizer2Impl::combine: null list");
    const auto bits = static_cast<std::uint32_t>(trail);
    std::uint16_t firstUnit;
    if (trail < Comp1TrailLimit) {
        const auto key1 = static_cast<std::uint16_t>(bits << 1);
        while (key1 > (firstUnit = *list)) {
            list += 2 + (firstUnit & Comp1Triple);
        }
        if (key1 == (firstUnit & Comp1TrailMask)) {
            if (firstUnit & Comp1Triple) {
                return static_cast<std::int32_t>((static_cast<std::uint32_t>(list[1]) << 16) | list[2]);
            }
            return list[1];
        }
        return -1;
    }
    const auto key1 = static_cast<std::uint16_t>(Comp1TrailLimit + ((trail >> Comp1TrailShift) & ~std::int32_t{Comp1Triple}));
    const auto key2 = static_cast<std::uint16_t>(bits << Comp2TrailShift);
    for (;;) {
        if (key1 > (firstUnit = *list)) {
            list += 2 + (firstUnit & Comp1Triple);
        } else if (key1 == (firstUnit & Comp1TrailMask)) {
            const std::uint16_t secondUnit = list[1];
            if (key2 > secondUnit) {
                if (firstUnit & Comp1LastTuple) {
                    return -1;
                }
                list += 3;
            } else if (key2 == (secondUnit & Comp2TrailMask)) {
                return static_cast<std::int32_t>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(secondUnit & ~Comp2TrailMask)) << 16) | list[2]);
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }
}

}
