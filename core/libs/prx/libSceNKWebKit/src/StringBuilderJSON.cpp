/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
 * Copyright (C) 2017 Mozilla Foundation. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Derived from Source/WTF/wtf/text/StringBuilderJSON.cpp (WebKit c55364d53f97)

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "WtfTextAbi.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(sizeof(bool) == 1);

constexpr unsigned MaxLength = 0x7FFFFFFF;
constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr unsigned kStringImplBufferOwnershipMask = 3u;
constexpr unsigned kStringImplBufferSubstring = 2u;
constexpr std::size_t kSubstringHeaderBytes = 24u + sizeof(StringImpl*);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

StringImpl ReadGuestStringImpl(const StringImpl* pointer, const char* function) {
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, pointer, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return header;
}

struct PlannedReleaseState {
    StringImpl* impl;
    unsigned refCount;
    unsigned hashAndFlags;
};

void ValidatePlannedRelease(StringImpl* impl, std::vector<PlannedReleaseState>& states, const char* function, unsigned depth = 0) {
    if (!impl) return;
    if (depth >= 64u) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    PlannedReleaseState* state = nullptr;
    for (auto& entry : states) {
        if (entry.impl == impl) {
            state = &entry;
            break;
        }
    }
    if (!state) {
        const StringImpl header = ReadGuestStringImpl(impl, function);
        RequireWritableGuestBytes(impl, sizeof(StringImpl), function);
        states.push_back({impl, header.refCount, header.hashAndFlags});
        state = &states.back();
    }
    if (state->refCount != 2u) {
        state->refCount -= 2u;
        return;
    }
    state->refCount = 0u;
    if ((state->hashAndFlags & kStringImplBufferOwnershipMask) == kStringImplBufferSubstring) {
        RequireReadableGuestBytes(impl, kSubstringHeaderBytes, function);
        StringImpl* owner = nullptr;
        if (!GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(impl) + 24, sizeof(owner)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (owner) ValidatePlannedRelease(owner, states, function, depth + 1u);
    }
}

void ValidateReleasableStrings(StringImpl* first, StringImpl* second, const char* function) {
    std::vector<PlannedReleaseState> states;
    if (first) ValidatePlannedRelease(first, states, function);
    if (second) ValidatePlannedRelease(second, states, function);
}

void UnpackStringBuilder(const unsigned char* raw, StringBuilder& builder) {
    StringImpl* stringImpl = nullptr;
    std::memcpy(&stringImpl, raw + offsetof(StringBuilder, string), sizeof(stringImpl));
    builder.string.impl = stringImpl;
    std::memcpy(&builder.buffer, raw + offsetof(StringBuilder, buffer), sizeof(builder.buffer));
    std::memcpy(&builder.characters8, raw + offsetof(StringBuilder, characters8), sizeof(builder.characters8));
    builder.overflowed = raw[offsetof(StringBuilder, overflowed)] != 0;
    builder.shouldCrashOnOverflow = raw[offsetof(StringBuilder, shouldCrashOnOverflow)] != 0;
    std::memcpy(&builder.length, raw + offsetof(StringBuilder, length), sizeof(builder.length));
    builder.is8Bit = raw[offsetof(StringBuilder, is8Bit)] != 0;
}

void PackStringBuilder(const StringBuilder& builder, unsigned char* raw) {
    const StringImpl* stringImpl = builder.string.impl;
    std::memcpy(raw + offsetof(StringBuilder, string), &stringImpl, sizeof(stringImpl));
    std::memcpy(raw + offsetof(StringBuilder, buffer), &builder.buffer, sizeof(builder.buffer));
    std::memcpy(raw + offsetof(StringBuilder, characters8), &builder.characters8, sizeof(builder.characters8));
    raw[offsetof(StringBuilder, overflowed)] = builder.overflowed ? 1 : 0;
    raw[offsetof(StringBuilder, shouldCrashOnOverflow)] = builder.shouldCrashOnOverflow ? 1 : 0;
    std::memcpy(raw + offsetof(StringBuilder, length), &builder.length, sizeof(builder.length));
    raw[offsetof(StringBuilder, is8Bit)] = builder.is8Bit ? 1 : 0;
}

struct BorrowedStringBuilder {
    StringBuilder value{};

    explicit BorrowedStringBuilder(const unsigned char* raw) {
        UnpackStringBuilder(raw, value);
    }

    ~BorrowedStringBuilder() {
        value.string.impl = nullptr;
    }

    BorrowedStringBuilder(const BorrowedStringBuilder&) = delete;
    BorrowedStringBuilder& operator=(const BorrowedStringBuilder&) = delete;
};

constexpr unsigned char EscapedFormsForJSON[0x100] = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'b', 't', 'n', 'u', 'f', 'r', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    0,   0,  '\"', 0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,  '\\', 0,   0,   0,
};

char LowerNibbleToLowercaseHex(unsigned value) {
    return "0123456789abcdef"[value & 15];
}

template<typename OutputCharacterType, typename InputCharacterType>
void AppendQuotedJSONStringInternal(OutputCharacterType*& output, const InputCharacterType* input, unsigned length) {
    for (auto* end = input + length; input != end; ++input) {
        unsigned character = *input;
        if (character <= 0xFF) {
            auto escaped = EscapedFormsForJSON[character];
            if (!escaped) {
                *output++ = static_cast<OutputCharacterType>(character);
                continue;
            }
            *output++ = '\\';
            *output++ = escaped;
            if (escaped == 'u') {
                *output++ = '0';
                *output++ = '0';
                *output++ = LowerNibbleToLowercaseHex(character >> 4);
                *output++ = LowerNibbleToLowercaseHex(character);
            }
            continue;
        }
        if (character < 0xD800 || character > 0xDFFF) {
            *output++ = static_cast<OutputCharacterType>(character);
            continue;
        }
        auto next = input + 1;
        if (character <= 0xDBFF && next != end && *next >= 0xDC00 && *next <= 0xDFFF) {
            *output++ = static_cast<OutputCharacterType>(character);
            *output++ = static_cast<OutputCharacterType>(*next);
            ++input;
            continue;
        }
        unsigned upper = character >> 8;
        unsigned lower = character & 0xFF;
        *output++ = '\\';
        *output++ = 'u';
        *output++ = LowerNibbleToLowercaseHex(upper >> 4);
        *output++ = LowerNibbleToLowercaseHex(upper);
        *output++ = LowerNibbleToLowercaseHex(lower >> 4);
        *output++ = LowerNibbleToLowercaseHex(lower);
    }
}

unsigned RoundUpToPowerOfTwo(unsigned value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(StringBuilder* self, const String& string) {
    const char* const functionName = __func__;
    if (!self) throw std::invalid_argument("null WTF::StringBuilder");
    RequireReadableGuestBytes(self, sizeof(StringBuilder), functionName);
    RequireWritableGuestBytes(self, sizeof(StringBuilder), functionName);

    unsigned char rawBuilder[sizeof(StringBuilder)]{};
    if (!GuestCopyIn_nid_no_patch(rawBuilder, self, sizeof(rawBuilder)))
        NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);

    const StringImpl* guestImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&guestImpl, &string, sizeof(guestImpl)))
        NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);

    BorrowedStringBuilder local(rawBuilder);
    auto& builder = local.value;
    if (builder.overflowed) return;

    unsigned stringLength = 0;
    bool stringIs8Bit = true;
    const void* guestStringData = nullptr;
    std::size_t guestStringBytes = 0;
    if (guestImpl) {
        const StringImpl implHeader = ReadGuestStringImpl(guestImpl, functionName);
        stringLength = implHeader.length;
        stringIs8Bit = (implHeader.hashAndFlags & kStringImplFlag8Bit) != 0;
        if (stringLength) {
            if (!implHeader.data) throw std::invalid_argument("null WTF character buffer");
            const std::size_t unitSize = stringIs8Bit ? 1u : sizeof(char16_t);
            if (stringLength > std::numeric_limits<std::size_t>::max() / unitSize)
                NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
            guestStringBytes = static_cast<std::size_t>(stringLength) * unitSize;
            RequireReadableGuestBytes(implHeader.data, guestStringBytes, functionName);
            guestStringData = implHeader.data;
        }
    }

    const unsigned currentLength = static_cast<unsigned>(builder.length);
    const std::uint64_t maximumCapacityRequired = std::uint64_t(currentLength) + 2 + std::uint64_t(stringLength) * 6;
    const auto failOverflow = [&]() {
        const unsigned char overflowByte = 1;
        if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(StringBuilder, overflowed), &overflowByte, sizeof(overflowByte)))
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        if (builder.shouldCrashOnOverflow) throw std::length_error("WTF::StringBuilder overflowed");
    };
    if (maximumCapacityRequired > 0xFFFFFFFFULL) {
        failOverflow();
        return;
    }
    unsigned allocationSize = static_cast<unsigned>(maximumCapacityRequired);
    allocationSize = std::max(allocationSize, RoundUpToPowerOfTwo(allocationSize));
    if (allocationSize > MaxLength) {
        failOverflow();
        return;
    }

    std::vector<unsigned char> copiedInput8;
    std::vector<char16_t> copiedInput16;
    if (stringLength) {
        if (stringIs8Bit) {
            copiedInput8.resize(stringLength);
            if (!GuestCopyIn_nid_no_patch(copiedInput8.data(), guestStringData, guestStringBytes))
                NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        } else {
            copiedInput16.resize(stringLength);
            if (!GuestCopyIn_nid_no_patch(copiedInput16.data(), guestStringData, guestStringBytes))
                NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        }
    }

    const bool upConvert = builder.is8Bit && !stringIs8Bit;
    const bool finalIs8Bit = builder.is8Bit && !upConvert;
    const std::size_t worstOutputUnits = 2u + static_cast<std::size_t>(stringLength) * 6u;
    std::vector<unsigned char> formatted8;
    std::vector<char16_t> formatted16;
    std::size_t formattedUnits = 0;
    if (finalIs8Bit) {
        formatted8.resize(worstOutputUnits);
        auto* output = formatted8.data();
        *output++ = '"';
        AppendQuotedJSONStringInternal(output, stringLength ? copiedInput8.data() : nullptr, stringLength);
        *output++ = '"';
        formattedUnits = static_cast<std::size_t>(output - formatted8.data());
    } else {
        formatted16.resize(worstOutputUnits);
        auto* output = formatted16.data();
        *output++ = '"';
        if (stringIs8Bit) AppendQuotedJSONStringInternal(output, stringLength ? copiedInput8.data() : nullptr, stringLength);
        else AppendQuotedJSONStringInternal(output, stringLength ? copiedInput16.data() : nullptr, stringLength);
        *output++ = '"';
        formattedUnits = static_cast<std::size_t>(output - formatted16.data());
    }

    const auto publishBuilder = [&]() {
        unsigned char updatedRaw[sizeof(StringBuilder)]{};
        std::memcpy(updatedRaw, rawBuilder, sizeof(updatedRaw));
        PackStringBuilder(builder, updatedRaw);
        if (!GuestCopyOut_nid_no_patch(self, updatedRaw, sizeof(updatedRaw)))
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
    };

    const auto writeFormattedIntoBuffer = [&](const StringImpl& targetBufferHeader, bool updateLengthOnly) {
        const bool targetIs8Bit = (targetBufferHeader.hashAndFlags & kStringImplFlag8Bit) != 0;
        if (targetIs8Bit != builder.is8Bit || targetIs8Bit != finalIs8Bit)
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        if (currentLength > targetBufferHeader.length || formattedUnits > targetBufferHeader.length - currentLength)
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        const std::size_t elemSize = finalIs8Bit ? 1u : sizeof(char16_t);
        void* rawChars = finalIs8Bit ? static_cast<void*>(builder.characters8) : static_cast<void*>(builder.characters16);
        const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(rawChars);
        const std::size_t byteOffset = static_cast<std::size_t>(currentLength) * elemSize;
        const std::size_t writeBytes = formattedUnits * elemSize;
        if (!baseAddress || byteOffset > std::numeric_limits<std::uintptr_t>::max() - baseAddress
            || writeBytes > std::numeric_limits<std::uintptr_t>::max() - (baseAddress + byteOffset))
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        void* destination = reinterpret_cast<void*>(baseAddress + byteOffset);
        RequireWritableGuestBytes(destination, writeBytes, functionName);
        const void* source = finalIs8Bit ? static_cast<const void*>(formatted8.data()) : static_cast<const void*>(formatted16.data());
        if (!GuestCopyOut_nid_no_patch(destination, source, writeBytes))
            NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        builder.length = static_cast<std::int32_t>(currentLength + formattedUnits);
        if (updateLengthOnly) {
            if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(StringBuilder, length), &builder.length, sizeof(builder.length)))
                NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
        } else {
            publishBuilder();
        }
    };

    if (upConvert) {
        if (currentLength && builder.characters8) RequireReadableGuestBytes(builder.characters8, currentLength, functionName);
        ValidateReleasableStrings(builder.buffer, builder.string.impl, functionName);
        NKWebKit::WtfAbi::AllocateBufferUpConvert(builder, builder.characters8, allocationSize);
    } else if (builder.buffer) {
        const StringImpl bufferHeader = ReadGuestStringImpl(builder.buffer, functionName);
        if (allocationSize > bufferHeader.length) {
            ValidateReleasableStrings(builder.buffer, builder.string.impl, functionName);
            const bool bufferIs8Bit = (bufferHeader.hashAndFlags & kStringImplFlag8Bit) != 0;
            const std::size_t bufferUnitSize = bufferIs8Bit ? 1u : sizeof(char16_t);
            const std::size_t copyUnits = std::max<std::size_t>(bufferHeader.length, currentLength);
            if (copyUnits) {
                if (copyUnits > std::numeric_limits<std::size_t>::max() / bufferUnitSize)
                    NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
                RequireReadableGuestBytes(bufferHeader.data, copyUnits * bufferUnitSize, functionName);
            }
            NKWebKit::WtfAbi::ReserveCapacity(builder, allocationSize);
        } else {
            writeFormattedIntoBuffer(bufferHeader, true);
            return;
        }
    } else if (allocationSize > currentLength) {
        if (currentLength) {
            if (!builder.string.impl) NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
            const StringImpl existingHeader = ReadGuestStringImpl(builder.string.impl, functionName);
            ValidateReleasableStrings(builder.string.impl, nullptr, functionName);
            const bool existingIs8Bit = (existingHeader.hashAndFlags & kStringImplFlag8Bit) != 0;
            const std::size_t existingUnitSize = existingIs8Bit ? 1u : sizeof(char16_t);
            if (currentLength > std::numeric_limits<std::size_t>::max() / existingUnitSize)
                NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
            RequireReadableGuestBytes(existingHeader.data, static_cast<std::size_t>(currentLength) * existingUnitSize, functionName);
        } else if (builder.string.impl) {
            ValidateReleasableStrings(builder.string.impl, nullptr, functionName);
        }
        NKWebKit::WtfAbi::ReserveCapacity(builder, allocationSize);
    } else {
        NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
    }

    if (builder.overflowed) {
        publishBuilder();
        return;
    }

    if (!builder.buffer) NotImplementedAt_nid_no_patch(functionName, __FILE__, __LINE__);
    const StringImpl allocatedBufferHeader = ReadGuestStringImpl(builder.buffer, functionName);
    writeFormattedIntoBuffer(allocatedBufferHeader, false);
}

}
