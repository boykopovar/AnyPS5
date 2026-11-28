/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
 * Derived from Source/JavaScriptCore/API/OpaqueJSString.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/JscExports.hpp"

#include <memory>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::OpaqueJSStringPtr;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr unsigned kStringImplBufferOwnershipMask = 3u;
constexpr unsigned kStringImplBufferSubstring = 2u;
constexpr unsigned kStringImplBufferOwned = 3u;
constexpr unsigned kStringImplTerminatingFlagsMask = 48u;
constexpr std::size_t kSubstringHeaderBytes = 24u + sizeof(StringImpl*);

struct PlannedRelease {
    const StringImpl* impl{nullptr};
    unsigned refCount{0};
    unsigned hashAndFlags{0};
    bool destroyed{false};
};

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

StringImpl ReadGuestStringImpl(const StringImpl* pointer, const char* function) {
    RequireReadableGuestBytes(pointer, sizeof(StringImpl), function);
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, pointer, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return header;
}

void ValidateReleasableString(StringImpl* impl, const char* function, std::vector<PlannedRelease>& planned, std::deque<GuestArena::HostWrite>& writeGuards, unsigned depth = 0) {
    if (!impl) return;
    if (depth >= 64u) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::size_t index = planned.size();
    for (std::size_t i = 0; i < planned.size(); ++i) {
        if (planned[i].impl == impl) {
            index = i;
            break;
        }
    }
    if (index == planned.size()) {
        const StringImpl header = ReadGuestStringImpl(impl, function);
        auto& guard = writeGuards.emplace_back(impl, sizeof(StringImpl));
        if (!guard.Open() || !GuestWritable_nid_no_patch(impl, sizeof(StringImpl)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        planned.push_back({impl, header.refCount, header.hashAndFlags, false});
    }
    if (planned[index].destroyed) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (planned[index].refCount != 2u) {
        planned[index].refCount -= 2u;
        return;
    }
    const unsigned flags = planned[index].hashAndFlags;
    if ((flags & kStringImplTerminatingFlagsMask) != 0u || (flags & kStringImplBufferOwnershipMask) == kStringImplBufferOwned)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    planned[index].destroyed = true;
    if ((flags & kStringImplBufferOwnershipMask) == kStringImplBufferSubstring) {
        RequireReadableGuestBytes(impl, kSubstringHeaderBytes, function);
        StringImpl* owner = nullptr;
        if (!GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(impl) + 24, sizeof(owner)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (owner) ValidateReleasableString(owner, function, planned, writeGuards, depth + 1u);
    }
}

OpaqueJSStringPtr CreateFromImpl(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return OpaqueJSStringPtr(nullptr);
    const StringImpl impl = ReadGuestStringImpl(guestImpl, function);
    if (!GuestJSStringValidLength<char16_t>(impl.length))
        throw std::length_error("OpaqueJSString::tryCreate: length above the string limit");
    if (impl.length && !impl.data) throw std::invalid_argument("null WTF::String storage");
    auto result = std::make_unique<GuestJSString>();
    if (impl.length) {
        const bool is8Bit = (impl.hashAndFlags & kStringImplFlag8Bit) != 0;
        const std::size_t unitSize = is8Bit ? 1u : sizeof(std::uint16_t);
        if (impl.length > std::numeric_limits<std::size_t>::max() / unitSize)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t byteLength = static_cast<std::size_t>(impl.length) * unitSize;
        RequireReadableGuestBytes(impl.data, byteLength, function);
        if (is8Bit) {
            std::vector<std::uint8_t> raw(impl.length);
            if (!GuestCopyIn_nid_no_patch(raw.data(), impl.data, byteLength))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            result->characters.assign(raw.begin(), raw.end());
        } else {
            result->characters.resize(impl.length);
            if (!GuestCopyIn_nid_no_patch(result->characters.data(), impl.data, byteLength))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
    }
    result->characters.push_back(0);
    return OpaqueJSStringPtr(result.release());
}

}

extern "C" {

OpaqueJSStringPtr APS5_VABI _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    RequireReadableGuestBytes(string, sizeof(String), __func__);
    const StringImpl* guestImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&guestImpl, string, sizeof(guestImpl)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return CreateFromImpl(guestImpl, __func__);
}

OpaqueJSStringPtr APS5_VABI _ZN14OpaqueJSString9tryCreateEON3WTF6StringE(String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    RequireReadableGuestBytes(string, sizeof(String), __func__);
    const GuestArena::HostWrite stringWrite(string, sizeof(String));
    if (!stringWrite.Open() || !GuestWritable_nid_no_patch(string, sizeof(String)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    StringImpl* guestImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&guestImpl, string, sizeof(guestImpl)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    std::deque<GuestArena::HostWrite> implWrites;
    if (guestImpl) {
        std::vector<PlannedRelease> planned;
        ValidateReleasableString(guestImpl, __func__, planned, implWrites);
    }
    auto result = CreateFromImpl(guestImpl, __func__);
    StringImpl* const nullImpl = nullptr;
    if (!GuestCopyOut_nid_no_patch(string, &nullImpl, sizeof(nullImpl)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    NKWebKit::WtfAbi::Release(guestImpl);
    return result;
}

}
