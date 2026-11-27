// Derived from Source/JavaScriptCore/runtime/BytecodeCacheError.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/parser/ParserError.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#include "prx/libSceNKWebKit/include/JscBytecodeCacheErrorExports.hpp"

#include <cstdint>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <type_traits>
#include <unordered_set>

namespace {

using NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError;
using NKWebKit::JscBytecodeCacheErrorAbi::StandardError;
using NKWebKit::JscBytecodeCacheErrorAbi::WriteError;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<BytecodeCacheError>);
static_assert(std::is_trivially_copyable_v<StandardError>);
static_assert(std::is_trivially_copyable_v<WriteError>);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr std::uint32_t ParserErrorIndex = 0;
constexpr std::uint32_t StandardErrorIndex = 1;
constexpr std::uint32_t WriteErrorIndex = 2;
constexpr std::uint32_t ValuelessIndex = 0xFF;
constexpr std::uint8_t ErrorNone = 0;
constexpr unsigned kMaxSubstringDepth = 64u;

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

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
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

std::uint32_t Alternative(const BytecodeCacheError& error) {
    std::uint32_t index = error.index & 0xFF;
    if (index > WriteErrorIndex && index != ValuelessIndex) throw std::invalid_argument("invalid JSC::BytecodeCacheError variant index");
    return index;
}

void ValidateReleasableStringImpl(StringImpl* impl, const char* function) {
    std::unordered_set<const StringImpl*> visited;
    unsigned depth = 0;
    while (impl) {
        if (depth >= kMaxSubstringDepth || !visited.insert(impl).second)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(impl, sizeof(StringImpl), function);
        RequireWritableGuestBytes(impl, sizeof(StringImpl), function);
        const StringImpl header = ReadGuestValue(impl, function);
        if (header.refCount != 2) return;
        if ((header.hashAndFlags & 48u) || (header.hashAndFlags & 3u) == 3u) return;
        if ((header.hashAndFlags & 3u) != 2u) return;
        StringImpl* owner = nullptr;
        if (!GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(impl) + 24, sizeof(owner)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        impl = owner;
        ++depth;
    }
}

void ReleaseChainWithHostWrite(StringImpl* root, StringImpl* current, unsigned depth) noexcept {
    if (!current || depth >= kMaxSubstringDepth) {
        try {
            NKWebKit::WtfAbi::Release(root);
        } catch (...) {
        }
        return;
    }
    try {
        const GuestArena::HostWrite guard(current, sizeof(StringImpl));
        if (!guard.Open() || !GuestWritable_nid_no_patch(current, sizeof(StringImpl))) return;
        StringImpl header{};
        if (!GuestCopyIn_nid_no_patch(&header, current, sizeof(header))) return;
        if (header.refCount == 2 && !(header.hashAndFlags & 48u) && (header.hashAndFlags & 3u) == 2u) {
            StringImpl* owner = nullptr;
            if (GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(current) + 24, sizeof(owner)) && owner) {
                ReleaseChainWithHostWrite(root, owner, depth + 1);
                return;
            }
        }
        NKWebKit::WtfAbi::Release(root);
    } catch (...) {
    }
}

void ReleaseStringImplNoThrow(StringImpl* impl) noexcept {
    if (!impl) return;
    ReleaseChainWithHostWrite(impl, impl, 0);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK3JSC18BytecodeCacheError7isValidEv(const BytecodeCacheError* self) {
    if (!self) throw std::invalid_argument("null JSC::BytecodeCacheError");
    const BytecodeCacheError error = ReadGuestValue(self, __func__);
    switch (Alternative(error)) {
    case ParserErrorIndex:
        return error.parserError.type != ErrorNone;
    case StandardErrorIndex:
    case WriteErrorIndex:
        return true;
    }
    throw std::invalid_argument("valueless JSC::BytecodeCacheError");
}

BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(BytecodeCacheError* self, const StandardError* error) {
    if (!self) throw std::invalid_argument("null JSC::BytecodeCacheError");
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError::StandardError");
    RequireReadableGuestBytes(self, sizeof(BytecodeCacheError), __func__);
    RequireWritableGuestBytes(self, sizeof(BytecodeCacheError), __func__);
    const BytecodeCacheError previous = ReadGuestValue(self, __func__);
    const StandardError value = ReadGuestValue(error, __func__);
    const std::uint32_t index = Alternative(previous);
    StringImpl* const previousMessage = (index == ParserErrorIndex) ? previous.parserError.message : nullptr;
    if (previousMessage) ValidateReleasableStringImpl(previousMessage, __func__);
    BytecodeCacheError updated = previous;
    if (index != StandardErrorIndex) updated.index = StandardErrorIndex;
    updated.standardError = value;
    if (!TryPublishGuestValue(self, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (previousMessage) ReleaseStringImplNoThrow(previousMessage);
    return self;
}

BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(BytecodeCacheError* self, const WriteError* error) {
    if (!self) throw std::invalid_argument("null JSC::BytecodeCacheError");
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError::WriteError");
    RequireReadableGuestBytes(self, sizeof(BytecodeCacheError), __func__);
    RequireWritableGuestBytes(self, sizeof(BytecodeCacheError), __func__);
    const BytecodeCacheError previous = ReadGuestValue(self, __func__);
    const WriteError value = ReadGuestValue(error, __func__);
    const std::uint32_t index = Alternative(previous);
    StringImpl* const previousMessage = (index == ParserErrorIndex) ? previous.parserError.message : nullptr;
    if (previousMessage) ValidateReleasableStringImpl(previousMessage, __func__);
    BytecodeCacheError updated = previous;
    if (index != WriteErrorIndex) updated.index = WriteErrorIndex;
    updated.writeError = value;
    if (!TryPublishGuestValue(self, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (previousMessage) ReleaseStringImplNoThrow(previousMessage);
    return self;
}

}
