/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Source/WTF/wtf/text/StringBuilder.cpp and Source/WTF/wtf/text/StringBuilder.h (WebKit c55364d53f97)

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include "WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned MaxCapacity = 0x7FFFFFFF;

StringBuilder& Checked(StringBuilder* builder) {
    if (!builder) throw std::invalid_argument("null WTF::StringBuilder");
    return *builder;
}

bool Is8Bit(const StringImpl* impl) {
    return !impl || (impl->hashAndFlags & 4);
}

const unsigned char* Characters8(const StringImpl* impl) {
    return impl ? static_cast<const unsigned char*>(impl->data) : nullptr;
}

const char16_t* Characters16(const StringImpl* impl) {
    return impl ? static_cast<const char16_t*>(impl->data) : nullptr;
}

bool HasOneRef(const StringImpl* impl) {
    return (impl->refCount & ~1U) == 2;
}

unsigned Capacity(const StringBuilder& builder) {
    return builder.buffer ? builder.buffer->length : static_cast<unsigned>(builder.length);
}

unsigned ExpandedCapacity(unsigned capacity, unsigned requiredLength) {
    constexpr unsigned minimumCapacity = 16;
    return std::max(requiredLength, std::max(minimumCapacity, std::min(capacity * 2, MaxCapacity)));
}

void SetBuffer(StringBuilder& builder, StringImpl* buffer) {
    NKWebKit::WtfAbi::Release(std::exchange(builder.buffer, buffer));
}

void AllocateBuffer(StringBuilder& builder, const unsigned char* characters, unsigned requiredLength) {
    if (!builder.is8Bit) throw std::logic_error("8-bit WTF::StringBuilder buffer for 16-bit contents");
    void* output;
    auto* buffer = NKWebKit::WtfAbi::TryCreateUninitialized(requiredLength, true, output);
    builder.characters8 = static_cast<unsigned char*>(output);
    if (!buffer) return NKWebKit::WtfAbi::DidOverflow(builder);
    if (builder.length) std::memcpy(builder.characters8, characters, static_cast<unsigned>(builder.length));
    SetBuffer(builder, buffer);
    builder.string = String();
}

void AllocateBuffer(StringBuilder& builder, const char16_t* characters, unsigned requiredLength) {
    if (builder.is8Bit) throw std::logic_error("16-bit WTF::StringBuilder buffer for 8-bit contents");
    void* output;
    auto* buffer = NKWebKit::WtfAbi::TryCreateUninitialized(requiredLength, false, output);
    builder.characters16 = static_cast<char16_t*>(output);
    if (!buffer) return NKWebKit::WtfAbi::DidOverflow(builder);
    if (builder.length) std::memcpy(builder.characters16, characters, static_cast<std::size_t>(builder.length) * sizeof(char16_t));
    SetBuffer(builder, buffer);
    builder.string = String();
}

template<typename CharacterType> void Reallocate(StringBuilder& builder, unsigned requiredLength) {
    constexpr bool is8Bit = sizeof(CharacterType) == 1;
    auto* original = std::exchange(builder.buffer, nullptr);
    if (!requiredLength) {
        NKWebKit::WtfAbi::Release(original);
        builder.buffer = NKWebKit::WtfAbi::EmptyStringImpl();
        builder.characters8 = nullptr;
        return;
    }
    void* output;
    auto* buffer = NKWebKit::WtfAbi::TryCreateUninitialized(requiredLength, is8Bit, output);
    if (!buffer) {
        NKWebKit::WtfAbi::Release(original);
        return NKWebKit::WtfAbi::DidOverflow(builder);
    }
    std::memcpy(output, original->data, std::size_t(std::min(original->length, requiredLength)) * sizeof(CharacterType));
    NKWebKit::WtfAbi::Release(original);
    builder.buffer = buffer;
    if constexpr (is8Bit) builder.characters8 = static_cast<unsigned char*>(output);
    else builder.characters16 = static_cast<char16_t*>(output);
}

template<typename CharacterType> void ReallocateBuffer(StringBuilder& builder, unsigned requiredLength) {
    builder.string = String();
    if constexpr (sizeof(CharacterType) == 1) {
        if (HasOneRef(builder.buffer)) Reallocate<unsigned char>(builder, requiredLength);
        else AllocateBuffer(builder, Characters8(builder.buffer), requiredLength);
    } else {
        if (Is8Bit(builder.buffer)) NKWebKit::WtfAbi::AllocateBufferUpConvert(builder, Characters8(builder.buffer), requiredLength);
        else if (HasOneRef(builder.buffer)) Reallocate<char16_t>(builder, requiredLength);
        else AllocateBuffer(builder, Characters16(builder.buffer), requiredLength);
    }
}

template<typename CharacterType> CharacterType* BufferCharacters(StringBuilder& builder) {
    if constexpr (sizeof(CharacterType) == 1) return builder.characters8;
    else return builder.characters16;
}

template<typename CharacterType> const CharacterType* StringCharacters(const StringBuilder& builder) {
    if constexpr (sizeof(CharacterType) == 1) return Characters8(builder.string.impl);
    else return Characters16(builder.string.impl);
}

template<typename CharacterType> CharacterType* ExtendBufferSlowCase(StringBuilder& builder, unsigned requiredLength) {
    if (builder.buffer) ReallocateBuffer<CharacterType>(builder, ExpandedCapacity(Capacity(builder), requiredLength));
    else AllocateBuffer(builder, builder.length ? StringCharacters<CharacterType>(builder) : nullptr, ExpandedCapacity(Capacity(builder), requiredLength));
    if (builder.overflowed) return nullptr;
    auto* result = BufferCharacters<CharacterType>(builder) + builder.length;
    builder.length = static_cast<std::int32_t>(requiredLength);
    return result;
}

template<typename CharacterType> CharacterType* ExtendBufferWithoutOverflowCheck(StringBuilder& builder, unsigned requiredLength) {
    if (builder.buffer && requiredLength <= builder.buffer->length) {
        unsigned currentLength = static_cast<unsigned>(builder.length);
        builder.string = String();
        builder.length = static_cast<std::int32_t>(requiredLength);
        return BufferCharacters<CharacterType>(builder) + currentLength;
    }
    return ExtendBufferSlowCase<CharacterType>(builder, requiredLength);
}

std::uint64_t RequiredLength(const StringBuilder& builder, unsigned additionalLength) {
    std::uint64_t requiredLength = std::uint64_t(static_cast<unsigned>(builder.length)) + additionalLength;
    if (requiredLength > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF::StringBuilder length overflow");
    return requiredLength;
}

template<typename CharacterType> CharacterType* ExtendBufferForAppending(StringBuilder& builder, unsigned additionalLength) {
    auto requiredLength = RequiredLength(builder, additionalLength);
    if (requiredLength > MaxCapacity) {
        NKWebKit::WtfAbi::DidOverflow(builder);
        return nullptr;
    }
    return ExtendBufferWithoutOverflowCheck<CharacterType>(builder, static_cast<unsigned>(requiredLength));
}

unsigned char* ExtendBufferForAppending8(StringBuilder& builder, std::uint64_t requiredLength) {
    if (requiredLength > MaxCapacity) {
        NKWebKit::WtfAbi::DidOverflow(builder);
        return nullptr;
    }
    return ExtendBufferWithoutOverflowCheck<unsigned char>(builder, static_cast<unsigned>(requiredLength));
}

char16_t* ExtendBufferForAppending16(StringBuilder& builder, std::uint64_t requiredLength) {
    if (requiredLength > MaxCapacity) {
        NKWebKit::WtfAbi::DidOverflow(builder);
        return nullptr;
    }
    if (builder.is8Bit) {
        const unsigned char* characters = builder.buffer ? Characters8(builder.buffer) : Characters8(builder.string.impl);
        NKWebKit::WtfAbi::AllocateBufferUpConvert(builder, characters, ExpandedCapacity(Capacity(builder), static_cast<unsigned>(requiredLength)));
        if (builder.overflowed) return nullptr;
        unsigned oldLength = static_cast<unsigned>(builder.length);
        builder.length = static_cast<std::int32_t>(requiredLength);
        return builder.characters16 + oldLength;
    }
    return ExtendBufferWithoutOverflowCheck<char16_t>(builder, static_cast<unsigned>(requiredLength));
}

bool CanShrink(const StringBuilder& builder) {
    if (builder.overflowed) return false;
    unsigned length = static_cast<unsigned>(builder.length);
    return builder.buffer && builder.buffer->length > length + (length >> 2);
}

void ReifyString(const StringBuilder& builder) {
    if (builder.string.impl) return;
    if (!builder.length) {
        builder.string = String(NKWebKit::WtfAbi::EmptyStringImpl());
        return;
    }
    if (!builder.buffer) throw std::logic_error("WTF::StringBuilder has contents but no buffer");
    if (static_cast<unsigned>(builder.length) == builder.buffer->length) {
        builder.buffer->refCount += 2;
        builder.string = String(builder.buffer);
    } else {
        builder.buffer->refCount += 2;
        String buffer(builder.buffer);
        builder.string = NKWebKit::WtfAbi::SubstringSharingImpl(buffer, 0, static_cast<unsigned>(builder.length));
    }
}

void ShrinkToFit(StringBuilder& builder) {
    if (!CanShrink(builder)) return;
    if (builder.is8Bit) ReallocateBuffer<unsigned char>(builder, static_cast<unsigned>(builder.length));
    else ReallocateBuffer<char16_t>(builder, static_cast<unsigned>(builder.length));
    builder.string = String(std::exchange(builder.buffer, nullptr));
}

void AppendCharacters(StringBuilder& builder, const unsigned char* characters, unsigned length) {
    if (!length || builder.overflowed) return;
    if (!characters) throw std::invalid_argument("null WTF::StringBuilder characters");
    if (builder.is8Bit) {
        auto* destination = ExtendBufferForAppending<unsigned char>(builder, length);
        if (!destination) return;
        std::memcpy(destination, characters, length);
    } else {
        auto* destination = ExtendBufferForAppending<char16_t>(builder, length);
        if (!destination) return;
        std::copy(characters, characters + length, destination);
    }
}

void AppendCharacters(StringBuilder& builder, const char16_t* characters, unsigned length) {
    if (!length || builder.overflowed) return;
    if (!characters) throw std::invalid_argument("null WTF::StringBuilder characters");
    if (builder.is8Bit && length == 1 && characters[0] <= 0xFF) {
        NKWebKit::WtfAbi::AppendCharacter(builder, static_cast<unsigned char>(characters[0]));
        return;
    }
    auto* destination = ExtendBufferForAppending16(builder, RequiredLength(builder, length));
    if (!destination) return;
    std::memcpy(destination, characters, static_cast<std::size_t>(length) * sizeof(char16_t));
}

}  // namespace

namespace NKWebKit::WtfAbi {

void DidOverflow(StringBuilder& builder) {
    builder.overflowed = true;
    if (builder.shouldCrashOnOverflow) throw std::length_error("WTF::StringBuilder overflowed");
}

void AllocateBufferUpConvert(StringBuilder& builder, const unsigned char* characters, unsigned requiredLength) {
    unsigned length = static_cast<unsigned>(builder.length);
    if (length && !characters) throw std::logic_error("WTF::StringBuilder has contents but no 8-bit characters");
    void* output;
    auto* buffer = TryCreateUninitialized(requiredLength, false, output);
    builder.characters16 = static_cast<char16_t*>(output);
    if (!buffer) return DidOverflow(builder);
    for (unsigned i = 0; i < length; ++i) builder.characters16[i] = characters[i];
    builder.is8Bit = false;
    SetBuffer(builder, buffer);
    builder.string = String();
}

void AppendCharacter(StringBuilder& builder, unsigned char character) {
    if (builder.overflowed) return;
    unsigned length = static_cast<unsigned>(builder.length);
    if (builder.buffer && length < builder.buffer->length && !builder.string.impl) {
        if (builder.is8Bit) builder.characters8[length] = character;
        else builder.characters16[length] = character;
        ++builder.length;
    } else {
        AppendCharacters(builder, &character, 1);
    }
}

void AppendString(StringBuilder& builder, const StringImpl* string) {
    if (builder.overflowed || !string || !string->length) return;
    if (!builder.length && !builder.buffer) {
        const_cast<StringImpl*>(string)->refCount += 2;
        builder.string = String(const_cast<StringImpl*>(string));
        builder.length = static_cast<std::int32_t>(string->length);
        builder.is8Bit = Is8Bit(string);
        return;
    }
    if (Is8Bit(string)) AppendCharacters(builder, Characters8(string), string->length);
    else AppendCharacters(builder, Characters16(string), string->length);
}

void AppendCString(StringBuilder& builder, const char* characters) {
    if (characters) AppendCharacters(builder, reinterpret_cast<const unsigned char*>(characters), static_cast<unsigned>(std::strlen(characters)));
}

void AppendAdapted(StringBuilder& builder, const char* characters, unsigned length) {
    std::uint64_t requiredLength = builder.overflowed ? std::uint64_t(MaxCapacity) + 1 : std::uint64_t(static_cast<unsigned>(builder.length)) + length;
    if (builder.is8Bit) {
        auto* destination = ExtendBufferForAppending8(builder, requiredLength);
        if (destination) std::memcpy(destination, characters, length);
    } else {
        auto* destination = ExtendBufferForAppending16(builder, requiredLength);
        if (destination) std::copy(characters, characters + length, destination);
    }
}

String ToString(StringBuilder& builder) {
    if (builder.string.impl) return builder.string;
    if (builder.overflowed) throw std::length_error("WTF::StringBuilder overflowed");
    ShrinkToFit(builder);
    ReifyString(builder);
    return builder.string;
}

void ReserveCapacity(StringBuilder& builder, unsigned newCapacity) {
    if (builder.overflowed) return;
    if (builder.buffer) {
        if (newCapacity > builder.buffer->length) {
            if (Is8Bit(builder.buffer)) ReallocateBuffer<unsigned char>(builder, newCapacity);
            else ReallocateBuffer<char16_t>(builder, newCapacity);
        }
        return;
    }
    unsigned length = static_cast<unsigned>(builder.length);
    if (newCapacity <= length) return;
    if (!length) AllocateBuffer(builder, static_cast<const unsigned char*>(nullptr), newCapacity);
    else if (Is8Bit(builder.string.impl)) AllocateBuffer(builder, Characters8(builder.string.impl), newCapacity);
    else AllocateBuffer(builder, Characters16(builder.string.impl), newCapacity);
}

void Clear(StringBuilder& builder) {
    builder.overflowed = false;
    builder.length = 0;
    builder.string = String();
    SetBuffer(builder, nullptr);
    builder.characters8 = nullptr;
    builder.is8Bit = true;
}

}  // namespace NKWebKit::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF13StringBuilder16appendCharactersEPKDsj(StringBuilder* self, const char16_t* characters, unsigned length) {
    AppendCharacters(Checked(self), characters, length);
}

void APS5_VABI _ZN3WTF13StringBuilder16appendCharactersEPKhj(StringBuilder* self, const unsigned char* characters, unsigned length) {
    AppendCharacters(Checked(self), characters, length);
}

void APS5_VABI _ZN3WTF13StringBuilder15reserveCapacityEj(StringBuilder* self, unsigned newCapacity) {
    NKWebKit::WtfAbi::ReserveCapacity(Checked(self), newCapacity);
}

void APS5_VABI _ZN3WTF13StringBuilder11shrinkToFitEv(StringBuilder* self) {
    ShrinkToFit(Checked(self));
}

void APS5_VABI _ZNK3WTF13StringBuilder11reifyStringEv(const StringBuilder* self) {
    ReifyString(Checked(const_cast<StringBuilder*>(self)));
}

void APS5_VABI _ZN3WTF13StringBuilder12appendNumberEf(StringBuilder* self, float number) {
    auto& builder = Checked(self);
    auto text = NKWebKit::WtfAbi::NumberToString(number);
    NKWebKit::WtfAbi::AppendCString(builder, text.c_str());
}

}
