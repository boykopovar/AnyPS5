// Derived from Source/WebCore/platform/graphics/displaylists/DisplayListItems.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/DisplayListDrawGlyphsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using namespace NKWebKit::DisplayListAbi;

template<typename T, std::size_t inlineCapacity> T* InlineStorage(InlineVector<T, inlineCapacity>& vector) {
    return reinterpret_cast<T*>(vector.inlineStorage);
}

template<typename T, std::size_t inlineCapacity>
void CheckInlineVector(const InlineVector<T, inlineCapacity>* vector, const char* function) {
    Checked(vector, function);
    const bool usesInlineStorage = vector->buffer == reinterpret_cast<const T*>(vector->inlineStorage);
    const bool validCapacity = usesInlineStorage ? vector->capacity == inlineCapacity : vector->capacity > inlineCapacity;
    if (!vector->buffer || !validCapacity || vector->size > vector->capacity)
        throw std::invalid_argument(std::string(function) + " requires a valid vector");
}

template<typename T, std::size_t inlineCapacity>
void MoveInlineVector(InlineVector<T, inlineCapacity>& to, InlineVector<T, inlineCapacity>& from) {
    if (from.buffer == InlineStorage(from)) {
        to.buffer = InlineStorage(to);
        to.capacity = inlineCapacity;
        to.size = from.size;
        if (from.size)
            std::memcpy(to.buffer, from.buffer, static_cast<std::size_t>(from.size) * sizeof(T));
        from.size = 0;
        return;
    }
    to.buffer = from.buffer;
    to.capacity = from.capacity;
    to.size = from.size;
    from.buffer = InlineStorage(from);
    from.capacity = inlineCapacity;
    from.size = 0;
}

template<typename T, std::size_t inlineCapacity> void ReleaseInlineVector(InlineVector<T, inlineCapacity>& vector) {
    if (vector.buffer && vector.buffer != InlineStorage(vector))
        _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = InlineStorage(vector);
    vector.capacity = inlineCapacity;
    vector.size = 0;
}

void APS5_VABI DrawGlyphsCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(static_cast<DrawGlyphs*>(self));
}

void APS5_VABI DrawGlyphsDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList10DrawGlyphsD0Ev(static_cast<DrawGlyphs*>(self));
}

OptionalFloatRect APS5_VABI DrawGlyphsLocalBounds(const void* self, const void*) {
    const auto& item = Checked(static_cast<const DrawGlyphs*>(self), "WebCore::DisplayList::DrawGlyphs::localBounds");
    WebCore::FloatRect bounds = item.bounds;
    bounds.m_location.m_x += item.blockLocation.m_x;
    bounds.m_location.m_y += item.blockLocation.m_y;
    return Bounds(bounds);
}

const DrawingItemVtable DrawGlyphsTable{
    {DrawGlyphsCompleteDestructor, DrawGlyphsDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawGlyphsLocalBounds, DefaultGlobalBounds};

void ConstructDrawGlyphs(DrawGlyphs* self, const WebCore::Font* font, GlyphVector* glyphs,
    GlyphAdvanceVector* advances, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor,
    int smoothingMode) {
    Checked(font, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    Checked(blockLocation, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    Checked(localAnchor, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    CheckInlineVector(glyphs, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    CheckInlineVector(advances, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    auto& item = Checked(self, "WebCore::DisplayList::DrawGlyphs::DrawGlyphs");
    ConstructDrawingItem(item.item, item.extent, &DrawGlyphsTable, ItemType::DrawGlyphs);
    item.font = font;
    MoveInlineVector(item.glyphs, *glyphs);
    MoveInlineVector(item.advances, *advances);
    item.bounds = WebCore::FloatRect();
    item.blockLocation = *blockLocation;
    item.localAnchor = *localAnchor;
    item.smoothingMode = static_cast<std::uint8_t>(smoothingMode);
}

void DestroyDrawGlyphs(DrawGlyphs* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawGlyphs::~DrawGlyphs");
    ReleaseInlineVector(item.glyphs);
    ReleaseInlineVector(item.advances);
    DestroyItem(item.item);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
    DrawGlyphs* self, const WebCore::Font* font, GlyphVector* glyphs, GlyphAdvanceVector* advances,
    const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, int smoothingMode) {
    ConstructDrawGlyphs(self, font, glyphs, advances, blockLocation, localAnchor, smoothingMode);
}

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsC2ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
    DrawGlyphs* self, const WebCore::Font* font, GlyphVector* glyphs, GlyphAdvanceVector* advances,
    const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, int smoothingMode) {
    ConstructDrawGlyphs(self, font, glyphs, advances, blockLocation, localAnchor, smoothingMode);
}

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD0Ev(DrawGlyphs* self) {
    DestroyDrawGlyphs(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(DrawGlyphs* self) {
    DestroyDrawGlyphs(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD2Ev(DrawGlyphs* self) {
    DestroyDrawGlyphs(self);
}

}
