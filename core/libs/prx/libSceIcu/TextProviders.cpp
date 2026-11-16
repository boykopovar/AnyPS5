// Derived from icu4c/source/common/utext.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): utext_setup, shallowTextClone, the UnicodeString and const UChar* providers.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2005-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using Text = Icu::Text;
using String = Icu::Abi::UnicodeString;

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t LengthIsExpensive = 1 << 1;
constexpr std::int32_t StableChunks = 1 << 2;
constexpr std::int32_t Writable = 1 << 3;
constexpr std::int32_t OwnsText = 1 << 5;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int64_t TextSize = sizeof(Text);
constexpr std::int32_t Int32Max = std::numeric_limits<std::int32_t>::max();
const char16_t EmptyString[] = {0};

template<class T>
T* Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("UText provider: null pointer");
    return pointer;
}

bool Lead(char16_t unit) {
    return (unit & 0xfc00) == 0xd800;
}

bool Trail(char16_t unit) {
    return (unit & 0xfc00) == 0xdc00;
}

void SetCodePointStart(const char16_t* string, std::int64_t& index) {
    if (index > 0 && Trail(string[index]) && Lead(string[index - 1])) --index;
}

std::int32_t PinIndex(std::int64_t& index, std::int64_t limit) {
    if (index < 0) index = 0;
    else if (index > limit) index = limit;
    return static_cast<std::int32_t>(index);
}

const String& Context(const Text* text) {
    return *Require(static_cast<const String*>(Require(text)->context));
}

String& MutableContext(const Text* text) {
    return const_cast<String&>(Context(text));
}

void AdjustPointer(Text* destination, const void** pointer, const Text* source) {
    const auto target = reinterpret_cast<std::uintptr_t>(*pointer);
    const auto extra = reinterpret_cast<std::uintptr_t>(source->extra);
    const auto self = reinterpret_cast<std::uintptr_t>(source);
    if (target >= extra && target < extra + static_cast<std::intptr_t>(source->extraSize))
        *pointer = static_cast<char*>(destination->extra) + (target - extra);
    else if (target >= self && target < self + static_cast<std::intptr_t>(source->sizeOfStruct))
        *pointer = reinterpret_cast<char*>(destination) + (target - self);
}

Text* ShallowClone(Text* destination, const Text* source, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    const auto sourceExtraSize = Require(source)->extraSize;
    destination = utext_setup_nid_postfix(destination, sourceExtraSize, status);
    if (*status > 0) return destination;
    auto* destinationExtra = destination->extra;
    const auto flags = destination->flags;
    auto size = source->sizeOfStruct;
    if (size > destination->sizeOfStruct) size = destination->sizeOfStruct;
    if (size > 0) std::memcpy(destination, source, static_cast<std::size_t>(size));
    destination->extra = destinationExtra;
    destination->flags = flags;
    if (sourceExtraSize > 0) std::memcpy(destination->extra, source->extra, static_cast<std::size_t>(sourceExtraSize));
    AdjustPointer(destination, &destination->context, source);
    AdjustPointer(destination, &destination->p, source);
    AdjustPointer(destination, &destination->q, source);
    AdjustPointer(destination, &destination->r, source);
    AdjustPointer(destination, reinterpret_cast<const void**>(&destination->chunkContents), source);
    destination->providerProperties &= ~OwnsText;
    return destination;
}

Text* APS5_VABI StringClone_nid_no_patch(Text* destination, const Text* source, std::int8_t deep, std::int32_t* status) {
    destination = ShallowClone(destination, source, status);
    if (deep && *status <= 0) {
        const auto& string = Context(source);
        auto* copy = static_cast<String*>(Icu::Abi::Allocate_nid_no_patch(sizeof(String)));
        if (copy) _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(copy, &string);
        destination->context = copy;
        destination->providerProperties |= OwnsText;
        destination->providerProperties |= Writable;
    }
    return destination;
}

void APS5_VABI StringClose_nid_no_patch(Text* text) {
    if (Require(text)->providerProperties & OwnsText) {
        auto* string = static_cast<String*>(const_cast<void*>(text->context));
        if (string) _ZN6icu_6813UnicodeStringD0Ev_nid_postfix(string);
        text->context = nullptr;
    }
}

std::int64_t APS5_VABI StringLength_nid_no_patch(Text* text) {
    return Context(text).Length();
}

std::int8_t APS5_VABI StringAccess_nid_no_patch(Text* text, std::int64_t index, std::int8_t forward) {
    const auto length = Require(text)->chunkLength;
    text->chunkOffset = PinIndex(index, length);
    return (forward && index < length) || (!forward && index > 0);
}

std::int32_t APS5_VABI StringExtract_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    const auto& string = Context(text);
    auto length = string.Length();
    if (Icu::Failure(status)) return 0;
    if (capacity < 0 || (!destination && capacity > 0)) *status = Icu::IllegalArgument;
    if (start < 0 || start > limit) {
        *status = IndexOutOfBounds;
        return 0;
    }
    const auto start32 = start < length ?
        _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(&string, static_cast<std::int32_t>(start)) : length;
    const auto limit32 = limit < length ?
        _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(&string, static_cast<std::int32_t>(limit)) : length;
    length = limit32 - start32;
    if (capacity > 0 && destination) {
        auto trimmed = length;
        if (trimmed > capacity) trimmed = capacity;
        _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(&string, start32, trimmed, destination, 0);
        text->chunkOffset = start32 + trimmed;
    } else {
        text->chunkOffset = start32;
    }
    u_terminateUChars_nid_postfix(destination, capacity, length, status);
    return length;
}

std::int32_t APS5_VABI StringReplace_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    const char16_t* source, std::int32_t length, std::int32_t* status) {
    auto& string = MutableContext(text);
    if (Icu::Failure(status)) return 0;
    if (!source && length != 0) *status = Icu::IllegalArgument;
    if (start > limit) {
        *status = IndexOutOfBounds;
        return 0;
    }
    const auto oldLength = string.Length();
    auto start32 = PinIndex(start, oldLength);
    auto limit32 = PinIndex(limit, oldLength);
    if (start32 < oldLength) start32 = _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(&string, start32);
    if (limit32 < oldLength) limit32 = _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(&string, limit32);
    _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(&string, start32, limit32 - start32, source, 0, length);
    const auto newLength = string.Length();
    text->chunkContents = string.Buffer();
    text->chunkLength = newLength;
    text->chunkNativeLimit = newLength;
    text->nativeIndexingLimit = newLength;
    const auto delta = newLength - oldLength;
    text->chunkOffset = limit32 + delta;
    return delta;
}

void APS5_VABI StringCopy_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit, std::int64_t destination,
    std::int8_t move, std::int32_t* status) {
    auto& string = MutableContext(text);
    const auto length = string.Length();
    if (Icu::Failure(status)) return;
    auto start32 = PinIndex(start, length);
    const auto limit32 = PinIndex(limit, length);
    const auto destination32 = PinIndex(destination, length);
    if (start32 > limit32 || (start32 < destination32 && destination32 < limit32)) {
        *status = IndexOutOfBounds;
        return;
    }
    if (move) {
        const auto segment = limit32 - start32;
        _ZN6icu_6813UnicodeString4copyEiii_nid_postfix(&string, start32, limit32, destination32);
        if (destination32 < start32) start32 += segment;
        _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(&string, start32, segment, nullptr, 0, 0);
    } else {
        _ZN6icu_6813UnicodeString4copyEiii_nid_postfix(&string, start32, limit32, destination32);
    }
    text->chunkContents = string.Buffer();
    if (!move) {
        text->chunkLength += limit32 - start32;
        text->chunkNativeLimit = text->chunkLength;
        text->nativeIndexingLimit = text->chunkLength;
    }
    text->chunkOffset = destination32 + limit32 - start32;
    if (move && destination32 > start32) text->chunkOffset = destination32;
}

const Icu::TextFunctions StringFunctions = {
    sizeof(Icu::TextFunctions), 0, 0, 0,
    StringClone_nid_no_patch, StringLength_nid_no_patch, StringAccess_nid_no_patch, StringExtract_nid_no_patch,
    StringReplace_nid_no_patch, StringCopy_nid_no_patch, nullptr, nullptr, StringClose_nid_no_patch,
    nullptr, nullptr, nullptr};

const char16_t* Characters(const Text* text) {
    return Require(static_cast<const char16_t*>(Require(text)->context));
}

Text* APS5_VABI CharactersClone_nid_no_patch(Text* destination, const Text* source, std::int8_t deep, std::int32_t* status) {
    destination = ShallowClone(destination, source, status);
    if (deep && *status <= 0) {
        const auto length = static_cast<std::int32_t>(utext_nativeLength_nid_postfix(destination));
        const auto* string = Characters(source);
        auto* copy = static_cast<char16_t*>(uprv_malloc_nid_postfix(
            static_cast<std::size_t>(static_cast<std::int64_t>(length) + 1) * sizeof(char16_t)));
        if (!copy) {
            *status = MemoryAllocationError;
        } else {
            for (std::int64_t i = 0; i < length; ++i) copy[i] = string[i];
            copy[length] = 0;
            destination->context = copy;
            destination->providerProperties |= OwnsText;
        }
    }
    return destination;
}

void APS5_VABI CharactersClose_nid_no_patch(Text* text) {
    if (Require(text)->providerProperties & OwnsText) {
        IcuReleaseMemory_nid_no_patch(const_cast<void*>(text->context));
        text->context = nullptr;
    }
}

std::int64_t APS5_VABI CharactersLength_nid_no_patch(Text* text) {
    if (Require(text)->a < 0) {
        const auto* string = Characters(text);
        while (string[text->chunkNativeLimit] != 0) ++text->chunkNativeLimit;
        text->a = text->chunkNativeLimit;
        text->chunkLength = static_cast<std::int32_t>(text->chunkNativeLimit);
        text->nativeIndexingLimit = text->chunkLength;
        text->providerProperties &= ~LengthIsExpensive;
    }
    return text->a;
}

std::int8_t APS5_VABI CharactersAccess_nid_no_patch(Text* text, std::int64_t index, std::int8_t forward) {
    const auto* string = Characters(text);
    if (index < 0) {
        index = 0;
    } else if (index < text->chunkNativeLimit) {
        SetCodePointStart(string, index);
    } else if (text->a >= 0) {
        index = text->a;
    } else {
        auto scanLimit = static_cast<std::int32_t>(static_cast<std::uint32_t>(index) + 32u);
        if (index + 32 > Int32Max || index + 32 < 0) scanLimit = Int32Max;
        auto chunkLimit = static_cast<std::int32_t>(text->chunkNativeLimit);
        bool found = false;
        for (; chunkLimit < scanLimit; ++chunkLimit) {
            if (string[chunkLimit] == 0) {
                text->a = chunkLimit;
                text->chunkLength = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                if (index >= chunkLimit) index = chunkLimit;
                else SetCodePointStart(string, index);
                text->chunkNativeLimit = chunkLimit;
                text->providerProperties &= ~LengthIsExpensive;
                found = true;
                break;
            }
        }
        if (!found) {
            SetCodePointStart(string, index);
            if (chunkLimit == Int32Max) {
                text->a = chunkLimit;
                text->chunkLength = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                if (index > chunkLimit) index = chunkLimit;
                text->chunkNativeLimit = chunkLimit;
                text->providerProperties &= ~LengthIsExpensive;
            } else {
                if (Lead(string[chunkLimit - 1])) --chunkLimit;
                text->chunkNativeLimit = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                text->chunkLength = chunkLimit;
            }
        }
    }
    text->chunkOffset = static_cast<std::int32_t>(index);
    return (forward && index < text->chunkNativeLimit) || (!forward && index > 0);
}

std::int32_t APS5_VABI CharactersExtract_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (capacity < 0 || (!destination && capacity > 0) || start > limit) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    CharactersAccess_nid_no_patch(text, start, 1);
    const auto* string = Require(text->chunkContents);
    const auto start32 = text->chunkOffset;
    auto length = static_cast<std::int32_t>(text->a);
    auto limit32 = length >= 0 ? PinIndex(limit, length) : PinIndex(limit, Int32Max);
    std::int32_t target = 0;
    std::int32_t index = start32;
    for (; index < limit32; ++index) {
        if (length < 0 && string[index] == 0) {
            text->a = index;
            text->chunkNativeLimit = index;
            text->chunkLength = index;
            text->nativeIndexingLimit = index;
            length = index;
            limit32 = index;
            break;
        }
        if (target < capacity) {
            destination[target] = string[index];
        } else if (length >= 0) {
            target = limit32 - start32;
            index = limit32;
            break;
        }
        ++target;
    }
    if (index > 0 && Lead(string[index - 1]) && ((index < length || length < 0) && Trail(string[index]))) {
        if (target < capacity) destination[target++] = string[index];
        ++index;
    }
    if (index <= text->chunkNativeLimit) text->chunkOffset = index;
    else CharactersAccess_nid_no_patch(text, index, 1);
    u_terminateUChars_nid_postfix(destination, capacity, target, status);
    return target;
}

const Icu::TextFunctions CharactersFunctions = {
    sizeof(Icu::TextFunctions), 0, 0, 0,
    CharactersClone_nid_no_patch, CharactersLength_nid_no_patch, CharactersAccess_nid_no_patch,
    CharactersExtract_nid_no_patch, nullptr, nullptr, nullptr, nullptr, CharactersClose_nid_no_patch,
    nullptr, nullptr, nullptr};

}

extern "C" {

Icu::Text* APS5_VABI utext_setup_nid_postfix(Icu::Text* text, std::int32_t extraSpace, std::int32_t* status) {
    if (Icu::Failure(status)) return text;
    if (!text) {
        auto required = static_cast<std::int32_t>(TextSize);
        if (extraSpace > 0)
            required = static_cast<std::int32_t>(static_cast<std::uint32_t>(TextSize) + static_cast<std::uint32_t>(extraSpace));
        text = static_cast<Text*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(static_cast<std::int64_t>(required))));
        if (!text) {
            *status = MemoryAllocationError;
            return nullptr;
        }
        *text = Text{};
        text->magic = Magic;
        text->sizeOfStruct = static_cast<std::int32_t>(TextSize);
        text->flags |= HeapAllocated;
        text->extraSize = extraSpace;
        text->extra = reinterpret_cast<char*>(text) + TextSize;
    } else {
        if (text->magic != Magic) {
            *status = Icu::IllegalArgument;
            return text;
        }
        if (text->flags & Open) {
            const auto* functions = Require(text->functions);
            if (functions->close) functions->close(text);
        }
        text->flags &= ~Open;
        if (extraSpace > text->extraSize) {
            if (text->flags & ExtraHeapAllocated) {
                IcuReleaseMemory_nid_no_patch(text->extra);
                text->extraSize = 0;
            }
            text->extra = uprv_malloc_nid_postfix(static_cast<std::size_t>(extraSpace));
            if (!text->extra) {
                *status = MemoryAllocationError;
            } else {
                text->extraSize = extraSpace;
                text->flags |= ExtraHeapAllocated;
            }
        }
    }
    if (*status <= 0) {
        text->flags |= Open;
        text->context = nullptr;
        text->chunkContents = nullptr;
        text->p = nullptr;
        text->q = nullptr;
        text->r = nullptr;
        text->a = 0;
        text->b = 0;
        text->c = 0;
        text->chunkOffset = 0;
        text->chunkLength = 0;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = 0;
        text->nativeIndexingLimit = 0;
        text->providerProperties = 0;
        text->privateA = 0;
        text->privateB = 0;
        text->privateC = 0;
        text->privatePointer = nullptr;
        if (text->extra && text->extraSize > 0) std::memset(text->extra, 0, static_cast<std::size_t>(text->extraSize));
    }
    return text;
}

Icu::Text* APS5_VABI utext_openUChars_nid_postfix(Icu::Text* text, const char16_t* string, std::int64_t length,
    std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (!string && length == 0) string = EmptyString;
    if (!string || length < -1 || length > Int32Max) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    text = utext_setup_nid_postfix(text, 0, status);
    if (*status <= 0) {
        text->functions = &CharactersFunctions;
        text->context = string;
        text->providerProperties = StableChunks;
        if (length == -1) text->providerProperties |= LengthIsExpensive;
        text->a = length;
        text->chunkContents = string;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = length >= 0 ? length : 0;
        text->chunkLength = static_cast<std::int32_t>(text->chunkNativeLimit);
        text->chunkOffset = 0;
        text->nativeIndexingLimit = text->chunkLength;
    }
    return text;
}

Icu::Text* APS5_VABI utext_openConstUnicodeString_nid_postfix(Icu::Text* text, const Icu::Abi::UnicodeString* string,
    std::int32_t* status) {
    if (!Icu::Failure(status) && (Require(string)->Flags() & String::IsBogus)) {
        utext_openUChars_nid_postfix(text, nullptr, 0, status);
        *status = Icu::IllegalArgument;
        return text;
    }
    text = utext_setup_nid_postfix(text, 0, status);
    if (*status <= 0) {
        text->functions = &StringFunctions;
        text->context = string;
        text->providerProperties = StableChunks;
        text->chunkContents = string->Buffer();
        text->chunkLength = string->Length();
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = text->chunkLength;
        text->nativeIndexingLimit = text->chunkLength;
    }
    return text;
}

}
