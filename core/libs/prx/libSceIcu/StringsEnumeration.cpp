// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

struct StringEnumeration {
    Icu::UEnumeration enumeration;
    std::int32_t index;
    std::int32_t count;
};

struct EnumBuffer {
    std::int32_t length;
    char data;
};

static_assert(sizeof(StringEnumeration) == 64 && alignof(StringEnumeration) == 8);
static_assert(offsetof(StringEnumeration, enumeration) == 0);
static_assert(offsetof(StringEnumeration, index) == 56 && offsetof(StringEnumeration, count) == 60);
static_assert(sizeof(EnumBuffer) == 8 && alignof(EnumBuffer) == 4);
static_assert(offsetof(EnumBuffer, length) == 0 && offsetof(EnumBuffer, data) == 4);

template<typename TValue>
TValue Require(TValue value) {
    if (!value) throw std::invalid_argument("ICU enumeration: null pointer");
    return value;
}

void* Allocate(void* previous, std::size_t bytes) {
    return std::realloc(previous, bytes);
}

std::int32_t Capacity(std::int32_t length, std::int32_t width) {
    const auto bytes = (static_cast<std::int64_t>(length) + 1) * width;
    if (length == std::numeric_limits<std::int32_t>::max() || bytes > std::numeric_limits<std::int32_t>::max() ||
        bytes < std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU enumeration: capacity overflow");
    return static_cast<std::int32_t>(bytes);
}

void* Buffer(Icu::UEnumeration* enumeration, std::int32_t capacity) {
    auto* buffer = static_cast<EnumBuffer*>(enumeration->baseContext);
    if (!buffer || buffer->length < capacity) {
        if (capacity > std::numeric_limits<std::int32_t>::max() - 8)
            throw std::invalid_argument("ICU enumeration: padded capacity overflow");
        capacity += 8;
        enumeration->baseContext = Allocate(buffer, sizeof(std::int32_t) + static_cast<std::size_t>(capacity));
        buffer = static_cast<EnumBuffer*>(enumeration->baseContext);
        if (!buffer) return nullptr;
        buffer->length = capacity;
    }
    return &buffer->data;
}

void APS5_VABI Close_nid_no_patch(Icu::UEnumeration* enumeration) {
    std::free(enumeration);
}

std::int32_t APS5_VABI Count_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t*) {
    return reinterpret_cast<StringEnumeration*>(Require(enumeration))->count;
}

template<typename TChar>
const TChar* Next(Icu::UEnumeration* enumeration, std::int32_t* resultLength) {
    auto* strings = reinterpret_cast<StringEnumeration*>(Require(enumeration));
    if (strings->index >= strings->count) return nullptr;
    if (strings->index < 0 || strings->index == std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU enumeration: invalid index");
    const auto* result = static_cast<const TChar* const*>(Require(enumeration->context))[strings->index++];
    if (resultLength) {
        Require(result);
        std::int32_t length = 0;
        while (result[length]) {
            if (length == std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("ICU enumeration: string length overflow");
            ++length;
        }
        *resultLength = length;
    }
    return result;
}

const char* APS5_VABI NextChars_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t* length, std::int32_t*) {
    return Next<char>(enumeration, length);
}

const char16_t* APS5_VABI NextUChars_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t* length, std::int32_t*) {
    return Next<char16_t>(enumeration, length);
}

void APS5_VABI Reset_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t*) {
    reinterpret_cast<StringEnumeration*>(Require(enumeration))->index = 0;
}

Icu::UEnumeration* Open(const void* strings, std::int32_t count, bool unicode, std::int32_t* status) {
    if (*Require(status) > 0 || count < 0 || (count && !strings)) return nullptr;
    auto* result = static_cast<StringEnumeration*>(Allocate(nullptr, sizeof(StringEnumeration)));
    if (!result) {
        *status = 7;
        return nullptr;
    }
    *result = {{nullptr, const_cast<void*>(strings), Close_nid_no_patch, Count_nid_no_patch,
                unicode ? NextUChars_nid_no_patch : uenum_unextDefault_nid_postfix,
                unicode ? uenum_nextDefault_nid_postfix : NextChars_nid_no_patch, Reset_nid_no_patch}, 0, count};
    return &result->enumeration;
}

}

extern "C" {

const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    Require(enumeration);
    char16_t* result = nullptr;
    std::int32_t length = 0;
    if (enumeration->next) {
        const char* text = enumeration->next(enumeration, &length, status);
        if (text) {
            result = static_cast<char16_t*>(Buffer(enumeration, Capacity(length, 2)));
            if (result) u_charsToUChars_nid_postfix(text, result, length + 1);
            else *Require(status) = 7;
        }
    } else *Require(status) = 16;
    if (resultLength) *resultLength = length;
    return result;
}

const char* APS5_VABI uenum_nextDefault_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    Require(enumeration);
    if (!enumeration->uNext) {
        *Require(status) = 16;
        return nullptr;
    }
    const auto* text = enumeration->uNext(enumeration, resultLength, status);
    if (!text) return nullptr;
    const auto capacity = Capacity(*Require(resultLength), 1);
    auto* result = static_cast<char*>(Buffer(enumeration, capacity));
    if (!result) {
        *Require(status) = 7;
        return nullptr;
    }
    u_UCharsToChars_nid_postfix(text, result, *resultLength + 1);
    return result;
}

Icu::UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const* strings, std::int32_t count, std::int32_t* status) {
    return Open(strings, count, false, status);
}

Icu::UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const* strings, std::int32_t count, std::int32_t* status) {
    return Open(strings, count, true, status);
}

void APS5_VABI uenum_reset_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration || *Require(status) > 0) return;
    if (enumeration->reset) enumeration->reset(enumeration, status);
    else *status = 16;
}

}
