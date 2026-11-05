// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTFREETYPE_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTFREETYPE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Font {

constexpr float ONE_OVER_64 = 1.0f / 64.0f;

inline std::int32_t TruncateFloatToInt(float value) {
    if (!std::isfinite(value) || value > 2147483647.0f || value < -2147483648.0f) return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}

inline std::int32_t RoundMulFixed(std::int64_t value, std::int64_t fixed16) {
    const std::int64_t product = value * fixed16;
    const std::int64_t signAdjust = ((~value) >> 63) * -0x10000LL;
    const std::int64_t base = signAdjust + product;
    std::int64_t rounded = base - 0x8000LL;
    if (rounded < 0) rounded = base + 0x7FFFLL;
    return static_cast<std::int32_t>(static_cast<std::uint64_t>(rounded) >> 16);
}

inline std::int32_t TruncFixed(std::int64_t fixed16) {
    std::int64_t value = fixed16;
    if (fixed16 < 0) value = fixed16 + 0xFFFFLL;
    return static_cast<std::int32_t>(value >> 16);
}

inline std::int64_t TruncMulUnits(std::int64_t fixed16, std::uint16_t units) {
    const std::int64_t product = fixed16 * static_cast<std::int64_t>(units);
    std::int64_t rounded = product;
    if (product < 0) rounded = product + 0xFFFFLL;
    return static_cast<std::int64_t>(static_cast<std::int32_t>(rounded >> 16));
}

inline float FixedMulUnitsToF26Dot6(std::int64_t fixed16, std::uint16_t unitsPerEm) {
    const std::int64_t product = fixed16 * static_cast<std::int64_t>(unitsPerEm);
    std::int64_t rounded = product + 0xFFFF;
    if (product >= 0) rounded = product;
    return static_cast<float>(rounded >> 16) * ONE_OVER_64;
}

inline int FloorToInt(float value) {
    int result = static_cast<int>(std::trunc(value));
    if (static_cast<float>(result) > value) --result;
    return result;
}

inline int CeilToInt(float value) {
    int result = static_cast<int>(std::trunc(value));
    if (static_cast<float>(result) < value) ++result;
    return result;
}

inline FT_UInt ResolveGlyphIndexWithFallback(FT_Face face, std::uint32_t codepoint) {
    if (!face) return 0;
    const auto code = static_cast<FT_ULong>(codepoint);
    const FT_CharMap original = face->charmap;
    FT_UInt glyphIndex = FT_Get_Char_Index(face, code);
    if (glyphIndex != 0 || !face->charmaps || face->num_charmaps <= 1) return glyphIndex;
    for (int i = 0; i < face->num_charmaps; ++i) {
        const FT_CharMap charmap = face->charmaps[i];
        if (!charmap || charmap == original) continue;
        if (FT_Set_Charmap(face, charmap) != 0) continue;
        glyphIndex = FT_Get_Char_Index(face, code);
        if (glyphIndex != 0) break;
    }
    if (original && face->charmap != original) FT_Set_Charmap(face, original);
    return glyphIndex;
}

inline FT_F26Dot6 ClampPpem(FT_F26Dot6 value) {
    constexpr FT_F26Dot6 maxPpem = static_cast<FT_F26Dot6>(65535 * 64);
    return std::clamp(value, static_cast<FT_F26Dot6>(-maxPpem), maxPpem);
}

inline FT_Error SetCharSizeCompat(FT_Face face, FT_F26Dot6 charW, FT_F26Dot6 charH, std::uint32_t dpiX, std::uint32_t dpiY, FT_F26Dot6* usedW = nullptr, FT_F26Dot6* usedH = nullptr) {
    const auto writeUsed = [&](FT_F26Dot6 w, FT_F26Dot6 h) {
        if (usedW) *usedW = w;
        if (usedH) *usedH = h;
    };
    if (!face) {
        writeUsed(charW, charH);
        return FT_Err_Invalid_Face_Handle;
    }
    FT_Error error = FT_Set_Char_Size(face, charW, charH, dpiX, dpiY);
    if (error == 0) {
        writeUsed(charW, charH);
        return 0;
    }
    const FT_F26Dot6 clampedW = ClampPpem(charW);
    const FT_F26Dot6 clampedH = ClampPpem(charH);
    if (clampedW != charW || clampedH != charH) {
        error = FT_Set_Char_Size(face, clampedW, clampedH, dpiX, dpiY);
        if (error == 0) {
            writeUsed(clampedW, clampedH);
            return 0;
        }
    }
    if (face->num_fixed_sizes <= 0 || !face->available_sizes) {
        writeUsed(charW, charH);
        return error;
    }
    const double requestW = static_cast<double>(std::llabs(static_cast<long long>(charW))) / 64.0;
    const double requestH = static_cast<double>(std::llabs(static_cast<long long>(charH))) / 64.0;
    const double targetW = requestW > 0.0 ? requestW : requestH;
    const double targetH = requestH > 0.0 ? requestH : requestW;
    if (targetW <= 0.0 || targetH <= 0.0) {
        writeUsed(charW, charH);
        return error;
    }
    int bestIndex = -1;
    double bestScore = std::numeric_limits<double>::infinity();
    for (int i = 0; i < face->num_fixed_sizes; ++i) {
        const FT_Bitmap_Size& strike = face->available_sizes[i];
        const double strikeW = std::max(1.0, static_cast<double>(strike.x_ppem) / 64.0);
        const double strikeH = std::max(1.0, static_cast<double>(strike.y_ppem) / 64.0);
        const double score = std::abs(strikeW - targetW) / targetW + std::abs(strikeH - targetH) / targetH;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    if (bestIndex < 0) {
        writeUsed(charW, charH);
        return error;
    }
    error = FT_Select_Size(face, bestIndex);
    if (error == 0) {
        const FT_Bitmap_Size& strike = face->available_sizes[bestIndex];
        writeUsed(strike.x_ppem, strike.y_ppem);
        return 0;
    }
    writeUsed(charW, charH);
    return error;
}

}

#endif
