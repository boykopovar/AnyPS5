/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/platform/Cursor.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/CursorExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace NKWebKit::WebCoreAbi {

namespace {

template <std::size_t... Index>
constexpr std::array<Cursor, StandardCursorCount> MakeStandardCursors(std::index_sequence<Index...>) {
    return {Cursor{static_cast<CursorType>(Index)}...};
}

constinit std::array<Cursor, StandardCursorCount> standardCursors =
    MakeStandardCursors(std::make_index_sequence<StandardCursorCount>{});

}

const Cursor& StandardCursor(CursorType type) {
    return standardCursors[static_cast<std::uint32_t>(type)];
}

}

using NKWebKit::WebCoreAbi::Cursor;
using NKWebKit::WebCoreAbi::CursorType;
using NKWebKit::WebCoreAbi::CursorTypeValueCount;
using NKWebKit::WebCoreAbi::ImageOrientation;
using NKWebKit::WebCoreAbi::OptionalIntPoint;
using NKWebKit::WebCoreAbi::StandardCursor;
using NKWebKit::WebCoreAbi::StandardCursorCount;

namespace {

constexpr unsigned ImageSizeSlot = 19;
constexpr unsigned ImageHotSpotSlot = 22;

struct GuestImageHeader {
    const void* vtable;
    std::atomic<std::uint32_t> refCount;
};
static_assert(offsetof(GuestImageHeader, refCount) == 8, "guest Image m_refCount sits right after the vtable pointer");
static_assert(sizeof(std::atomic<std::uint32_t>) == sizeof(std::uint32_t) && std::atomic<std::uint32_t>::is_always_lock_free);

using ImageSizeFunction = WebCore::FloatSize (APS5_VABI*)(const void* self, ImageOrientation orientation);
using ImageHotSpotFunction = OptionalIntPoint (APS5_VABI*)(const void* self);

const void* const* GuestImageVtable(const void* image, const char* caller) {
    const void* const* vtable = *reinterpret_cast<void* const* const*>(image);
    if (!vtable) throw std::invalid_argument(std::string(caller) + ": image vtable is null");
    return vtable;
}

WebCore::FloatSize GuestImageSize(const void* image, const char* caller) {
    const void* const* vtable = GuestImageVtable(image, caller);
    const void* entry = vtable[ImageSizeSlot];
    if (!entry) throw std::invalid_argument(std::string(caller) + ": image size slot is null");
    return reinterpret_cast<ImageSizeFunction>(const_cast<void*>(entry))(image, ImageOrientation{0});
}

OptionalIntPoint GuestImageHotSpot(const void* image, const char* caller) {
    const void* const* vtable = GuestImageVtable(image, caller);
    const void* entry = vtable[ImageHotSpotSlot];
    if (!entry) throw std::invalid_argument(std::string(caller) + ": image hot spot slot is null");
    return reinterpret_cast<ImageHotSpotFunction>(const_cast<void*>(entry))(image);
}

bool Contains(const WebCore::IntRect& rect, const WebCore::IntPoint& point) {
    return point.m_x >= rect.m_location.m_x && point.m_x < rect.m_location.m_x + rect.m_size.m_width
        && point.m_y >= rect.m_location.m_y && point.m_y < rect.m_location.m_y + rect.m_size.m_height;
}

WebCore::IntPoint DetermineHotSpot(const void* image, const WebCore::IntPoint& specifiedHotSpot, const char* caller) {
    const WebCore::FloatSize nullCheckSize = GuestImageSize(image, caller);
    if (nullCheckSize.m_width <= 0.0f || nullCheckSize.m_height <= 0.0f) return {};
    WebCore::IntRect imageRect;
    _ZN7WebCore7IntRectC1ERKNS_9FloatRectE(&imageRect, WebCore::FloatRect{{}, GuestImageSize(image, caller)});
    if (Contains(imageRect, specifiedHotSpot)) return specifiedHotSpot;
    const OptionalIntPoint intrinsicHotSpot = GuestImageHotSpot(image, caller);
    if (intrinsicHotSpot.hasValue && Contains(imageRect, intrinsicHotSpot.value)) return intrinsicHotSpot.value;
    return {};
}

void ConstructCursor(Cursor* self, void* image, const WebCore::IntPoint* hotSpot, const char* caller) {
    if (!self) throw std::invalid_argument(std::string(caller) + ": null this");
    if (!image) throw std::invalid_argument(std::string(caller) + ": null image");
    if (!hotSpot) throw std::invalid_argument(std::string(caller) + ": null hot spot");
    std::atomic<std::uint32_t>& refCount = static_cast<GuestImageHeader*>(image)->refCount;
    refCount.fetch_add(1, std::memory_order_relaxed);
    WebCore::IntPoint determined;
    try {
        determined = DetermineHotSpot(image, *hotSpot, caller);
    } catch (...) {
        refCount.fetch_sub(1, std::memory_order_relaxed);
        throw;
    }
    new (self) Cursor{CursorType::Custom, image, determined, nullptr};
}

}

extern "C" {

const Cursor& APS5_VABI _ZN7WebCore10handCursorEv() {
    return StandardCursor(CursorType::Hand);
}

const Cursor& APS5_VABI _ZN7WebCore11iBeamCursorEv() {
    return StandardCursor(CursorType::IBeam);
}

const Cursor& APS5_VABI _ZN7WebCore6Cursor8fromTypeENS0_4TypeE(CursorType type) {
    const auto value = static_cast<std::uint32_t>(type);
    if (value >= CursorTypeValueCount) {
        throw std::out_of_range("WebCore::Cursor::fromType: value outside the Cursor::Type range");
    }
    if (value >= StandardCursorCount) {
        return StandardCursor(CursorType::Pointer);
    }
    return StandardCursor(type);
}

void APS5_VABI _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(Cursor* self, void* image, const WebCore::IntPoint* hotSpot) {
    ConstructCursor(self, image, hotSpot, "WebCore::Cursor::Cursor");
}

void APS5_VABI _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(Cursor* self, void* image, const WebCore::IntPoint* hotSpot) {
    ConstructCursor(self, image, hotSpot, "WebCore::Cursor::Cursor");
}

void APS5_VABI _ZNK7WebCore6Cursor19setAsPlatformCursorEv(const Cursor*) {
}

}
