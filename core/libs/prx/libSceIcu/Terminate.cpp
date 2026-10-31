// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

template<typename TChar>
std::int32_t Terminate(TChar* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    if (!status || *status > 0 || length < 0) return length;
    if (length < capacity) {
        if (!destination) throw std::invalid_argument("ICU termination: null destination");
        destination[length] = 0;
        if (*status == -124) *status = 0;
    } else *status = length == capacity ? -124 : 15;
    return length;
}

}

extern "C" {

std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status);
}

std::int32_t APS5_VABI u_terminateWChars_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status);
}

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t value) {
    return value >= u'a' && value <= u'z' ? value - (u'a' - u'A') : value;
}

char16_t* APS5_VABI u_uastrcpy_nid_postfix(char16_t* destination, const char* source) {
    if (!destination || !source) throw std::invalid_argument("ICU ASCII copy: null buffer");
    const auto sourceLength = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(std::strlen(source)));
    if (sourceLength <= 0) {
        destination[0] = 0;
        return destination;
    }
    const auto length = static_cast<std::size_t>(sourceLength);
    for (std::size_t i = 0; i < length; ++i)
        if (static_cast<std::uint8_t>(source[i]) > 127) NotImplemented_nid_no_patch(__func__);
    constexpr std::size_t capacity = 0x0fffffff;
    for (std::size_t i = 0; i < std::min(length, capacity); ++i) destination[i] = static_cast<std::uint8_t>(source[i]);
    if (length < capacity) destination[length] = 0;
    else if (length > capacity) destination[0] = 0;
    return destination;
}

char16_t* APS5_VABI u_uastrncpy_nid_postfix(char16_t* destination, const char* source, std::int32_t count) {
    if (!destination || !source) throw std::invalid_argument("ICU ASCII copy: null buffer");
    if (count < 0) {
        auto remaining = count;
        const char* cursor = source;
        for (;;) {
            if (remaining == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU ASCII copy: length underflow");
            --remaining;
            if (!*cursor++) break;
        }
        destination[0] = 0;
        return destination;
    }
    std::int32_t length = 0;
    while (length < count && source[length]) {
        if (static_cast<std::uint8_t>(source[length]) > 127) NotImplemented_nid_no_patch(__func__);
        ++length;
    }
    for (std::int32_t i = 0; i < length; ++i) destination[i] = static_cast<std::uint8_t>(source[i]);
    if (length < count) destination[length] = 0;
    return destination;
}

}
