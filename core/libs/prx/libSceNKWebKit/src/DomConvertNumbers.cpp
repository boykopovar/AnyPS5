/* Derived from Source/WebCore/bindings/js/JSDOMConvertNumbers.cpp@e12a212a531aeddc11259e8515671cf5188e038d */
/* Derived from Source/WTF/wtf/MathExtras.h@e12a212a531aeddc11259e8515671cf5188e038d */
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "prx/libSceNKWebKit/include/DomConvertNumbersExports.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

constexpr std::uint64_t NumberTag = 0xfffe000000000000ull;
constexpr std::uint64_t DoubleEncodeOffset = 1ull << 49;
constexpr std::uint64_t ValueNull = 0x02;
constexpr std::uint64_t ValueFalse = 0x06;
constexpr std::uint64_t ValueTrue = 0x07;
constexpr std::uint64_t ValueUndefined = 0x0a;
constexpr double JSMaxInteger = 9007199254740991.0;

enum class Mode { Normal, Clamp };

bool IsInt32(std::uint64_t value) {
    return (value & NumberTag) == NumberTag;
}

double ToNumber(std::uint64_t value, const char* name) {
    if (IsInt32(value)) return static_cast<std::int32_t>(value);
    if (value & NumberTag) {
        const std::uint64_t bits = value - DoubleEncodeOffset;
        double number;
        std::memcpy(&number, &bits, sizeof(number));
        return number;
    }
    switch (value) {
    case ValueNull:
    case ValueFalse:
        return 0;
    case ValueTrue:
        return 1;
    case ValueUndefined:
        return std::numeric_limits<double>::quiet_NaN();
    }
    NotImplemented_nid_no_patch(name);
}

std::uint64_t Modulo2To64(double x) {
    if (std::isnan(x) || std::isinf(x)) return 0;
    const double fmodValue = std::fmod(std::trunc(x), 18446744073709551616.0);
    if (fmodValue >= 0) return static_cast<std::uint64_t>(fmodValue);
    return std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(-fmodValue) + 1;
}

template<typename T>
T ClampTo(double x) {
    if (x >= static_cast<double>(std::numeric_limits<T>::max())) return std::numeric_limits<T>::max();
    if (!(x > static_cast<double>(std::numeric_limits<T>::min()))) return std::numeric_limits<T>::min();
    return static_cast<T>(x);
}

template<typename T, Mode mode>
T Convert(std::uint64_t value, const char* name) {
    constexpr bool isSigned = std::is_signed_v<T>;
    if (IsInt32(value) && (isSigned || static_cast<std::int32_t>(value) >= 0)) {
        const std::int32_t d = static_cast<std::int32_t>(value);
        if constexpr (sizeof(T) < 4 && mode == Mode::Clamp) {
            if (d < std::numeric_limits<T>::min()) return std::numeric_limits<T>::min();
            if (d > std::numeric_limits<T>::max()) return std::numeric_limits<T>::max();
        }
        return static_cast<T>(d);
    }
    const double x = ToNumber(value, name);
    if (std::isnan(x)) return 0;
    if constexpr (sizeof(T) == 8) {
        if constexpr (mode == Mode::Clamp) return static_cast<T>(std::min(std::max(x, isSigned ? -JSMaxInteger : 0.0), JSMaxInteger));
        return static_cast<T>(Modulo2To64(x));
    } else {
        if constexpr (mode == Mode::Clamp) return ClampTo<T>(x);
        return static_cast<T>(static_cast<std::uint32_t>(Modulo2To64(x)));
    }
}

}

#define APS5_CONVERT(mangled, type, mode) \
    extern "C" type APS5_VABI mangled(void*, std::uint64_t value) { \
        return Convert<type, Mode::mode>(value, __func__); \
    }

APS5_CONVERT(_ZN7WebCore16convertToIntegerIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int8_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint8_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int32_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint32_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int64_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint64_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int16_t, Normal)
APS5_CONVERT(_ZN7WebCore16convertToIntegerItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint16_t, Normal)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int8_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint8_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int32_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint32_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int64_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint64_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::int16_t, Clamp)
APS5_CONVERT(_ZN7WebCore21convertToIntegerClampItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE, std::uint16_t, Clamp)
