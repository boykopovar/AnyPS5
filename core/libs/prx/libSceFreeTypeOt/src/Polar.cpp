// Derived from FreeType src/base/fttrigon.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 2001-2005, 2012 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Polar.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>

using namespace FreeType;

namespace {

constexpr std::int64_t AnglePi = 180ll << 16;
constexpr std::int64_t AnglePi2 = 90ll << 16;
constexpr std::uint64_t TrigScale = 0x9B74EDA8;
constexpr int NormalBits = 27;
constexpr std::array<std::int64_t, 23> ArctanTable{
    2949120, 1740967, 919879, 466945, 234379, 117304, 58666, 29335, 14668, 7334, 3667, 1833,
    917, 458, 229, 115, 57, 29, 14, 7, 4, 2, 1};

std::uint64_t Magnitude(std::int64_t value) {
    return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

std::int64_t ShiftLeft(std::int64_t value, int shift) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) << shift);
}

struct Normalized {
    GuestVector vector;
    int shift;
};

Normalized Prenorm(const GuestVector& vector, const char* function) {
    const std::uint64_t z = Magnitude(vector.x) | Magnitude(vector.y);
    if (z > 0xFFFFFFFF) NotImplemented_nid_no_patch(function);
    const int msb = z == 0 ? 0 : static_cast<int>(std::bit_width(z)) - 1;
    if (msb <= NormalBits) {
        const int shift = NormalBits - msb;
        return {{ShiftLeft(vector.x, shift), ShiftLeft(vector.y, shift)}, shift};
    }
    const int shift = msb - NormalBits;
    return {{vector.x >> shift, vector.y >> shift}, -shift};
}

GuestVector PseudoRotate(GuestVector vector, std::int64_t angle) {
    __int128 theta = angle;
    __int128 halfTurns = 0;
    if (theta <= -AnglePi2) {
        halfTurns = (-AnglePi2 - theta) / AnglePi + 1;
        theta += halfTurns * AnglePi;
    } else if (theta > AnglePi2) {
        halfTurns = (theta - AnglePi2 - 1) / AnglePi + 1;
        theta -= halfTurns * AnglePi;
    }
    std::int64_t x = vector.x;
    std::int64_t y = vector.y;
    if ((halfTurns & 1) != 0) {
        x = -x;
        y = -y;
    }
    auto remaining = static_cast<std::int64_t>(theta);
    for (std::size_t i = 0; i < ArctanTable.size(); ++i) {
        const std::int64_t xShifted = x >> i;
        const std::int64_t yShifted = y >> i;
        if (remaining < 0) {
            x += yShifted;
            y -= xShifted;
            remaining += ArctanTable[i];
        } else {
            x -= yShifted;
            y += xShifted;
            remaining -= ArctanTable[i];
        }
    }
    return {x, y};
}

GuestVector PseudoPolarize(GuestVector vector) {
    std::int64_t x = vector.x;
    std::int64_t y = vector.y;
    std::int64_t theta = 0;
    if (x < 0) {
        x = -x;
        y = -y;
        theta = AnglePi;
    }
    if (y > 0) theta = -theta;
    for (std::size_t i = 0; i < ArctanTable.size(); ++i) {
        const std::int64_t xShifted = x >> i;
        const std::int64_t yShifted = y >> i;
        if (y > 0) {
            x += yShifted;
            y -= xShifted;
            theta += ArctanTable[i];
        } else {
            x -= yShifted;
            y += xShifted;
            theta -= ArctanTable[i];
        }
    }
    theta = theta >= 0 ? (theta + 16) & ~std::int64_t{31} : -((-theta + 16) & ~std::int64_t{31});
    return {x, theta};
}

std::uint32_t DownscaleNarrow(std::uint32_t value) {
    const std::uint32_t v1 = value >> 16;
    const std::uint32_t v2 = value & 0xFFFF;
    constexpr auto k1 = static_cast<std::uint32_t>(TrigScale >> 16);
    constexpr auto k2 = static_cast<std::uint32_t>(TrigScale & 0xFFFF);
    std::uint32_t hi = k1 * v1;
    std::uint32_t lo1 = k1 * v2 + k2 * v1;
    const std::uint32_t lo2 = (k2 * v2) >> 16;
    const std::uint32_t lo3 = lo1 >= lo2 ? lo1 : lo2;
    lo1 += lo2;
    hi += lo1 >> 16;
    if (lo1 < lo3) hi += 0x10000;
    return hi;
}

std::int64_t Downscale(std::int64_t value, bool long64) {
    const std::uint64_t magnitude = Magnitude(value);
    const auto scaled = static_cast<std::int64_t>(
        long64 ? (magnitude * TrigScale + 0x100000000ull) >> 32 : DownscaleNarrow(static_cast<std::uint32_t>(magnitude)));
    return value >= 0 ? scaled : -scaled;
}

std::int64_t Trivial(std::int64_t value, const char* function) {
    if (value == std::numeric_limits<std::int64_t>::min()) NotImplemented_nid_no_patch(function);
    return value >= 0 ? value : -value;
}

GuestVector Rotated(const GuestVector& vector, std::int64_t angle, const char* function) {
    if (angle == 0 || (vector.x == 0 && vector.y == 0)) return vector;
    const Normalized normalized = Prenorm(vector, function);
    const GuestVector rotated = PseudoRotate(normalized.vector, angle);
    const int shift = normalized.shift;
    const auto finish = [&](bool long64) {
        const GuestVector scaled{Downscale(rotated.x, long64), Downscale(rotated.y, long64)};
        if (shift > 0) {
            const std::int64_t half = std::int64_t{1} << (shift - 1);
            return GuestVector{(scaled.x + half + (scaled.x >> 63)) >> shift, (scaled.y + half + (scaled.y >> 63)) >> shift};
        }
        return GuestVector{ShiftLeft(scaled.x, -shift), ShiftLeft(scaled.y, -shift)};
    };
    const GuestVector result = finish(false);
    const GuestVector wide = finish(true);
    if (result.x != wide.x || result.y != wide.y) NotImplemented_nid_no_patch(function);
    return result;
}

}

namespace FreeType {

std::int64_t OtVectorLength(const GuestVector& vector, const char* function) {
    if (vector.x == 0) return Trivial(vector.y, function);
    if (vector.y == 0) return Trivial(vector.x, function);
    const Normalized normalized = Prenorm(vector, function);
    const GuestVector polar = PseudoPolarize(normalized.vector);
    const int shift = normalized.shift;
    const auto finish = [&](bool long64) {
        const std::int64_t scaled = Downscale(polar.x, long64);
        return shift > 0 ? (scaled + (std::int64_t{1} << (shift - 1))) >> shift : ShiftLeft(scaled, -shift);
    };
    const std::int64_t length = finish(false);
    if (length != finish(true)) NotImplemented_nid_no_patch(function);
    return length;
}

}

extern "C" {

std::int64_t APS5_VABI sceFreeTypeOtVectorLength_nid_no_patch(GuestVector* vector) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    return OtVectorLength(*vector, __func__);
}
APS5_EXPORT("1jmC2QcpRl0", sceFreeTypeOtVectorLength_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorPolarize_nid_no_patch(GuestVector* vector, std::int64_t* length, std::int64_t* angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    if (vector->x == 0 && vector->y == 0) return;
    if (!length || !angle) NotImplemented_nid_no_patch(__func__);
    const Normalized normalized = Prenorm(*vector, __func__);
    const GuestVector polar = PseudoPolarize(normalized.vector);
    const int shift = normalized.shift;
    const auto finish = [&](bool long64) {
        const std::int64_t scaled = Downscale(polar.x, long64);
        return shift >= 0 ? scaled >> shift : ShiftLeft(scaled, -shift);
    };
    const std::int64_t result = finish(false);
    if (result != finish(true)) NotImplemented_nid_no_patch(__func__);
    *length = result;
    *angle = polar.y;
}
APS5_EXPORT("HMcNe70D9TI", sceFreeTypeOtVectorPolarize_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorFromPolar_nid_no_patch(GuestVector* vector, std::int64_t length, std::int64_t angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    *vector = Rotated({length, 0}, angle, __func__);
}
APS5_EXPORT("2rmvbvOCyjY", sceFreeTypeOtVectorFromPolar_nid_no_patch);

void APS5_VABI sceFreeTypeOtVectorRotate_nid_no_patch(GuestVector* vector, std::int64_t angle) {
    if (!vector) NotImplemented_nid_no_patch(__func__);
    *vector = Rotated(*vector, angle, __func__);
}
APS5_EXPORT("6wJwUIVwAxg", sceFreeTypeOtVectorRotate_nid_no_patch);

}
