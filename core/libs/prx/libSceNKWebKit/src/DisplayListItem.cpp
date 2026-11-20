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

#include "prx/libSceNKWebKit/include/DisplayListItemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstring>
#include <iterator>
#include <limits>

namespace {

using namespace NKWebKit::DisplayListAbi;

void APS5_VABI ItemCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList4ItemD1Ev(static_cast<Item*>(self));
}

void APS5_VABI ItemDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList4ItemD0Ev(static_cast<Item*>(self));
}

void APS5_VABI PureApply(const void*, void*) {
    throw std::logic_error("WebCore::DisplayList::Item::apply is pure virtual");
}

void APS5_VABI DrawingItemCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawingItemD1Ev(static_cast<DrawingItem*>(self));
}

void APS5_VABI DrawingItemDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawingItemD0Ev(static_cast<DrawingItem*>(self));
}

const char* const ApplyNames[] = {
    "WebCore::DisplayList::Save::apply",
    "WebCore::DisplayList::Restore::apply",
    "WebCore::DisplayList::Translate::apply",
    "WebCore::DisplayList::Rotate::apply",
    "WebCore::DisplayList::Scale::apply",
    "WebCore::DisplayList::ConcatenateCTM::apply",
    "WebCore::DisplayList::SetCTM::apply",
    "WebCore::DisplayList::SetState::apply",
    "WebCore::DisplayList::SetLineCap::apply",
    "WebCore::DisplayList::SetLineDash::apply",
    "WebCore::DisplayList::SetLineJoin::apply",
    "WebCore::DisplayList::SetMiterLimit::apply",
    "WebCore::DisplayList::ClearShadow::apply",
    "WebCore::DisplayList::Clip::apply",
    "WebCore::DisplayList::ClipOut::apply",
    "WebCore::DisplayList::ClipOutToPath::apply",
    "WebCore::DisplayList::ClipPath::apply",
    "WebCore::DisplayList::DrawGlyphs::apply",
    "WebCore::DisplayList::DrawImage::apply",
    "WebCore::DisplayList::DrawTiledImage::apply",
    "WebCore::DisplayList::DrawTiledScaledImage::apply",
    "WebCore::DisplayList::DrawNativeImage::apply",
    "WebCore::DisplayList::DrawPattern::apply",
    "WebCore::DisplayList::DrawRect::apply",
    "WebCore::DisplayList::DrawLine::apply",
    "WebCore::DisplayList::DrawLinesForText::apply",
    "WebCore::DisplayList::DrawDotsForDocumentMarker::apply",
    "WebCore::DisplayList::DrawEllipse::apply",
    "WebCore::DisplayList::DrawPath::apply",
    "WebCore::DisplayList::DrawFocusRingPath::apply",
    "WebCore::DisplayList::DrawFocusRingRects::apply",
    "WebCore::DisplayList::FillRect::apply",
    "WebCore::DisplayList::FillRectWithColor::apply",
    "WebCore::DisplayList::FillRectWithGradient::apply",
    "WebCore::DisplayList::FillCompositedRect::apply",
    "WebCore::DisplayList::FillRoundedRect::apply",
    "WebCore::DisplayList::FillRectWithRoundedHole::apply",
    "WebCore::DisplayList::FillPath::apply",
    "WebCore::DisplayList::FillEllipse::apply",
    "WebCore::DisplayList::PutImageData::apply",
    "WebCore::DisplayList::StrokeRect::apply",
    "WebCore::DisplayList::StrokePath::apply",
    "WebCore::DisplayList::StrokeEllipse::apply",
    "WebCore::DisplayList::ClearRect::apply",
    "WebCore::DisplayList::BeginTransparencyLayer::apply",
    "WebCore::DisplayList::EndTransparencyLayer::apply",
    "WebCore::DisplayList::ApplyDeviceScaleFactor::apply",
};

const ItemVtable ItemVtableInstance{ItemCompleteDestructor, ItemDeletingDestructor, PureApply, IsNotDrawingItem};
const DrawingItemVtable DrawingItemVtableInstance{{DrawingItemCompleteDestructor, DrawingItemDeletingDestructor, PureApply, IsDrawingItem},
    DefaultLocalBounds, DefaultGlobalBounds};

}  // namespace

namespace NKWebKit::DisplayListAbi {

const ItemVtable* ItemTable() {
    return &ItemVtableInstance;
}

const DrawingItemVtable* DrawingItemTable() {
    return &DrawingItemVtableInstance;
}

void ConstructItem(Item& item, const void* vtable, ItemType type) {
    item.vptr = vtable;
    item.refCount = 1;
    item.type = static_cast<std::uint8_t>(type);
}

void ConstructDrawingItem(Item& item, OptionalFloatRect& extent, const void* vtable, ItemType type) {
    ConstructItem(item, vtable, type);
    extent.engaged = false;
    std::memset(&extent.value, 0, 1);
}

void DestroyItem(Item& item) {
    item.vptr = &ItemVtableInstance;
}

void DeleteItem(void* item) {
    _ZdlPv_nid_postfix(item);
}

OptionalFloatRect NoBounds() {
    OptionalFloatRect result;
    std::memset(&result, 0, sizeof(result));
    return result;
}

OptionalFloatRect Bounds(const WebCore::FloatRect& rect) {
    OptionalFloatRect result = NoBounds();
    result.engaged = true;
    result.value = rect;
    return result;
}

bool APS5_VABI IsNotDrawingItem(const void*) {
    return false;
}

void APS5_VABI ApplyWithoutGraphicsContext(const void* self, void*) {
    const std::uint8_t type = static_cast<const Item*>(self)->type;
    NotImplemented_nid_no_patch(type < std::size(ApplyNames) ? ApplyNames[type] : "WebCore::DisplayList::Item::apply");
}

bool APS5_VABI IsDrawingItem(const void*) {
    return true;
}

OptionalFloatRect APS5_VABI DefaultLocalBounds(const void*, const void*) {
    return NoBounds();
}

OptionalFloatRect APS5_VABI DefaultGlobalBounds(const void*) {
    return NoBounds();
}

void CheckVectorStorage(const void* vector, const char* function) {
    Vector<unsigned char> source;
    std::memcpy(&source, vector, sizeof(source));
    if (source.size > source.capacity || (source.capacity && !source.buffer))
        throw std::invalid_argument(std::string(function) + " requires a valid vector");
}

void CopyVectorStorage(void* to, const void* from, std::size_t elementSize, const char* function) {
    CheckVectorStorage(from, function);
    Vector<unsigned char> source;
    std::memcpy(&source, from, sizeof(source));
    if (source.capacity > std::numeric_limits<std::uint32_t>::max() / elementSize)
        throw std::length_error(std::string(function) + " vector is too large");
    Vector<unsigned char> copy{nullptr, source.capacity, source.size};
    if (source.capacity) {
        copy.buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(source.capacity * elementSize));
        if (source.size) std::memcpy(copy.buffer, source.buffer, source.size * elementSize);
    }
    std::memcpy(to, &copy, sizeof(copy));
}

void DestroyVectorStorage(void* vector) {
    Vector<unsigned char> storage;
    std::memcpy(&storage, vector, sizeof(storage));
    if (storage.buffer) _ZN3WTF8fastFreeEPv(storage.buffer);
    storage.buffer = nullptr;
    storage.capacity = 0;
    std::memcpy(vector, &storage, sizeof(storage));
}

}  // namespace NKWebKit::DisplayListAbi

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(Item* self, ItemType type) {
    ConstructItem(Checked(self, "WebCore::DisplayList::Item::Item"), &ItemVtableInstance, type);
}

void APS5_VABI _ZN7WebCore11DisplayList4ItemD1Ev(Item* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Item::~Item"));
}

void APS5_VABI _ZN7WebCore11DisplayList4ItemD2Ev(Item* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Item::~Item"));
}

void APS5_VABI _ZN7WebCore11DisplayList4ItemD0Ev(Item* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Item::~Item"));
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(DrawingItem* self, ItemType type) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawingItem::DrawingItem");
    ConstructDrawingItem(item.item, item.extent, &DrawingItemVtableInstance, type);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD1Ev(DrawingItem* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawingItem::~DrawingItem").item);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD2Ev(DrawingItem* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawingItem::~DrawingItem").item);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD0Ev(DrawingItem* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawingItem::~DrawingItem").item);
    DeleteItem(self);
}

}
