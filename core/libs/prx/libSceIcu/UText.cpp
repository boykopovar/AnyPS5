// Derived from icu4c/source/common/utext.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2005-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t LengthIsExpensive = 1 << 1;
constexpr std::int32_t StableChunks = 1 << 2;
constexpr std::int32_t OwnsText = 1 << 5;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t Int32Max = INT32_MAX;
constexpr char16_t EmptyString[] = {0};

const UTextFuncs& Functions(const UText* ut) {
    if (!ut->pFuncs) throw std::invalid_argument("libSceIcu: open UText without a function table");
    return *ut->pFuncs;
}

bool IsLead(char16_t unit) {
    return (unit & 0xfc00) == 0xd800;
}

bool IsTrail(char16_t unit) {
    return (unit & 0xfc00) == 0xdc00;
}

void SetCodePointStart(const char16_t* text, std::int64_t& index) {
    if (IsTrail(text[index]) && index > 0 && IsLead(text[index - 1])) --index;
}

std::int32_t PinIndex(std::int64_t& index, std::int64_t limit) {
    if (index < 0) index = 0;
    else if (index > limit) index = limit;
    return static_cast<std::int32_t>(index);
}

bool Within(std::uintptr_t address, std::uintptr_t base, std::int32_t size) {
    return size > 0 && address >= base && address - base < static_cast<std::uintptr_t>(size);
}

template<class T>
T* Relocated(T* pointer, const UText* dest, const UText* src) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto sourceExtra = reinterpret_cast<std::uintptr_t>(src->pExtra);
    const auto sourceText = reinterpret_cast<std::uintptr_t>(src);
    if (Within(address, sourceExtra, src->extraSize))
        return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(dest->pExtra) + (address - sourceExtra));
    if (Within(address, sourceText, src->sizeOfStruct))
        return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(dest) + (address - sourceText));
    return pointer;
}

UText* ShallowClone(UText* dest, const UText* src, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    const std::int32_t sourceExtraSize = src->extraSize;
    dest = utext_setup(dest, sourceExtraSize, status);
    if (*status > 0) return dest;
    void* const destExtra = dest->pExtra;
    const std::int32_t flags = dest->flags;
    std::int32_t sizeToCopy = src->sizeOfStruct;
    if (sizeToCopy > dest->sizeOfStruct) sizeToCopy = dest->sizeOfStruct;
    std::memcpy(static_cast<void*>(dest), src, static_cast<std::size_t>(sizeToCopy));
    dest->pExtra = destExtra;
    dest->flags = flags;
    if (sourceExtraSize > 0) std::memcpy(dest->pExtra, src->pExtra, static_cast<std::size_t>(sourceExtraSize));
    dest->context = Relocated(dest->context, dest, src);
    dest->p = Relocated(dest->p, dest, src);
    dest->q = Relocated(dest->q, dest, src);
    dest->r = Relocated(dest->r, dest, src);
    dest->chunkContents = Relocated(dest->chunkContents, dest, src);
    dest->providerProperties &= ~OwnsText;
    return dest;
}

UText* APS5_VABI StringClone_nid_no_patch(UText* dest, const UText* src, std::int8_t deep, std::int32_t* status) {
    dest = ShallowClone(dest, src, status);
    if (deep && *status <= 0) {
        const auto length = static_cast<std::int32_t>(Functions(dest).nativeLength(dest));
        const auto* source = static_cast<const char16_t*>(src->context);
        auto* copy = static_cast<char16_t*>(uprv_malloc_nid_postfix((static_cast<std::size_t>(length) + 1) * sizeof(char16_t)));
        if (!copy) {
            *status = MemoryAllocation;
        } else {
            for (std::int32_t index = 0; index < length; ++index) copy[index] = source[index];
            copy[length] = 0;
            dest->context = copy;
            dest->providerProperties |= OwnsText;
        }
    }
    return dest;
}

void APS5_VABI StringClose_nid_no_patch(UText* ut) {
    if (ut->providerProperties & OwnsText) {
        IcuReleaseMemory_nid_no_patch(const_cast<void*>(ut->context));
        ut->context = nullptr;
    }
}

std::int64_t APS5_VABI StringLength_nid_no_patch(UText* ut) {
    if (ut->a < 0) {
        const auto* text = static_cast<const char16_t*>(ut->context);
        while (text[ut->chunkNativeLimit] != 0) ++ut->chunkNativeLimit;
        ut->a = ut->chunkNativeLimit;
        ut->chunkLength = static_cast<std::int32_t>(ut->chunkNativeLimit);
        ut->nativeIndexingLimit = ut->chunkLength;
        ut->providerProperties &= ~LengthIsExpensive;
    }
    return ut->a;
}

void SetKnownLength(UText* ut, std::int32_t length) {
    ut->a = length;
    ut->chunkLength = length;
    ut->nativeIndexingLimit = length;
    ut->chunkNativeLimit = length;
    ut->providerProperties &= ~LengthIsExpensive;
}

void Scan(UText* ut, const char16_t* text, std::int64_t& index) {
    const std::int32_t scanLimit = index > Int32Max - 32 ? Int32Max : static_cast<std::int32_t>(index + 32);
    auto chunkLimit = static_cast<std::int32_t>(ut->chunkNativeLimit);
    for (; chunkLimit < scanLimit; ++chunkLimit) {
        if (text[chunkLimit] != 0) continue;
        if (index >= chunkLimit) index = chunkLimit;
        else SetCodePointStart(text, index);
        SetKnownLength(ut, chunkLimit);
        return;
    }
    SetCodePointStart(text, index);
    if (chunkLimit == Int32Max) {
        if (index > chunkLimit) index = chunkLimit;
        SetKnownLength(ut, chunkLimit);
        return;
    }
    if (IsLead(text[chunkLimit - 1])) --chunkLimit;
    ut->chunkNativeLimit = chunkLimit;
    ut->nativeIndexingLimit = chunkLimit;
    ut->chunkLength = chunkLimit;
}

std::int8_t APS5_VABI StringAccess_nid_no_patch(UText* ut, std::int64_t index, std::int8_t forward) {
    const auto* text = static_cast<const char16_t*>(ut->context);
    if (index < 0) index = 0;
    else if (index < ut->chunkNativeLimit) SetCodePointStart(text, index);
    else if (ut->a >= 0) index = ut->a;
    else Scan(ut, text, index);
    ut->chunkOffset = static_cast<std::int32_t>(index);
    return (forward && index < ut->chunkNativeLimit) || (!forward && index > 0);
}

std::int32_t APS5_VABI StringExtract_nid_no_patch(UText* ut, std::int64_t start, std::int64_t limit, char16_t* dest,
                                                  std::int32_t destCapacity, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (destCapacity < 0 || (!dest && destCapacity > 0) || start > limit) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    StringAccess_nid_no_patch(ut, start, 1);
    const char16_t* text = ut->chunkContents;
    const std::int32_t start32 = ut->chunkOffset;
    auto length = static_cast<std::int32_t>(ut->a);
    std::int32_t limit32 = PinIndex(limit, length >= 0 ? length : Int32Max);
    std::int32_t written = 0;
    std::int32_t source = start32;
    for (; source < limit32; ++source) {
        if (length < 0 && text[source] == 0) {
            ut->a = source;
            ut->chunkNativeLimit = source;
            ut->chunkLength = source;
            ut->nativeIndexingLimit = source;
            length = source;
            limit32 = source;
            break;
        }
        if (written < destCapacity) {
            dest[written] = text[source];
        } else if (length >= 0) {
            written = limit32 - start32;
            source = limit32;
            break;
        }
        ++written;
    }
    if (source > 0 && IsLead(text[source - 1]) && (source < length || length < 0) && IsTrail(text[source])) {
        if (written < destCapacity) dest[written++] = text[source];
        ++source;
    }
    if (source <= ut->chunkNativeLimit) ut->chunkOffset = source;
    else StringAccess_nid_no_patch(ut, source, 1);
    Icu::Terminate(dest, destCapacity, written, status);
    return written;
}

constexpr UTextFuncs StringFunctions{
    sizeof(UTextFuncs), 0, 0, 0, StringClone_nid_no_patch, StringLength_nid_no_patch, StringAccess_nid_no_patch,
    StringExtract_nid_no_patch, nullptr, nullptr, nullptr, nullptr, StringClose_nid_no_patch, nullptr, nullptr, nullptr};

}

extern "C" {

UText* APS5_VABI utext_setup(UText* ut, std::int32_t extraSpace, std::int32_t* status) {
    if (Icu::Failure(status)) return ut;
    if (!ut) {
        auto spaceRequired = static_cast<std::int32_t>(sizeof(UText));
        if (extraSpace > 0)
            spaceRequired = static_cast<std::int32_t>(static_cast<std::uint32_t>(sizeof(UText)) + static_cast<std::uint32_t>(extraSpace));
        ut = static_cast<UText*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(spaceRequired)));
        if (!ut) {
            *status = MemoryAllocation;
            return nullptr;
        }
        *ut = UText{};
        ut->magic = Magic;
        ut->sizeOfStruct = sizeof(UText);
        ut->flags |= HeapAllocated;
        if (spaceRequired > 0) {
            ut->extraSize = extraSpace;
            ut->pExtra = ut + 1;
        }
    } else {
        if (ut->magic != Magic) {
            *status = Icu::IllegalArgument;
            return ut;
        }
        if ((ut->flags & Open) && Functions(ut).close) ut->pFuncs->close(ut);
        ut->flags &= ~Open;
        if (extraSpace > ut->extraSize) {
            if (ut->flags & ExtraHeapAllocated) {
                IcuReleaseMemory_nid_no_patch(ut->pExtra);
                ut->extraSize = 0;
            }
            ut->pExtra = uprv_malloc_nid_postfix(static_cast<std::size_t>(extraSpace));
            if (!ut->pExtra) {
                *status = MemoryAllocation;
            } else {
                ut->extraSize = extraSpace;
                ut->flags |= ExtraHeapAllocated;
            }
        }
    }
    if (*status <= 0) {
        ut->flags |= Open;
        ut->context = nullptr;
        ut->chunkContents = nullptr;
        ut->p = nullptr;
        ut->q = nullptr;
        ut->r = nullptr;
        ut->a = 0;
        ut->b = 0;
        ut->c = 0;
        ut->chunkOffset = 0;
        ut->chunkLength = 0;
        ut->chunkNativeStart = 0;
        ut->chunkNativeLimit = 0;
        ut->nativeIndexingLimit = 0;
        ut->providerProperties = 0;
        ut->privA = 0;
        ut->privB = 0;
        ut->privC = 0;
        ut->privP = nullptr;
        if (ut->pExtra && ut->extraSize > 0) std::memset(ut->pExtra, 0, static_cast<std::size_t>(ut->extraSize));
    }
    return ut;
}

UText* APS5_VABI utext_close(UText* ut) {
    if (!ut || ut->magic != Magic || (ut->flags & Open) == 0) return ut;
    if (Functions(ut).close) ut->pFuncs->close(ut);
    ut->flags &= ~Open;
    if (ut->flags & ExtraHeapAllocated) {
        IcuReleaseMemory_nid_no_patch(ut->pExtra);
        ut->pExtra = nullptr;
        ut->flags &= ~ExtraHeapAllocated;
        ut->extraSize = 0;
    }
    ut->pFuncs = nullptr;
    if (ut->flags & HeapAllocated) {
        ut->magic = 0;
        IcuReleaseMemory_nid_no_patch(ut);
        ut = nullptr;
    }
    return ut;
}

UText* APS5_VABI utext_openUChars(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (!s && length == 0) s = EmptyString;
    if (!s || length < -1 || length > Int32Max) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    ut = utext_setup(ut, 0, status);
    if (*status <= 0) {
        ut->pFuncs = &StringFunctions;
        ut->context = s;
        ut->providerProperties = StableChunks;
        if (length == -1) ut->providerProperties |= LengthIsExpensive;
        ut->a = length;
        ut->chunkContents = s;
        ut->chunkNativeStart = 0;
        ut->chunkNativeLimit = length >= 0 ? length : 0;
        ut->chunkLength = static_cast<std::int32_t>(ut->chunkNativeLimit);
        ut->chunkOffset = 0;
        ut->nativeIndexingLimit = ut->chunkLength;
    }
    return ut;
}

}
