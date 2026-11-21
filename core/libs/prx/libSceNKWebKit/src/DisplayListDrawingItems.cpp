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

#include "prx/libSceNKWebKit/include/DisplayListDrawingItemExports.hpp"
#include "prx/libSceNKWebKit/include/DisplayListItemExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>

namespace {

using namespace NKWebKit::DisplayListAbi;

WebCore::FloatRect FromEdges(float left, float top, float right, float bottom) {
    return {{left, top}, {right - left, bottom - top}};
}

WebCore::FloatRect FitToPoints(const WebCore::FloatPoint& p0, const WebCore::FloatPoint& p1) {
    return FromEdges(std::min(p0.m_x, p1.m_x), std::min(p0.m_y, p1.m_y), std::max(p0.m_x, p1.m_x), std::max(p0.m_y, p1.m_y));
}

void Inflate(WebCore::FloatRect& rect, float d) {
    rect.m_location.m_x = rect.x() - d;
    rect.m_size.m_width = rect.width() + d + d;
    rect.m_location.m_y = rect.y() - d;
    rect.m_size.m_height = rect.height() + d + d;
}

void APS5_VABI BeginTransparencyLayerCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList22BeginTransparencyLayerD1Ev(static_cast<BeginTransparencyLayer*>(self));
}

void APS5_VABI BeginTransparencyLayerDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList22BeginTransparencyLayerD0Ev(static_cast<BeginTransparencyLayer*>(self));
}

const DrawingItemVtable BeginTransparencyLayerTable{{BeginTransparencyLayerCompleteDestructor, BeginTransparencyLayerDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DefaultLocalBounds, DefaultGlobalBounds};

void ConstructBeginTransparencyLayer(BeginTransparencyLayer* self, float opacity) {
    auto& item = Checked(self, "WebCore::DisplayList::BeginTransparencyLayer::BeginTransparencyLayer");
    ConstructDrawingItem(item.item, item.extent, &BeginTransparencyLayerTable, ItemType::BeginTransparencyLayer);
    item.opacity = opacity;
}

void DestroyBeginTransparencyLayer(BeginTransparencyLayer* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::BeginTransparencyLayer::~BeginTransparencyLayer").item);
}

void APS5_VABI EndTransparencyLayerCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList20EndTransparencyLayerD1Ev(static_cast<EndTransparencyLayer*>(self));
}

void APS5_VABI EndTransparencyLayerDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList20EndTransparencyLayerD0Ev(static_cast<EndTransparencyLayer*>(self));
}

const DrawingItemVtable EndTransparencyLayerTable{{EndTransparencyLayerCompleteDestructor, EndTransparencyLayerDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DefaultLocalBounds, DefaultGlobalBounds};

void ConstructEndTransparencyLayer(EndTransparencyLayer* self) {
    auto& item = Checked(self, "WebCore::DisplayList::EndTransparencyLayer::EndTransparencyLayer");
    ConstructDrawingItem(item.item, item.extent, &EndTransparencyLayerTable, ItemType::EndTransparencyLayer);
}

void DestroyEndTransparencyLayer(EndTransparencyLayer* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::EndTransparencyLayer::~EndTransparencyLayer").item);
}

void APS5_VABI DrawRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawRectD1Ev(static_cast<DrawRect*>(self));
}

void APS5_VABI DrawRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawRectD0Ev(static_cast<DrawRect*>(self));
}

OptionalFloatRect APS5_VABI DrawRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawRect*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable DrawRectTable{{DrawRectCompleteDestructor, DrawRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawRectLocalBounds, DefaultGlobalBounds};

void ConstructDrawRect(DrawRect* self, const WebCore::FloatRect* rect, float borderThickness) {
    Checked(rect, "WebCore::DisplayList::DrawRect::DrawRect");
    auto& item = Checked(self, "WebCore::DisplayList::DrawRect::DrawRect");
    ConstructDrawingItem(item.item, item.extent, &DrawRectTable, ItemType::DrawRect);
    item.rect = *rect;
    item.borderThickness = borderThickness;
}

void DestroyDrawRect(DrawRect* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawRect::~DrawRect").item);
}

void APS5_VABI DrawLineCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawLineD1Ev(static_cast<DrawLine*>(self));
}

void APS5_VABI DrawLineDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8DrawLineD0Ev(static_cast<DrawLine*>(self));
}

OptionalFloatRect APS5_VABI DrawLineLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawLine*>(self);
    return Bounds(FitToPoints(item.point1, item.point2));
}

const DrawingItemVtable DrawLineTable{{DrawLineCompleteDestructor, DrawLineDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawLineLocalBounds, DefaultGlobalBounds};

void ConstructDrawLine(DrawLine* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2) {
    Checked(point1, "WebCore::DisplayList::DrawLine::DrawLine");
    Checked(point2, "WebCore::DisplayList::DrawLine::DrawLine");
    auto& item = Checked(self, "WebCore::DisplayList::DrawLine::DrawLine");
    ConstructDrawingItem(item.item, item.extent, &DrawLineTable, ItemType::DrawLine);
    item.point1 = *point1;
    item.point2 = *point2;
}

void DestroyDrawLine(DrawLine* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawLine::~DrawLine").item);
}

void APS5_VABI DrawLinesForTextCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList16DrawLinesForTextD1Ev(static_cast<DrawLinesForText*>(self));
}

void APS5_VABI DrawLinesForTextDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList16DrawLinesForTextD0Ev(static_cast<DrawLinesForText*>(self));
}

OptionalFloatRect APS5_VABI DrawLinesForTextLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawLinesForText*>(self);
    if (!item.widths.size) return Bounds(WebCore::FloatRect{});
    WebCore::FloatPoint point{item.blockLocation.m_x + item.localAnchor.m_width, item.blockLocation.m_y + item.localAnchor.m_height};
    WebCore::FloatRect result{point, {static_cast<float>(item.widths.buffer[item.widths.size - 1]), item.thickness}};
    Inflate(result, 1);
    return Bounds(result);
}

const DrawingItemVtable DrawLinesForTextTable{{DrawLinesForTextCompleteDestructor, DrawLinesForTextDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawLinesForTextLocalBounds, DefaultGlobalBounds};

void ConstructDrawLinesForText(DrawLinesForText* self, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, float thickness, const Vector<double>* widths, bool printing, bool doubleLines) {
    Checked(blockLocation, "WebCore::DisplayList::DrawLinesForText::DrawLinesForText");
    Checked(localAnchor, "WebCore::DisplayList::DrawLinesForText::DrawLinesForText");
    CheckVector(widths, "WebCore::DisplayList::DrawLinesForText::DrawLinesForText");
    auto& item = Checked(self, "WebCore::DisplayList::DrawLinesForText::DrawLinesForText");
    ConstructDrawingItem(item.item, item.extent, &DrawLinesForTextTable, ItemType::DrawLinesForText);
    item.blockLocation = *blockLocation;
    item.localAnchor = *localAnchor;
    CopyVector(item.widths, widths, "WebCore::DisplayList::DrawLinesForText::DrawLinesForText");
    item.thickness = thickness;
    item.printing = printing;
    item.doubleLines = doubleLines;
}

void DestroyDrawLinesForText(DrawLinesForText* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawLinesForText::~DrawLinesForText");
    DestroyVector(item.widths);
    DestroyItem(item.item);
}

void APS5_VABI DrawDotsForDocumentMarkerCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD1Ev(static_cast<DrawDotsForDocumentMarker*>(self));
}

void APS5_VABI DrawDotsForDocumentMarkerDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD0Ev(static_cast<DrawDotsForDocumentMarker*>(self));
}

OptionalFloatRect APS5_VABI DrawDotsForDocumentMarkerLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawDotsForDocumentMarker*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable DrawDotsForDocumentMarkerTable{{DrawDotsForDocumentMarkerCompleteDestructor, DrawDotsForDocumentMarkerDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawDotsForDocumentMarkerLocalBounds, DefaultGlobalBounds};

void ConstructDrawDotsForDocumentMarker(DrawDotsForDocumentMarker* self, const WebCore::FloatRect* rect, DocumentMarkerLineStyle style) {
    Checked(rect, "WebCore::DisplayList::DrawDotsForDocumentMarker::DrawDotsForDocumentMarker");
    auto& item = Checked(self, "WebCore::DisplayList::DrawDotsForDocumentMarker::DrawDotsForDocumentMarker");
    ConstructDrawingItem(item.item, item.extent, &DrawDotsForDocumentMarkerTable, ItemType::DrawDotsForDocumentMarker);
    item.rect = *rect;
    item.style = style;
}

void DestroyDrawDotsForDocumentMarker(DrawDotsForDocumentMarker* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawDotsForDocumentMarker::~DrawDotsForDocumentMarker").item);
}

void APS5_VABI DrawEllipseCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawEllipseD1Ev(static_cast<DrawEllipse*>(self));
}

void APS5_VABI DrawEllipseDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawEllipseD0Ev(static_cast<DrawEllipse*>(self));
}

OptionalFloatRect APS5_VABI DrawEllipseLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawEllipse*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable DrawEllipseTable{{DrawEllipseCompleteDestructor, DrawEllipseDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawEllipseLocalBounds, DefaultGlobalBounds};

void ConstructDrawEllipse(DrawEllipse* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::DrawEllipse::DrawEllipse");
    auto& item = Checked(self, "WebCore::DisplayList::DrawEllipse::DrawEllipse");
    ConstructDrawingItem(item.item, item.extent, &DrawEllipseTable, ItemType::DrawEllipse);
    item.rect = *rect;
}

void DestroyDrawEllipse(DrawEllipse* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::DrawEllipse::~DrawEllipse").item);
}

void APS5_VABI FillRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList8FillRectD1Ev(static_cast<FillRect*>(self));
}

void APS5_VABI FillRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList8FillRectD0Ev(static_cast<FillRect*>(self));
}

OptionalFloatRect APS5_VABI FillRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillRect*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable FillRectTable{{FillRectCompleteDestructor, FillRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillRectLocalBounds, DefaultGlobalBounds};

void ConstructFillRect(FillRect* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::FillRect::FillRect");
    auto& item = Checked(self, "WebCore::DisplayList::FillRect::FillRect");
    ConstructDrawingItem(item.item, item.extent, &FillRectTable, ItemType::FillRect);
    item.rect = *rect;
}

void DestroyFillRect(FillRect* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::FillRect::~FillRect").item);
}

void APS5_VABI FillEllipseCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11FillEllipseD1Ev(static_cast<FillEllipse*>(self));
}

void APS5_VABI FillEllipseDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11FillEllipseD0Ev(static_cast<FillEllipse*>(self));
}

OptionalFloatRect APS5_VABI FillEllipseLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillEllipse*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable FillEllipseTable{{FillEllipseCompleteDestructor, FillEllipseDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillEllipseLocalBounds, DefaultGlobalBounds};

void ConstructFillEllipse(FillEllipse* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::FillEllipse::FillEllipse");
    auto& item = Checked(self, "WebCore::DisplayList::FillEllipse::FillEllipse");
    ConstructDrawingItem(item.item, item.extent, &FillEllipseTable, ItemType::FillEllipse);
    item.rect = *rect;
}

void DestroyFillEllipse(FillEllipse* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::FillEllipse::~FillEllipse").item);
}

void APS5_VABI StrokeRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList10StrokeRectD1Ev(static_cast<StrokeRect*>(self));
}

void APS5_VABI StrokeRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList10StrokeRectD0Ev(static_cast<StrokeRect*>(self));
}

OptionalFloatRect APS5_VABI StrokeRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const StrokeRect*>(self);
    WebCore::FloatRect bounds = item.rect;
    bounds.m_size.m_width += item.lineWidth;
    bounds.m_size.m_height += item.lineWidth;
    return Bounds(bounds);
}

const DrawingItemVtable StrokeRectTable{{StrokeRectCompleteDestructor, StrokeRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, StrokeRectLocalBounds, DefaultGlobalBounds};

void ConstructStrokeRect(StrokeRect* self, const WebCore::FloatRect* rect, float lineWidth) {
    Checked(rect, "WebCore::DisplayList::StrokeRect::StrokeRect");
    auto& item = Checked(self, "WebCore::DisplayList::StrokeRect::StrokeRect");
    ConstructDrawingItem(item.item, item.extent, &StrokeRectTable, ItemType::StrokeRect);
    item.rect = *rect;
    item.lineWidth = lineWidth;
}

void DestroyStrokeRect(StrokeRect* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::StrokeRect::~StrokeRect").item);
}

void APS5_VABI StrokeEllipseCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList13StrokeEllipseD1Ev(static_cast<StrokeEllipse*>(self));
}

void APS5_VABI StrokeEllipseDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList13StrokeEllipseD0Ev(static_cast<StrokeEllipse*>(self));
}

OptionalFloatRect APS5_VABI StrokeEllipseLocalBounds(const void*, const void*) {
    NotImplemented_nid_no_patch("WebCore::DisplayList::StrokeEllipse::localBounds");
}

const DrawingItemVtable StrokeEllipseTable{{StrokeEllipseCompleteDestructor, StrokeEllipseDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, StrokeEllipseLocalBounds, DefaultGlobalBounds};

void ConstructStrokeEllipse(StrokeEllipse* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::StrokeEllipse::StrokeEllipse");
    auto& item = Checked(self, "WebCore::DisplayList::StrokeEllipse::StrokeEllipse");
    ConstructDrawingItem(item.item, item.extent, &StrokeEllipseTable, ItemType::StrokeEllipse);
    item.rect = *rect;
}

void DestroyStrokeEllipse(StrokeEllipse* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::StrokeEllipse::~StrokeEllipse").item);
}

void APS5_VABI ClearRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList9ClearRectD1Ev(static_cast<ClearRect*>(self));
}

void APS5_VABI ClearRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList9ClearRectD0Ev(static_cast<ClearRect*>(self));
}

OptionalFloatRect APS5_VABI ClearRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const ClearRect*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable ClearRectTable{{ClearRectCompleteDestructor, ClearRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, ClearRectLocalBounds, DefaultGlobalBounds};

void ConstructClearRect(ClearRect* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::ClearRect::ClearRect");
    auto& item = Checked(self, "WebCore::DisplayList::ClearRect::ClearRect");
    ConstructDrawingItem(item.item, item.extent, &ClearRectTable, ItemType::ClearRect);
    item.rect = *rect;
}

void DestroyClearRect(ClearRect* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::ClearRect::~ClearRect").item);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerC1Ef(BeginTransparencyLayer* self, float opacity) {
    ConstructBeginTransparencyLayer(self, opacity);
}

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerC2Ef(BeginTransparencyLayer* self, float opacity) {
    ConstructBeginTransparencyLayer(self, opacity);
}

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD0Ev(BeginTransparencyLayer* self) {
    DestroyBeginTransparencyLayer(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD1Ev(BeginTransparencyLayer* self) {
    DestroyBeginTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD2Ev(BeginTransparencyLayer* self) {
    DestroyBeginTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerC1Ev(EndTransparencyLayer* self) {
    ConstructEndTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerC2Ev(EndTransparencyLayer* self) {
    ConstructEndTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD0Ev(EndTransparencyLayer* self) {
    DestroyEndTransparencyLayer(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD1Ev(EndTransparencyLayer* self) {
    DestroyEndTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD2Ev(EndTransparencyLayer* self) {
    DestroyEndTransparencyLayer(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectC1ERKNS_9FloatRectEf(DrawRect* self, const WebCore::FloatRect* rect, float borderThickness) {
    ConstructDrawRect(self, rect, borderThickness);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectC2ERKNS_9FloatRectEf(DrawRect* self, const WebCore::FloatRect* rect, float borderThickness) {
    ConstructDrawRect(self, rect, borderThickness);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD0Ev(DrawRect* self) {
    DestroyDrawRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD1Ev(DrawRect* self) {
    DestroyDrawRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD2Ev(DrawRect* self) {
    DestroyDrawRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineC1ERKNS_10FloatPointES4_(DrawLine* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2) {
    ConstructDrawLine(self, point1, point2);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineC2ERKNS_10FloatPointES4_(DrawLine* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2) {
    ConstructDrawLine(self, point1, point2);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD0Ev(DrawLine* self) {
    DestroyDrawLine(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD1Ev(DrawLine* self) {
    DestroyDrawLine(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD2Ev(DrawLine* self) {
    DestroyDrawLine(self);
}

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(DrawLinesForText* self, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, float thickness, const Vector<double>* widths, bool printing, bool doubleLines) {
    ConstructDrawLinesForText(self, blockLocation, localAnchor, thickness, widths, printing, doubleLines);
}

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextC2ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(DrawLinesForText* self, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, float thickness, const Vector<double>* widths, bool printing, bool doubleLines) {
    ConstructDrawLinesForText(self, blockLocation, localAnchor, thickness, widths, printing, doubleLines);
}

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD0Ev(DrawLinesForText* self) {
    DestroyDrawLinesForText(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD1Ev(DrawLinesForText* self) {
    DestroyDrawLinesForText(self);
}

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD2Ev(DrawLinesForText* self) {
    DestroyDrawLinesForText(self);
}

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC1ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(DrawDotsForDocumentMarker* self, const WebCore::FloatRect* rect, DocumentMarkerLineStyle style) {
    ConstructDrawDotsForDocumentMarker(self, rect, style);
}

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC2ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(DrawDotsForDocumentMarker* self, const WebCore::FloatRect* rect, DocumentMarkerLineStyle style) {
    ConstructDrawDotsForDocumentMarker(self, rect, style);
}

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD0Ev(DrawDotsForDocumentMarker* self) {
    DestroyDrawDotsForDocumentMarker(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD1Ev(DrawDotsForDocumentMarker* self) {
    DestroyDrawDotsForDocumentMarker(self);
}

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD2Ev(DrawDotsForDocumentMarker* self) {
    DestroyDrawDotsForDocumentMarker(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseC1ERKNS_9FloatRectE(DrawEllipse* self, const WebCore::FloatRect* rect) {
    ConstructDrawEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseC2ERKNS_9FloatRectE(DrawEllipse* self, const WebCore::FloatRect* rect) {
    ConstructDrawEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD0Ev(DrawEllipse* self) {
    DestroyDrawEllipse(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD1Ev(DrawEllipse* self) {
    DestroyDrawEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD2Ev(DrawEllipse* self) {
    DestroyDrawEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8FillRectC1ERKNS_9FloatRectE(FillRect* self, const WebCore::FloatRect* rect) {
    ConstructFillRect(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList8FillRectC2ERKNS_9FloatRectE(FillRect* self, const WebCore::FloatRect* rect) {
    ConstructFillRect(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList8FillRectD0Ev(FillRect* self) {
    DestroyFillRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8FillRectD1Ev(FillRect* self) {
    DestroyFillRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList8FillRectD2Ev(FillRect* self) {
    DestroyFillRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseC1ERKNS_9FloatRectE(FillEllipse* self, const WebCore::FloatRect* rect) {
    ConstructFillEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseC2ERKNS_9FloatRectE(FillEllipse* self, const WebCore::FloatRect* rect) {
    ConstructFillEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD0Ev(FillEllipse* self) {
    DestroyFillEllipse(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD1Ev(FillEllipse* self) {
    DestroyFillEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD2Ev(FillEllipse* self) {
    DestroyFillEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectC1ERKNS_9FloatRectEf(StrokeRect* self, const WebCore::FloatRect* rect, float lineWidth) {
    ConstructStrokeRect(self, rect, lineWidth);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectC2ERKNS_9FloatRectEf(StrokeRect* self, const WebCore::FloatRect* rect, float lineWidth) {
    ConstructStrokeRect(self, rect, lineWidth);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD0Ev(StrokeRect* self) {
    DestroyStrokeRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD1Ev(StrokeRect* self) {
    DestroyStrokeRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD2Ev(StrokeRect* self) {
    DestroyStrokeRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseC1ERKNS_9FloatRectE(StrokeEllipse* self, const WebCore::FloatRect* rect) {
    ConstructStrokeEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseC2ERKNS_9FloatRectE(StrokeEllipse* self, const WebCore::FloatRect* rect) {
    ConstructStrokeEllipse(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD0Ev(StrokeEllipse* self) {
    DestroyStrokeEllipse(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD1Ev(StrokeEllipse* self) {
    DestroyStrokeEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD2Ev(StrokeEllipse* self) {
    DestroyStrokeEllipse(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectC1ERKNS_9FloatRectE(ClearRect* self, const WebCore::FloatRect* rect) {
    ConstructClearRect(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectC2ERKNS_9FloatRectE(ClearRect* self, const WebCore::FloatRect* rect) {
    ConstructClearRect(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD0Ev(ClearRect* self) {
    DestroyClearRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD1Ev(ClearRect* self) {
    DestroyClearRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD2Ev(ClearRect* self) {
    DestroyClearRect(self);
}

}
