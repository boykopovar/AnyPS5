// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/utext.cpp

#include "prx/libSceIcu/UText.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/GuestAccess.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>

namespace {

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t ExpensiveLength = 1 << 1;
constexpr std::int32_t StableChunks = 1 << 2;
constexpr std::int32_t OwnsText = 1 << 5;
constexpr std::int32_t MemoryError = 7;
constexpr auto MaximumLength = std::numeric_limits<std::int32_t>::max();
constexpr std::size_t ExtraOffset = (sizeof(UText) + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);

struct Release {
    void operator()(void* pointer) const { IcuReleaseMemory_nid_no_patch(pointer); }
};

using Allocation = std::unique_ptr<void, Release>;

struct CloseText {
    void operator()(UText* text) const { utext_close(text); }
};

bool Lead(char16_t value) { return value >= 0xd800 && value <= 0xdbff; }
bool Trail(char16_t value) { return value >= 0xdc00 && value <= 0xdfff; }

void CopyMemory(void* destination, const void* source, std::size_t bytes) {
    std::array<std::byte, 256> buffer{};
    for (std::size_t offset = 0; offset < bytes;) {
        const auto count = std::min(buffer.size(), bytes - offset);
        const auto* input = Icu::GuestElement(static_cast<const std::byte*>(source), offset);
        auto* output = Icu::GuestElement(static_cast<std::byte*>(destination), offset);
        if (!GuestBytes(input).CopyIn(buffer.data(), count) || !GuestBytes(output).CopyOut(buffer.data(), count))
            APS5_INVALID_ARG_EX;
        offset += count;
    }
}

void CloseProvider(UText* text, UText& value) {
    const auto functions = Icu::ReadGuest(value.pFuncs);
    if (functions.close) {
        functions.close(text);
        value = Icu::ReadGuest(text);
    }
}

void SetLength(UText& value, std::int32_t length) {
    value.a = length;
    value.chunkNativeLimit = length;
    value.chunkLength = length;
    value.nativeIndexingLimit = length;
    value.providerProperties &= ~ExpensiveLength;
}

std::int64_t Boundary(const char16_t* source, std::int64_t index) {
    if (index > 0 && Trail(Icu::ReadGuest(source, index)) && Lead(Icu::ReadGuest(source, index - 1))) --index;
    return index;
}

std::int64_t APS5_VABI UCharsLength_nid_no_patch(UText* text) {
    const Icu::SavedErrno savedErrno;
    auto value = Icu::ReadGuest(text);
    if (value.a < 0) {
        auto length = value.chunkNativeLimit;
        const auto* source = static_cast<const char16_t*>(value.context);
        while (Icu::ReadGuest(source, length) != 0) {
            if (length == MaximumLength) APS5_INVALID_ARG_EX;
            ++length;
        }
        SetLength(value, static_cast<std::int32_t>(length));
        Icu::WriteGuest(text, value);
    }
    return value.a;
}

std::int8_t APS5_VABI UCharsAccess_nid_no_patch(UText* text, std::int64_t index, std::int8_t forward) {
    const Icu::SavedErrno savedErrno;
    auto value = Icu::ReadGuest(text);
    const auto* source = static_cast<const char16_t*>(value.context);
    if (index < 0) {
        index = 0;
    } else if (index < value.chunkNativeLimit) {
        index = Boundary(source, index);
    } else if (value.a >= 0) {
        index = value.a;
    } else {
        const auto limit = index > MaximumLength - 32 ? MaximumLength : index + 32;
        auto scanned = value.chunkNativeLimit;
        while (scanned < limit && Icu::ReadGuest(source, scanned) != 0) ++scanned;
        if (scanned < limit || scanned == MaximumLength) {
            SetLength(value, static_cast<std::int32_t>(scanned));
            index = index >= scanned ? scanned : Boundary(source, index);
        } else {
            index = Boundary(source, index);
            if (Lead(Icu::ReadGuest(source, scanned - 1))) --scanned;
            value.chunkNativeLimit = scanned;
            value.chunkLength = static_cast<std::int32_t>(scanned);
            value.nativeIndexingLimit = value.chunkLength;
        }
    }
    value.chunkOffset = static_cast<std::int32_t>(index);
    Icu::WriteGuest(text, value);
    return forward ? index < value.chunkNativeLimit : index > 0;
}

std::int32_t APS5_VABI UCharsExtract_nid_no_patch(UText* text, std::int64_t start, std::int64_t limit,
                                                char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    const Icu::SavedErrno savedErrno;
    if (Icu::ReadGuest(status) > 0) return 0;
    if (capacity < 0 || (!destination && capacity > 0) || start > limit) {
        Icu::WriteGuest(status, Icu::IllegalArgument);
        return 0;
    }
    UCharsAccess_nid_no_patch(text, start, 1);
    auto value = Icu::ReadGuest(text);
    const auto* source = static_cast<const char16_t*>(value.context);
    const auto last = std::clamp<std::int64_t>(limit, 0, value.a >= 0 ? value.a : MaximumLength);
    const auto first = value.chunkOffset;
    std::int64_t cursor = first;
    std::int32_t length = 0;
    for (; cursor < last; ++cursor) {
        if (value.a < 0 && Icu::ReadGuest(source, cursor) == 0) {
            value.a = cursor;
            value.chunkNativeLimit = cursor;
            value.chunkLength = static_cast<std::int32_t>(cursor);
            value.nativeIndexingLimit = value.chunkLength;
            break;
        }
        if (length < capacity) {
            Icu::WriteGuest(destination, Icu::ReadGuest(source, cursor), length);
        } else if (value.a >= 0) {
            length = static_cast<std::int32_t>(last - first);
            cursor = last;
            break;
        }
        ++length;
    }
    if (cursor > 0 && Lead(Icu::ReadGuest(source, cursor - 1)) &&
        (value.a < 0 || cursor < value.a) && Trail(Icu::ReadGuest(source, cursor))) {
        if (length < capacity) Icu::WriteGuest(destination, Icu::ReadGuest(source, cursor), length++);
        ++cursor;
    }
    if (cursor <= value.chunkNativeLimit) {
        value.chunkOffset = static_cast<std::int32_t>(cursor);
        Icu::WriteGuest(text, value);
    } else {
        Icu::WriteGuest(text, value);
        UCharsAccess_nid_no_patch(text, cursor, 1);
    }
    Icu::TerminateGuest(destination, capacity, length, status);
    return length;
}

const void* Relocate(const void* pointer, const UText& source, const UText* sourceAddress, const UText& target, const UText* targetAddress) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto extra = reinterpret_cast<std::uintptr_t>(source.pExtra);
    const auto object = reinterpret_cast<std::uintptr_t>(sourceAddress);
    if (source.extraSize > 0 && address >= extra && address - extra < static_cast<std::size_t>(source.extraSize))
        return Icu::GuestElement(static_cast<const std::byte*>(target.pExtra), address - extra);
    if (address >= object && address - object < static_cast<std::size_t>(source.sizeOfStruct))
        return Icu::GuestElement(reinterpret_cast<const std::byte*>(targetAddress), address - object);
    return pointer;
}

UText* APS5_VABI UCharsClone_nid_no_patch(UText* target, const UText* source, std::int8_t deep, std::int32_t* status) {
    const Icu::SavedErrno savedErrno;
    if (Icu::ReadGuest(status) > 0) return nullptr;
    if (target == source) APS5_INVALID_ARG_EX;
    const auto original = Icu::ReadGuest(source);
    if (original.sizeOfStruct != sizeof(UText)) APS5_INVALID_ARG_EX;
    const bool ownsTarget = target == nullptr;
    target = utext_setup(target, original.extraSize, status);
    if (Icu::ReadGuest(status) > 0) return target;
    const auto allocated = Icu::ReadGuest(target);
    auto clone = original;
    clone.flags = allocated.flags;
    clone.pExtra = allocated.pExtra;
    clone.providerProperties &= ~OwnsText;
    Icu::WriteGuest(target, clone);
    std::unique_ptr<UText, CloseText> cleanup(ownsTarget ? target : nullptr);
    if (original.extraSize > 0) CopyMemory(clone.pExtra, original.pExtra, original.extraSize);
    clone.context = Relocate(clone.context, original, source, clone, target);
    clone.p = Relocate(clone.p, original, source, clone, target);
    clone.q = Relocate(clone.q, original, source, clone, target);
    clone.r = Relocate(clone.r, original, source, clone, target);
    clone.chunkContents = static_cast<const char16_t*>(Relocate(clone.chunkContents, original, source, clone, target));
    Icu::WriteGuest(target, clone);
    if (deep) {
        const auto length = UCharsLength_nid_no_patch(target);
        Allocation storage(uprv_malloc_nid_postfix((static_cast<std::size_t>(length) + 1) * sizeof(char16_t)));
        if (!storage) {
            Icu::WriteGuest(status, MemoryError);
            cleanup.release();
            return target;
        }
        clone = Icu::ReadGuest(target);
        CopyMemory(storage.get(), clone.context, static_cast<std::size_t>(length) * sizeof(char16_t));
        Icu::WriteGuest(static_cast<char16_t*>(storage.get()), char16_t{}, length);
        clone.context = storage.get();
        clone.chunkContents = static_cast<const char16_t*>(storage.get());
        clone.providerProperties |= OwnsText;
        Icu::WriteGuest(target, clone);
        storage.release();
    }
    cleanup.release();
    return target;
}

void APS5_VABI UCharsClose_nid_no_patch(UText* text) {
    const Icu::SavedErrno savedErrno;
    auto value = Icu::ReadGuest(text);
    if ((value.providerProperties & OwnsText) == 0) return;
    const GuestArena::HostWrite access(text, sizeof(UText));
    if (!access.Open()) APS5_INVALID_ARG_EX;
    Icu::RequireWritable(text, sizeof(UText));
    IcuReleaseMemory_nid_no_patch(const_cast<void*>(value.context));
    value.context = nullptr;
    Icu::WriteGuest(text, value);
}

const UTextFuncs UCharsFunctions{
    sizeof(UTextFuncs), 0, 0, 0, UCharsClone_nid_no_patch, UCharsLength_nid_no_patch, UCharsAccess_nid_no_patch,
    UCharsExtract_nid_no_patch, nullptr, nullptr, nullptr, nullptr, UCharsClose_nid_no_patch, nullptr, nullptr, nullptr};
const char16_t EmptyString[] = {0};

}

extern "C" {

UText* APS5_VABI utext_setup(UText* text, std::int32_t extraSpace, std::int32_t* status) {
    const Icu::SavedErrno savedErrno;
    if (Icu::ReadGuest(status) > 0) return text;
    Allocation allocated;
    Allocation extra;
    UText value{};
    if (!text) {
        const auto bytes = extraSpace > 0 ? ExtraOffset + static_cast<std::size_t>(extraSpace) : sizeof(UText);
        allocated.reset(uprv_malloc_nid_postfix(bytes));
        if (!allocated) {
            Icu::WriteGuest(status, MemoryError);
            return nullptr;
        }
        text = static_cast<UText*>(allocated.get());
        value.magic = Magic;
        value.sizeOfStruct = sizeof(UText);
        value.flags = HeapAllocated;
        value.pExtra = Icu::GuestElement(static_cast<std::byte*>(allocated.get()), ExtraOffset);
        value.extraSize = extraSpace;
    } else {
        if (Icu::ReadGuest(reinterpret_cast<const std::uint32_t*>(text)) != Magic) {
            Icu::WriteGuest(status, Icu::IllegalArgument);
            return text;
        }
        value = Icu::ReadGuest(text);
        const GuestArena::HostWrite access(text, sizeof(UText));
        if (!access.Open()) APS5_INVALID_ARG_EX;
        Icu::RequireWritable(text, sizeof(UText));
        if ((value.flags & Open) != 0) CloseProvider(text, value);
        value.flags &= ~Open;
        Icu::WriteGuest(text, value);
        if (extraSpace > value.extraSize) {
            if ((value.flags & ExtraAllocated) != 0) IcuReleaseMemory_nid_no_patch(value.pExtra);
            value.pExtra = nullptr;
            value.extraSize = 0;
            value.flags &= ~ExtraAllocated;
            Icu::WriteGuest(text, value);
            extra.reset(uprv_malloc_nid_postfix(extraSpace));
            if (!extra) {
                Icu::WriteGuest(status, MemoryError);
                return text;
            }
            value.pExtra = extra.get();
            value.extraSize = extraSpace;
            value.flags |= ExtraAllocated;
        }
    }
    UText fresh{};
    fresh.magic = value.magic;
    fresh.sizeOfStruct = value.sizeOfStruct;
    fresh.flags = value.flags | Open;
    fresh.pExtra = value.pExtra;
    fresh.extraSize = value.extraSize;
    fresh.pFuncs = value.pFuncs;
    if (fresh.extraSize > 0 && fresh.pExtra) {
        const GuestArena::HostWrite access(fresh.pExtra, fresh.extraSize);
        if (!access.Open()) APS5_INVALID_ARG_EX;
        Icu::RequireWritable(fresh.pExtra, fresh.extraSize);
        std::memset(fresh.pExtra, 0, fresh.extraSize);
    }
    Icu::WriteGuest(text, fresh);
    allocated.release();
    extra.release();
    return text;
}

UText* APS5_VABI utext_openUChars(UText* text, const char16_t* source, std::int64_t length, std::int32_t* status) {
    const Icu::SavedErrno savedErrno;
    if (Icu::ReadGuest(status) > 0) return nullptr;
    if ((!source && length != 0) || length < -1 || length > MaximumLength) {
        Icu::WriteGuest(status, Icu::IllegalArgument);
        return nullptr;
    }
    text = utext_setup(text, 0, status);
    if (Icu::ReadGuest(status) > 0) return text;
    auto value = Icu::ReadGuest(text);
    value.pFuncs = &UCharsFunctions;
    value.context = source ? source : EmptyString;
    value.chunkContents = static_cast<const char16_t*>(value.context);
    value.providerProperties = StableChunks | (length < 0 ? ExpensiveLength : 0);
    value.a = length;
    value.chunkNativeLimit = std::max<std::int64_t>(length, 0);
    value.chunkLength = static_cast<std::int32_t>(value.chunkNativeLimit);
    value.nativeIndexingLimit = value.chunkLength;
    Icu::WriteGuest(text, value);
    return text;
}

UText* APS5_VABI utext_close(UText* text) {
    const Icu::SavedErrno savedErrno;
    if (!text || Icu::ReadGuest(reinterpret_cast<const std::uint32_t*>(text)) != Magic) return text;
    auto value = Icu::ReadGuest(text);
    if ((value.flags & Open) == 0) return text;
    {
        const GuestArena::HostWrite access(text, sizeof(UText));
        if (!access.Open()) APS5_INVALID_ARG_EX;
        Icu::RequireWritable(text, sizeof(UText));
        CloseProvider(text, value);
        value.flags &= ~Open;
        if ((value.flags & ExtraAllocated) != 0) {
            IcuReleaseMemory_nid_no_patch(value.pExtra);
            value.pExtra = nullptr;
            value.extraSize = 0;
            value.flags &= ~ExtraAllocated;
        }
        value.pFuncs = nullptr;
        if ((value.flags & HeapAllocated) != 0) value.magic = 0;
        Icu::WriteGuest(text, value);
    }
    if ((value.flags & HeapAllocated) != 0) {
        IcuReleaseMemory_nid_no_patch(text);
        return nullptr;
    }
    return text;
}

}
