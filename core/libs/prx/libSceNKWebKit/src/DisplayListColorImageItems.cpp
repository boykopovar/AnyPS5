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

#include "prx/libSceNKWebKit/include/DisplayListColorImageItemExports.hpp"
#include "prx/libSceNKWebKit/include/DisplayListItemExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>

namespace {

using namespace NKWebKit::DisplayListAbi;

constexpr float PlatformFocusRingWidth = 3;

bool IsEmpty(const WebCore::FloatRect& rect) {
    return rect.m_size.m_width <= 0 || rect.m_size.m_height <= 0;
}

void Unite(WebCore::FloatRect& rect, const WebCore::FloatRect& other) {
    if (IsEmpty(other)) return;
    if (IsEmpty(rect)) {
        rect = other;
        return;
    }
    float minX = std::min(rect.x(), other.x());
    float minY = std::min(rect.y(), other.y());
    float maxX = std::max(rect.x() + rect.width(), other.x() + other.width());
    float maxY = std::max(rect.y() + rect.height(), other.y() + other.height());
    rect = {{minX, minY}, {maxX - minX, maxY - minY}};
}

void Inflate(WebCore::FloatRect& rect, float d) {
    rect.m_location.m_x = rect.x() - d;
    rect.m_size.m_width = rect.width() + d + d;
    rect.m_location.m_y = rect.y() - d;
    rect.m_size.m_height = rect.height() + d + d;
}

bool IsExtended(const Color& color) {
    return !(color.inlineColorAndFlags & 1);
}

ExtendedColor* Extended(const Color& color) {
    return reinterpret_cast<ExtendedColor*>(static_cast<std::uintptr_t>(color.inlineColorAndFlags));
}

void CheckColor(const Color& color, const char* function) {
    if (IsExtended(color) && !Extended(color)) throw std::invalid_argument(std::string(function) + " requires a color with an extended color");
}

Color RetainColor(const Color& color) {
    if (IsExtended(color)) ++Extended(color)->refCount;
    return color;
}

void ReleaseColor(Color& color) {
    if (!IsExtended(color)) return;
    ExtendedColor* extended = Extended(color);
    if (extended->refCount == 1) {
        _ZdlPv_nid_postfix(extended);
        return;
    }
    --extended->refCount;
}

Image* RetainImage(Image* image) {
    ++image->refCount;
    return image;
}

void ReleaseImage(Image* image) {
    if (image->refCount != 1) {
        --image->refCount;
        return;
    }
    using DeletingDestructor = void (APS5_VABI*)(Image*);
    reinterpret_cast<DeletingDestructor>(const_cast<void*>(image->vptr[1]))(image);
}

void APS5_VABI FillRectWithColorCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList17FillRectWithColorD1Ev(static_cast<FillRectWithColor*>(self));
}

void APS5_VABI FillRectWithColorDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList17FillRectWithColorD0Ev(static_cast<FillRectWithColor*>(self));
}

OptionalFloatRect APS5_VABI FillRectWithColorLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillRectWithColor*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable FillRectWithColorTable{{FillRectWithColorCompleteDestructor, FillRectWithColorDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillRectWithColorLocalBounds, DefaultGlobalBounds};

void ConstructFillRectWithColor(FillRectWithColor* self, const WebCore::FloatRect* rect, const Color* color) {
    Checked(rect, "WebCore::DisplayList::FillRectWithColor::FillRectWithColor");
    CheckColor(Checked(color, "WebCore::DisplayList::FillRectWithColor::FillRectWithColor"), "WebCore::DisplayList::FillRectWithColor::FillRectWithColor");
    auto& item = Checked(self, "WebCore::DisplayList::FillRectWithColor::FillRectWithColor");
    ConstructDrawingItem(item.item, item.extent, &FillRectWithColorTable, ItemType::FillRectWithColor);
    item.rect = *rect;
    item.color = RetainColor(*color);
}

void DestroyFillRectWithColor(FillRectWithColor* self) {
    auto& item = Checked(self, "WebCore::DisplayList::FillRectWithColor::~FillRectWithColor");
    ReleaseColor(item.color);
    DestroyItem(item.item);
}

void APS5_VABI FillCompositedRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList18FillCompositedRectD1Ev(static_cast<FillCompositedRect*>(self));
}

void APS5_VABI FillCompositedRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList18FillCompositedRectD0Ev(static_cast<FillCompositedRect*>(self));
}

OptionalFloatRect APS5_VABI FillCompositedRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillCompositedRect*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable FillCompositedRectTable{{FillCompositedRectCompleteDestructor, FillCompositedRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillCompositedRectLocalBounds, DefaultGlobalBounds};

void ConstructFillCompositedRect(FillCompositedRect* self, const WebCore::FloatRect* rect, const Color* color, std::uint8_t op, std::uint8_t blendMode) {
    Checked(rect, "WebCore::DisplayList::FillCompositedRect::FillCompositedRect");
    CheckColor(Checked(color, "WebCore::DisplayList::FillCompositedRect::FillCompositedRect"), "WebCore::DisplayList::FillCompositedRect::FillCompositedRect");
    auto& item = Checked(self, "WebCore::DisplayList::FillCompositedRect::FillCompositedRect");
    ConstructDrawingItem(item.item, item.extent, &FillCompositedRectTable, ItemType::FillCompositedRect);
    item.rect = *rect;
    item.color = RetainColor(*color);
    item.op = op;
    item.blendMode = blendMode;
}

void DestroyFillCompositedRect(FillCompositedRect* self) {
    auto& item = Checked(self, "WebCore::DisplayList::FillCompositedRect::~FillCompositedRect");
    ReleaseColor(item.color);
    DestroyItem(item.item);
}

void APS5_VABI FillRoundedRectCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList15FillRoundedRectD1Ev(static_cast<FillRoundedRect*>(self));
}

void APS5_VABI FillRoundedRectDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList15FillRoundedRectD0Ev(static_cast<FillRoundedRect*>(self));
}

OptionalFloatRect APS5_VABI FillRoundedRectLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillRoundedRect*>(self);
    return Bounds(item.rect.m_rect);
}

const DrawingItemVtable FillRoundedRectTable{{FillRoundedRectCompleteDestructor, FillRoundedRectDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillRoundedRectLocalBounds, DefaultGlobalBounds};

void ConstructFillRoundedRect(FillRoundedRect* self, const WebCore::FloatRoundedRect* rect, const Color* color, std::uint8_t blendMode) {
    Checked(rect, "WebCore::DisplayList::FillRoundedRect::FillRoundedRect");
    CheckColor(Checked(color, "WebCore::DisplayList::FillRoundedRect::FillRoundedRect"), "WebCore::DisplayList::FillRoundedRect::FillRoundedRect");
    auto& item = Checked(self, "WebCore::DisplayList::FillRoundedRect::FillRoundedRect");
    ConstructDrawingItem(item.item, item.extent, &FillRoundedRectTable, ItemType::FillRoundedRect);
    item.rect = *rect;
    item.color = RetainColor(*color);
    item.blendMode = blendMode;
}

void DestroyFillRoundedRect(FillRoundedRect* self) {
    auto& item = Checked(self, "WebCore::DisplayList::FillRoundedRect::~FillRoundedRect");
    ReleaseColor(item.color);
    DestroyItem(item.item);
}

void APS5_VABI FillRectWithRoundedHoleCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD1Ev(static_cast<FillRectWithRoundedHole*>(self));
}

void APS5_VABI FillRectWithRoundedHoleDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD0Ev(static_cast<FillRectWithRoundedHole*>(self));
}

OptionalFloatRect APS5_VABI FillRectWithRoundedHoleLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const FillRectWithRoundedHole*>(self);
    return Bounds(item.rect);
}

const DrawingItemVtable FillRectWithRoundedHoleTable{{FillRectWithRoundedHoleCompleteDestructor, FillRectWithRoundedHoleDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, FillRectWithRoundedHoleLocalBounds, DefaultGlobalBounds};

void ConstructFillRectWithRoundedHole(FillRectWithRoundedHole* self, const WebCore::FloatRect* rect, const WebCore::FloatRoundedRect* roundedHoleRect, const Color* color) {
    Checked(rect, "WebCore::DisplayList::FillRectWithRoundedHole::FillRectWithRoundedHole");
    Checked(roundedHoleRect, "WebCore::DisplayList::FillRectWithRoundedHole::FillRectWithRoundedHole");
    CheckColor(Checked(color, "WebCore::DisplayList::FillRectWithRoundedHole::FillRectWithRoundedHole"), "WebCore::DisplayList::FillRectWithRoundedHole::FillRectWithRoundedHole");
    auto& item = Checked(self, "WebCore::DisplayList::FillRectWithRoundedHole::FillRectWithRoundedHole");
    ConstructDrawingItem(item.item, item.extent, &FillRectWithRoundedHoleTable, ItemType::FillRectWithRoundedHole);
    item.rect = *rect;
    item.roundedHoleRect = *roundedHoleRect;
    item.color = RetainColor(*color);
}

void DestroyFillRectWithRoundedHole(FillRectWithRoundedHole* self) {
    auto& item = Checked(self, "WebCore::DisplayList::FillRectWithRoundedHole::~FillRectWithRoundedHole");
    ReleaseColor(item.color);
    DestroyItem(item.item);
}

void APS5_VABI DrawFocusRingRectsCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList18DrawFocusRingRectsD1Ev(static_cast<DrawFocusRingRects*>(self));
}

void APS5_VABI DrawFocusRingRectsDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList18DrawFocusRingRectsD0Ev(static_cast<DrawFocusRingRects*>(self));
}

OptionalFloatRect APS5_VABI DrawFocusRingRectsLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawFocusRingRects*>(self);
    WebCore::FloatRect result{};
    for (std::uint32_t index = 0; index < item.rects.size; ++index) Unite(result, item.rects.buffer[index]);
    Inflate(result, PlatformFocusRingWidth);
    return Bounds(result);
}

const DrawingItemVtable DrawFocusRingRectsTable{{DrawFocusRingRectsCompleteDestructor, DrawFocusRingRectsDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawFocusRingRectsLocalBounds, DefaultGlobalBounds};

void ConstructDrawFocusRingRects(DrawFocusRingRects* self, const Vector<WebCore::FloatRect>* rects, float width, float offset, const Color* color) {
    CheckVector(rects, "WebCore::DisplayList::DrawFocusRingRects::DrawFocusRingRects");
    CheckColor(Checked(color, "WebCore::DisplayList::DrawFocusRingRects::DrawFocusRingRects"), "WebCore::DisplayList::DrawFocusRingRects::DrawFocusRingRects");
    auto& item = Checked(self, "WebCore::DisplayList::DrawFocusRingRects::DrawFocusRingRects");
    ConstructDrawingItem(item.item, item.extent, &DrawFocusRingRectsTable, ItemType::DrawFocusRingRects);
    CopyVector(item.rects, rects, "WebCore::DisplayList::DrawFocusRingRects::DrawFocusRingRects");
    item.width = width;
    item.offset = offset;
    item.color = RetainColor(*color);
}

void DestroyDrawFocusRingRects(DrawFocusRingRects* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawFocusRingRects::~DrawFocusRingRects");
    ReleaseColor(item.color);
    DestroyVector(item.rects);
    DestroyItem(item.item);
}

void APS5_VABI DrawImageCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList9DrawImageD1Ev(static_cast<DrawImage*>(self));
}

void APS5_VABI DrawImageDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList9DrawImageD0Ev(static_cast<DrawImage*>(self));
}

OptionalFloatRect APS5_VABI DrawImageLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawImage*>(self);
    return Bounds(item.destination);
}

const DrawingItemVtable DrawImageTable{{DrawImageCompleteDestructor, DrawImageDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawImageLocalBounds, DefaultGlobalBounds};

void ConstructDrawImage(DrawImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const ImagePaintingOptions* imagePaintingOptions) {
    Checked(image, "WebCore::DisplayList::DrawImage::DrawImage");
    Checked(destination, "WebCore::DisplayList::DrawImage::DrawImage");
    Checked(source, "WebCore::DisplayList::DrawImage::DrawImage");
    Checked(imagePaintingOptions, "WebCore::DisplayList::DrawImage::DrawImage");
    auto& item = Checked(self, "WebCore::DisplayList::DrawImage::DrawImage");
    ConstructDrawingItem(item.item, item.extent, &DrawImageTable, ItemType::DrawImage);
    item.image = RetainImage(image);
    item.destination = *destination;
    item.source = *source;
    item.imagePaintingOptions = *imagePaintingOptions;
}

void DestroyDrawImage(DrawImage* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawImage::~DrawImage");
    ReleaseImage(item.image);
    DestroyItem(item.item);
}

void APS5_VABI DrawTiledImageCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList14DrawTiledImageD1Ev(static_cast<DrawTiledImage*>(self));
}

void APS5_VABI DrawTiledImageDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList14DrawTiledImageD0Ev(static_cast<DrawTiledImage*>(self));
}

OptionalFloatRect APS5_VABI DrawTiledImageLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawTiledImage*>(self);
    return Bounds(item.destination);
}

const DrawingItemVtable DrawTiledImageTable{{DrawTiledImageCompleteDestructor, DrawTiledImageDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawTiledImageLocalBounds, DefaultGlobalBounds};

void ConstructDrawTiledImage(DrawTiledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatPoint* source, const WebCore::FloatSize* tileSize, const WebCore::FloatSize* spacing, const ImagePaintingOptions* imagePaintingOptions) {
    Checked(image, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    Checked(destination, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    Checked(source, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    Checked(tileSize, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    Checked(spacing, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    Checked(imagePaintingOptions, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    auto& item = Checked(self, "WebCore::DisplayList::DrawTiledImage::DrawTiledImage");
    ConstructDrawingItem(item.item, item.extent, &DrawTiledImageTable, ItemType::DrawTiledImage);
    item.image = RetainImage(image);
    item.destination = *destination;
    item.source = *source;
    item.tileSize = *tileSize;
    item.spacing = *spacing;
    item.imagePaintingOptions = *imagePaintingOptions;
}

void DestroyDrawTiledImage(DrawTiledImage* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawTiledImage::~DrawTiledImage");
    ReleaseImage(item.image);
    DestroyItem(item.item);
}

void APS5_VABI DrawTiledScaledImageCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList20DrawTiledScaledImageD1Ev(static_cast<DrawTiledScaledImage*>(self));
}

void APS5_VABI DrawTiledScaledImageDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList20DrawTiledScaledImageD0Ev(static_cast<DrawTiledScaledImage*>(self));
}

OptionalFloatRect APS5_VABI DrawTiledScaledImageLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawTiledScaledImage*>(self);
    return Bounds(item.destination);
}

const DrawingItemVtable DrawTiledScaledImageTable{{DrawTiledScaledImageCompleteDestructor, DrawTiledScaledImageDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawTiledScaledImageLocalBounds, DefaultGlobalBounds};

void ConstructDrawTiledScaledImage(DrawTiledScaledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const WebCore::FloatSize* tileScaleFactor, int hRule, int vRule, const ImagePaintingOptions* imagePaintingOptions) {
    Checked(image, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    Checked(destination, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    Checked(source, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    Checked(tileScaleFactor, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    Checked(imagePaintingOptions, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    auto& item = Checked(self, "WebCore::DisplayList::DrawTiledScaledImage::DrawTiledScaledImage");
    ConstructDrawingItem(item.item, item.extent, &DrawTiledScaledImageTable, ItemType::DrawTiledScaledImage);
    item.image = RetainImage(image);
    item.destination = *destination;
    item.source = *source;
    item.tileScaleFactor = *tileScaleFactor;
    item.hRule = hRule;
    item.vRule = vRule;
    item.imagePaintingOptions = *imagePaintingOptions;
}

void DestroyDrawTiledScaledImage(DrawTiledScaledImage* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawTiledScaledImage::~DrawTiledScaledImage");
    ReleaseImage(item.image);
    DestroyItem(item.item);
}

void APS5_VABI DrawPatternCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawPatternD1Ev(static_cast<DrawPattern*>(self));
}

void APS5_VABI DrawPatternDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11DrawPatternD0Ev(static_cast<DrawPattern*>(self));
}

OptionalFloatRect APS5_VABI DrawPatternLocalBounds(const void* self, const void*) {
    const auto& item = *static_cast<const DrawPattern*>(self);
    return Bounds(item.destination);
}

const DrawingItemVtable DrawPatternTable{{DrawPatternCompleteDestructor, DrawPatternDeletingDestructor, ApplyWithoutGraphicsContext, IsDrawingItem}, DrawPatternLocalBounds, DefaultGlobalBounds};

void ConstructDrawPattern(DrawPattern* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* tileRect, const WebCore::AffineTransform* patternTransform, const WebCore::FloatPoint* phase, const WebCore::FloatSize* spacing, const ImagePaintingOptions* options) {
    Checked(image, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(destination, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(tileRect, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(patternTransform, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(phase, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(spacing, "WebCore::DisplayList::DrawPattern::DrawPattern");
    Checked(options, "WebCore::DisplayList::DrawPattern::DrawPattern");
    auto& item = Checked(self, "WebCore::DisplayList::DrawPattern::DrawPattern");
    ConstructDrawingItem(item.item, item.extent, &DrawPatternTable, ItemType::DrawPattern);
    item.image = RetainImage(image);
    item.patternTransform = *patternTransform;
    item.tileRect = *tileRect;
    item.destination = *destination;
    item.phase = *phase;
    item.spacing = *spacing;
    item.options = *options;
}

void DestroyDrawPattern(DrawPattern* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawPattern::~DrawPattern");
    ReleaseImage(item.image);
    DestroyItem(item.item);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(FillRectWithColor* self, const WebCore::FloatRect* rect, const Color* color) {
    ConstructFillRectWithColor(self, rect, color);
}

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorC2ERKNS_9FloatRectERKNS_5ColorE(FillRectWithColor* self, const WebCore::FloatRect* rect, const Color* color) {
    ConstructFillRectWithColor(self, rect, color);
}

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD0Ev(FillRectWithColor* self) {
    DestroyFillRectWithColor(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD1Ev(FillRectWithColor* self) {
    DestroyFillRectWithColor(self);
}

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD2Ev(FillRectWithColor* self) {
    DestroyFillRectWithColor(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectC1ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(FillCompositedRect* self, const WebCore::FloatRect* rect, const Color* color, std::uint8_t op, std::uint8_t blendMode) {
    ConstructFillCompositedRect(self, rect, color, op, blendMode);
}

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectC2ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(FillCompositedRect* self, const WebCore::FloatRect* rect, const Color* color, std::uint8_t op, std::uint8_t blendMode) {
    ConstructFillCompositedRect(self, rect, color, op, blendMode);
}

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD0Ev(FillCompositedRect* self) {
    DestroyFillCompositedRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD1Ev(FillCompositedRect* self) {
    DestroyFillCompositedRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD2Ev(FillCompositedRect* self) {
    DestroyFillCompositedRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectC1ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(FillRoundedRect* self, const WebCore::FloatRoundedRect* rect, const Color* color, std::uint8_t blendMode) {
    ConstructFillRoundedRect(self, rect, color, blendMode);
}

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectC2ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(FillRoundedRect* self, const WebCore::FloatRoundedRect* rect, const Color* color, std::uint8_t blendMode) {
    ConstructFillRoundedRect(self, rect, color, blendMode);
}

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD0Ev(FillRoundedRect* self) {
    DestroyFillRoundedRect(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD1Ev(FillRoundedRect* self) {
    DestroyFillRoundedRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD2Ev(FillRoundedRect* self) {
    DestroyFillRoundedRect(self);
}

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC1ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(FillRectWithRoundedHole* self, const WebCore::FloatRect* rect, const WebCore::FloatRoundedRect* roundedHoleRect, const Color* color) {
    ConstructFillRectWithRoundedHole(self, rect, roundedHoleRect, color);
}

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC2ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(FillRectWithRoundedHole* self, const WebCore::FloatRect* rect, const WebCore::FloatRoundedRect* roundedHoleRect, const Color* color) {
    ConstructFillRectWithRoundedHole(self, rect, roundedHoleRect, color);
}

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD0Ev(FillRectWithRoundedHole* self) {
    DestroyFillRectWithRoundedHole(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD1Ev(FillRectWithRoundedHole* self) {
    DestroyFillRectWithRoundedHole(self);
}

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD2Ev(FillRectWithRoundedHole* self) {
    DestroyFillRectWithRoundedHole(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsC1ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(DrawFocusRingRects* self, const Vector<WebCore::FloatRect>* rects, float width, float offset, const Color* color) {
    ConstructDrawFocusRingRects(self, rects, width, offset, color);
}

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsC2ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(DrawFocusRingRects* self, const Vector<WebCore::FloatRect>* rects, float width, float offset, const Color* color) {
    ConstructDrawFocusRingRects(self, rects, width, offset, color);
}

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD0Ev(DrawFocusRingRects* self) {
    DestroyDrawFocusRingRects(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD1Ev(DrawFocusRingRects* self) {
    DestroyDrawFocusRingRects(self);
}

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD2Ev(DrawFocusRingRects* self) {
    DestroyDrawFocusRingRects(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(DrawImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawImage(self, image, destination, source, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(DrawImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawImage(self, image, destination, source, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD0Ev(DrawImage* self) {
    DestroyDrawImage(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD1Ev(DrawImage* self) {
    DestroyDrawImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD2Ev(DrawImage* self) {
    DestroyDrawImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageC1ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(DrawTiledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatPoint* source, const WebCore::FloatSize* tileSize, const WebCore::FloatSize* spacing, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawTiledImage(self, image, destination, source, tileSize, spacing, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageC2ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(DrawTiledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatPoint* source, const WebCore::FloatSize* tileSize, const WebCore::FloatSize* spacing, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawTiledImage(self, image, destination, source, tileSize, spacing, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD0Ev(DrawTiledImage* self) {
    DestroyDrawTiledImage(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD1Ev(DrawTiledImage* self) {
    DestroyDrawTiledImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD2Ev(DrawTiledImage* self) {
    DestroyDrawTiledImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(DrawTiledScaledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const WebCore::FloatSize* tileScaleFactor, int hRule, int vRule, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawTiledScaledImage(self, image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(DrawTiledScaledImage* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const WebCore::FloatSize* tileScaleFactor, int hRule, int vRule, const ImagePaintingOptions* imagePaintingOptions) {
    ConstructDrawTiledScaledImage(self, image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions);
}

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD0Ev(DrawTiledScaledImage* self) {
    DestroyDrawTiledScaledImage(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD1Ev(DrawTiledScaledImage* self) {
    DestroyDrawTiledScaledImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD2Ev(DrawTiledScaledImage* self) {
    DestroyDrawTiledScaledImage(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(DrawPattern* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* tileRect, const WebCore::AffineTransform* patternTransform, const WebCore::FloatPoint* phase, const WebCore::FloatSize* spacing, const ImagePaintingOptions* options) {
    ConstructDrawPattern(self, image, destination, tileRect, patternTransform, phase, spacing, options);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(DrawPattern* self, Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* tileRect, const WebCore::AffineTransform* patternTransform, const WebCore::FloatPoint* phase, const WebCore::FloatSize* spacing, const ImagePaintingOptions* options) {
    ConstructDrawPattern(self, image, destination, tileRect, patternTransform, phase, spacing, options);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD0Ev(DrawPattern* self) {
    DestroyDrawPattern(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD1Ev(DrawPattern* self) {
    DestroyDrawPattern(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD2Ev(DrawPattern* self) {
    DestroyDrawPattern(self);
}

}
