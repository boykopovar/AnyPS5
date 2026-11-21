// Derived from icu4c/source/common/putil.cpp, ustrfmt.cpp, uarrsort.cpp and propname.cpp at
// 84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the clock, IEEE 754, path, integer formatting,
// array sort and property-name comparison helpers below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustrfmt.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/propname.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/Platform.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>

namespace {

constexpr std::int32_t MinimumQuickSort = 9;

char LowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 0x20) : value;
}

std::int32_t NextPropertyNameChar(const char* name) {
    std::int32_t index = 0;
    char current = 0;
    do {
        current = name[index++];
    } while (current == '-' || current == '_' || current == ' ' || (current >= 0x09 && current <= 0x0d));
    if (current != 0) return (index << 8) | static_cast<std::uint8_t>(LowerAscii(current));
    return index << 8;
}

void CopyItems(void* destination, const void* source, std::int32_t count, std::int32_t itemSize) {
    if (count == 1) {
        std::memcpy(destination, source, static_cast<std::size_t>(itemSize));
        return;
    }
    std::memmove(destination, source, static_cast<std::size_t>(count) * static_cast<std::size_t>(itemSize));
}

}

extern "C" {

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value,
    std::uint32_t radix, std::int32_t minwidth) {
    if (buffer == nullptr || capacity <= 0 || radix < 2 || radix > 36) return 0;
    std::int32_t length = 0;
    do {
        const std::int32_t digit = static_cast<std::int32_t>(value % radix);
        buffer[length++] = static_cast<char16_t>(digit <= 9 ? 0x0030 + digit : 0x0030 + digit + 7);
        value /= radix;
    } while (value != 0 && length < capacity);
    while (length < minwidth && length < capacity) {
        buffer[length++] = 0x0030;
    }
    if (length < capacity) buffer[length] = 0;
    for (std::int32_t index = 0; index < length / 2; ++index) {
        const char16_t temporary = buffer[length - 1 - index];
        buffer[length - 1 - index] = buffer[index];
        buffer[index] = temporary;
    }
    return length;
}






std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    if (path == nullptr || *path == 0) return 0;
    return static_cast<std::int8_t>(*path == '/');
}

std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* first, const char* second) {
    for (;;) {
        const std::int32_t left = NextPropertyNameChar(first);
        const std::int32_t right = NextPropertyNameChar(second);
        if (((left | right) & 0xff) == 0) return 0;
        if (left != right) {
            const std::int32_t difference = (left & 0xff) - (right & 0xff);
            if (difference != 0) return difference;
        }
        first += left >> 8;
        second += right >> 8;
    }
}

std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item,
    std::int32_t itemSize, UComparator comparator, const void* context) {
    std::int32_t start = 0;
    bool found = false;
    while ((limit - start) >= MinimumQuickSort) {
        const std::int32_t index = (start + limit) / 2;
        const std::int32_t difference = comparator(context, item, array + index * itemSize);
        if (difference == 0) {
            found = true;
            start = index + 1;
        } else if (difference < 0) {
            limit = index;
        } else {
            start = index + 1;
        }
    }
    while (start < limit) {
        const std::int32_t difference = comparator(context, item, array + start * itemSize);
        if (difference == 0) {
            found = true;
        } else if (difference < 0) {
            break;
        }
        ++start;
    }
    return found ? start - 1 : ~start;
}

void APS5_VABI uprv_sortArray_nid_postfix(void* array, std::int32_t length, std::int32_t itemSize,
    UComparator comparator, const void* context, std::int8_t sortStable, std::int32_t* status) {
    (void)sortStable;
    if (status == nullptr || *status > 0) return;
    if ((length > 0 && array == nullptr) || length < 0 || itemSize <= 0 || comparator == nullptr) {
        *status = Icu::IllegalArgument;
        return;
    }
    if (length <= 1) return;
    auto* bytes = static_cast<char*>(array);
    char* scratch = nullptr;
    char inlineScratch[64];
    if (itemSize <= static_cast<std::int32_t>(sizeof(inlineScratch))) {
        scratch = inlineScratch;
    } else {
        scratch = static_cast<char*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(itemSize)));
        if (scratch == nullptr) {
            *status = 7;
            return;
        }
    }
    for (std::int32_t index = 1; index < length; ++index) {
        char* item = bytes + index * itemSize;
        std::int32_t insertionPoint = uprv_stableBinarySearch_nid_postfix(bytes, index, item, itemSize, comparator,
            context);
        if (insertionPoint < 0) {
            insertionPoint = ~insertionPoint;
        } else {
            ++insertionPoint;
        }
        if (insertionPoint < index) {
            char* destination = bytes + insertionPoint * itemSize;
            std::memcpy(scratch, item, static_cast<std::size_t>(itemSize));
            CopyItems(destination + itemSize, destination, index - insertionPoint, itemSize);
            std::memcpy(destination, scratch, static_cast<std::size_t>(itemSize));
        }
    }
    if (scratch != inlineScratch) IcuReleaseMemory_nid_no_patch(scratch);
}

}
