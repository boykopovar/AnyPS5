// Derived from Source/WebCore/platform/graphics/displaylists/DisplayListItems.h and DisplayListItems.cpp@c55364d53f971f63a709976f168d932d558b8909
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/PathAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList8FillPathD0Ev(NKWebKit::PathAbi::FillPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8FillPathD1Ev(NKWebKit::PathAbi::FillPath* self);
void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD0Ev(NKWebKit::PathAbi::StrokePath* self);
void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD1Ev(NKWebKit::PathAbi::StrokePath* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD0Ev(NKWebKit::PathAbi::DrawPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD1Ev(NKWebKit::PathAbi::DrawPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD0Ev(NKWebKit::PathAbi::ClipPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD1Ev(NKWebKit::PathAbi::ClipPath* self);
void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD0Ev(NKWebKit::PathAbi::ClipOutToPath* self);
void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD1Ev(NKWebKit::PathAbi::ClipOutToPath* self);
void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD0Ev(NKWebKit::PathAbi::DrawFocusRingPath* self);
void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD1Ev(NKWebKit::PathAbi::DrawFocusRingPath* self);

}

namespace {

using namespace NKWebKit::DisplayListAbi;
using namespace NKWebKit::PathAbi;

OptionalFloatRect APS5_VABI FillPathLocalBounds(const void* self, const void*) {
    const auto& item = Checked(static_cast<const FillPath*>(self), "WebCore::DisplayList::FillPath::localBounds");
    return Bounds(_ZNK7WebCore4Path16fastBoundingRectEv(&item.path));
}

OptionalFloatRect APS5_VABI DrawPathLocalBounds(const void* self, const void*) {
    const auto& item = Checked(static_cast<const DrawPath*>(self), "WebCore::DisplayList::DrawPath::localBounds");
    return Bounds(_ZNK7WebCore4Path16fastBoundingRectEv(&item.path));
}

void APS5_VABI FillPathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8FillPathD1Ev(static_cast<FillPath*>(self));
}

void APS5_VABI FillPathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8FillPathD0Ev(static_cast<FillPath*>(self));
}

const DrawingItemVtable FillPathTable{
    {FillPathCompleteDestructor, FillPathDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem},
    FillPathLocalBounds,
    DefaultGlobalBounds,
};

void APS5_VABI StrokePathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList10StrokePathD1Ev(static_cast<StrokePath*>(self));
}

void APS5_VABI StrokePathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList10StrokePathD0Ev(static_cast<StrokePath*>(self));
}

const DrawingItemVtable StrokePathTable{
    {StrokePathCompleteDestructor, StrokePathDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem},
    DefaultLocalBounds,
    DefaultGlobalBounds,
};

void APS5_VABI DrawPathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawPathD1Ev(static_cast<DrawPath*>(self));
}

void APS5_VABI DrawPathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawPathD0Ev(static_cast<DrawPath*>(self));
}

const DrawingItemVtable DrawPathTable{
    {DrawPathCompleteDestructor, DrawPathDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem},
    DrawPathLocalBounds,
    DefaultGlobalBounds,
};

void APS5_VABI ClipPathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8ClipPathD1Ev(static_cast<ClipPath*>(self));
}

void APS5_VABI ClipPathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8ClipPathD0Ev(static_cast<ClipPath*>(self));
}

const ItemVtable ClipPathTable{ClipPathCompleteDestructor, ClipPathDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void APS5_VABI ClipOutToPathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList13ClipOutToPathD1Ev(static_cast<ClipOutToPath*>(self));
}

void APS5_VABI ClipOutToPathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList13ClipOutToPathD0Ev(static_cast<ClipOutToPath*>(self));
}

const ItemVtable ClipOutToPathTable{ClipOutToPathCompleteDestructor, ClipOutToPathDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void APS5_VABI DrawFocusRingPathCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList17DrawFocusRingPathD1Ev(static_cast<DrawFocusRingPath*>(self));
}

void APS5_VABI DrawFocusRingPathDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList17DrawFocusRingPathD0Ev(static_cast<DrawFocusRingPath*>(self));
}

const DrawingItemVtable DrawFocusRingPathTable{
    {DrawFocusRingPathCompleteDestructor, DrawFocusRingPathDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem},
    DefaultLocalBounds,
    DefaultGlobalBounds,
};

void ConstructFillPath(FillPath& self, const Path& path) {
    ConstructDrawingItem(self.drawingItem.item, self.drawingItem.extent, &FillPathTable, ItemType::FillPath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
}

void ConstructStrokePath(StrokePath& self, const Path& path) {
    ConstructDrawingItem(self.drawingItem.item, self.drawingItem.extent, &StrokePathTable, ItemType::StrokePath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
}

void ConstructDrawPath(DrawPath& self, const Path& path) {
    ConstructDrawingItem(self.drawingItem.item, self.drawingItem.extent, &DrawPathTable, ItemType::DrawPath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
}

void ConstructClipPath(ClipPath& self, const Path& path, std::uint8_t windRule) {
    ConstructItem(self.item, &ClipPathTable, ItemType::ClipPath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
    self.windRule = windRule;
}

void ConstructClipOutToPath(ClipOutToPath& self, const Path& path) {
    ConstructItem(self.item, &ClipOutToPathTable, ItemType::ClipOutToPath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
}

void ConstructDrawFocusRingPath(DrawFocusRingPath& self, const Path& path, float width, float offset, const WebCore::Color& color) {
    ConstructDrawingItem(self.drawingItem.item, self.drawingItem.extent, &DrawFocusRingPathTable, ItemType::DrawFocusRingPath);
    _ZN7WebCore4PathC1ERKS0_(&self.path, &path);
    self.width = width;
    self.offset = offset;
    self.color = color;
}

void DestroyFillPath(FillPath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.drawingItem.item);
}

void DestroyStrokePath(StrokePath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.drawingItem.item);
}

void DestroyDrawPath(DrawPath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.drawingItem.item);
}

void DestroyClipPath(ClipPath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.item);
}

void DestroyClipOutToPath(ClipOutToPath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.item);
}

void DestroyDrawFocusRingPath(DrawFocusRingPath& self) {
    _ZN7WebCore4PathD1Ev(&self.path);
    DestroyItem(self.drawingItem.item);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList8FillPathC1ERKNS_4PathE(FillPath* self, const Path* path) {
    ConstructFillPath(Checked(self, "WebCore::DisplayList::FillPath::FillPath"), Checked(path, "WebCore::DisplayList::FillPath::FillPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8FillPathC2ERKNS_4PathE(FillPath* self, const Path* path) {
    ConstructFillPath(Checked(self, "WebCore::DisplayList::FillPath::FillPath"), Checked(path, "WebCore::DisplayList::FillPath::FillPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8FillPathD1Ev(FillPath* self) {
    DestroyFillPath(Checked(self, "WebCore::DisplayList::FillPath::~FillPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8FillPathD2Ev(FillPath* self) {
    DestroyFillPath(Checked(self, "WebCore::DisplayList::FillPath::~FillPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8FillPathD0Ev(FillPath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::FillPath::~FillPath");
    DestroyFillPath(item);
    DeleteItem(&item);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathC1ERKNS_4PathE(StrokePath* self, const Path* path) {
    ConstructStrokePath(Checked(self, "WebCore::DisplayList::StrokePath::StrokePath"), Checked(path, "WebCore::DisplayList::StrokePath::StrokePath"));
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathC2ERKNS_4PathE(StrokePath* self, const Path* path) {
    ConstructStrokePath(Checked(self, "WebCore::DisplayList::StrokePath::StrokePath"), Checked(path, "WebCore::DisplayList::StrokePath::StrokePath"));
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD1Ev(StrokePath* self) {
    DestroyStrokePath(Checked(self, "WebCore::DisplayList::StrokePath::~StrokePath"));
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD2Ev(StrokePath* self) {
    DestroyStrokePath(Checked(self, "WebCore::DisplayList::StrokePath::~StrokePath"));
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD0Ev(StrokePath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::StrokePath::~StrokePath");
    DestroyStrokePath(item);
    DeleteItem(&item);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathC1ERKNS_4PathE(DrawPath* self, const Path* path) {
    ConstructDrawPath(Checked(self, "WebCore::DisplayList::DrawPath::DrawPath"), Checked(path, "WebCore::DisplayList::DrawPath::DrawPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathC2ERKNS_4PathE(DrawPath* self, const Path* path) {
    ConstructDrawPath(Checked(self, "WebCore::DisplayList::DrawPath::DrawPath"), Checked(path, "WebCore::DisplayList::DrawPath::DrawPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD1Ev(DrawPath* self) {
    DestroyDrawPath(Checked(self, "WebCore::DisplayList::DrawPath::~DrawPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD2Ev(DrawPath* self) {
    DestroyDrawPath(Checked(self, "WebCore::DisplayList::DrawPath::~DrawPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD0Ev(DrawPath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawPath::~DrawPath");
    DestroyDrawPath(item);
    DeleteItem(&item);
}

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathC1ERKNS_4PathENS_8WindRuleE(ClipPath* self, const Path* path, std::uint8_t windRule) {
    ConstructClipPath(Checked(self, "WebCore::DisplayList::ClipPath::ClipPath"), Checked(path, "WebCore::DisplayList::ClipPath::ClipPath"), windRule);
}

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathC2ERKNS_4PathENS_8WindRuleE(ClipPath* self, const Path* path, std::uint8_t windRule) {
    ConstructClipPath(Checked(self, "WebCore::DisplayList::ClipPath::ClipPath"), Checked(path, "WebCore::DisplayList::ClipPath::ClipPath"), windRule);
}

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD1Ev(ClipPath* self) {
    DestroyClipPath(Checked(self, "WebCore::DisplayList::ClipPath::~ClipPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD2Ev(ClipPath* self) {
    DestroyClipPath(Checked(self, "WebCore::DisplayList::ClipPath::~ClipPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD0Ev(ClipPath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::ClipPath::~ClipPath");
    DestroyClipPath(item);
    DeleteItem(&item);
}

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathC1ERKNS_4PathE(ClipOutToPath* self, const Path* path) {
    ConstructClipOutToPath(Checked(self, "WebCore::DisplayList::ClipOutToPath::ClipOutToPath"), Checked(path, "WebCore::DisplayList::ClipOutToPath::ClipOutToPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathC2ERKNS_4PathE(ClipOutToPath* self, const Path* path) {
    ConstructClipOutToPath(Checked(self, "WebCore::DisplayList::ClipOutToPath::ClipOutToPath"), Checked(path, "WebCore::DisplayList::ClipOutToPath::ClipOutToPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD1Ev(ClipOutToPath* self) {
    DestroyClipOutToPath(Checked(self, "WebCore::DisplayList::ClipOutToPath::~ClipOutToPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD2Ev(ClipOutToPath* self) {
    DestroyClipOutToPath(Checked(self, "WebCore::DisplayList::ClipOutToPath::~ClipOutToPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD0Ev(ClipOutToPath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::ClipOutToPath::~ClipOutToPath");
    DestroyClipOutToPath(item);
    DeleteItem(&item);
}

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathC1ERKNS_4PathEffRKNS_5ColorE(DrawFocusRingPath* self, const Path* path, float width, float offset, const WebCore::Color* color) {
    if (!color) throw std::invalid_argument("WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath requires a color");
    ConstructDrawFocusRingPath(Checked(self, "WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath"), Checked(path, "WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath"), width, offset, *color);
}

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathC2ERKNS_4PathEffRKNS_5ColorE(DrawFocusRingPath* self, const Path* path, float width, float offset, const WebCore::Color* color) {
    if (!color) throw std::invalid_argument("WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath requires a color");
    ConstructDrawFocusRingPath(Checked(self, "WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath"), Checked(path, "WebCore::DisplayList::DrawFocusRingPath::DrawFocusRingPath"), width, offset, *color);
}

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD1Ev(DrawFocusRingPath* self) {
    DestroyDrawFocusRingPath(Checked(self, "WebCore::DisplayList::DrawFocusRingPath::~DrawFocusRingPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD2Ev(DrawFocusRingPath* self) {
    DestroyDrawFocusRingPath(Checked(self, "WebCore::DisplayList::DrawFocusRingPath::~DrawFocusRingPath"));
}

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD0Ev(DrawFocusRingPath* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawFocusRingPath::~DrawFocusRingPath");
    DestroyDrawFocusRingPath(item);
    DeleteItem(&item);
}

}
