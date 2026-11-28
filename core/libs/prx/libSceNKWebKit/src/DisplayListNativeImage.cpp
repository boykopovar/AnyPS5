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

#include "prx/libSceNKWebKit/include/DisplayListNativeImageExports.hpp"

namespace {

using NKWebKit::DisplayListAbi::Bounds;
using NKWebKit::DisplayListAbi::ConstructDrawingItem;
using NKWebKit::DisplayListAbi::DefaultGlobalBounds;
using NKWebKit::DisplayListAbi::DeleteItem;
using NKWebKit::DisplayListAbi::DestroyItem;
using NKWebKit::DisplayListAbi::DrawingItemVtable;
using NKWebKit::DisplayListAbi::IsDrawingItem;
using NKWebKit::DisplayListAbi::ItemType;
using NKWebKit::DisplayListAbi::OptionalFloatRect;
using NKWebKit::DisplayListNativeImageAbi::DrawNativeImage;

DrawNativeImage& Checked(DrawNativeImage* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires an object");
    return *self;
}

void APS5_VABI DrawNativeImageApply(const void*, void*) {}

OptionalFloatRect APS5_VABI DrawNativeImageLocalBounds(const void* self, const void*) {
    const auto& item = Checked(static_cast<DrawNativeImage*>(const_cast<void*>(self)), "WebCore::DisplayList::DrawNativeImage::localBounds");
    return Bounds(item.destinationRect);
}

void APS5_VABI DrawNativeImageCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList15DrawNativeImageD1Ev(static_cast<DrawNativeImage*>(self));
}

void APS5_VABI DrawNativeImageDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList15DrawNativeImageD0Ev(static_cast<DrawNativeImage*>(self));
}

const DrawingItemVtable DrawNativeImageTable{
    {DrawNativeImageCompleteDestructor, DrawNativeImageDeletingDestructor, DrawNativeImageApply, IsDrawingItem},
    DrawNativeImageLocalBounds,
    DefaultGlobalBounds,
};

void ConstructDrawNativeImage(DrawNativeImage& self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destinationRect, const WebCore::FloatRect* sourceRect, const NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions* options) {
    if (!image || !imageSize || !destinationRect || !sourceRect || !options)
        throw std::invalid_argument("WebCore::DisplayList::DrawNativeImage::DrawNativeImage requires an image, size, rectangles and options");
    ConstructDrawingItem(self.drawingItem.item, self.drawingItem.extent, &DrawNativeImageTable, ItemType::DrawNativeImage);
    self.imageSize = *imageSize;
    self.destinationRect = *destinationRect;
    self.sourceRect = *sourceRect;
    self.options = *options;
}

void DestroyDrawNativeImage(DrawNativeImage& self) {
    DestroyItem(self.drawingItem.item);
}

}

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageC1ERKN3WTF6RefPtrI14_cairo_surfaceNS2_13DumbPtrTraitsIS4_EEEERKNS_9FloatSizeERKNS_9FloatRectESF_RKNS_20ImagePaintingOptionsE(DrawNativeImage* self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destinationRect, const WebCore::FloatRect* sourceRect, const NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions* options) {
    ConstructDrawNativeImage(Checked(self, "WebCore::DisplayList::DrawNativeImage::DrawNativeImage"), image, imageSize, destinationRect, sourceRect, options);
}

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageC2ERKN3WTF6RefPtrI14_cairo_surfaceNS2_13DumbPtrTraitsIS4_EEEERKNS_9FloatSizeERKNS_9FloatRectESF_RKNS_20ImagePaintingOptionsE(DrawNativeImage* self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destinationRect, const WebCore::FloatRect* sourceRect, const NKWebKit::DisplayListNativeImageAbi::ImagePaintingOptions* options) {
    ConstructDrawNativeImage(Checked(self, "WebCore::DisplayList::DrawNativeImage::DrawNativeImage"), image, imageSize, destinationRect, sourceRect, options);
}

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD1Ev(DrawNativeImage* self) {
    DestroyDrawNativeImage(Checked(self, "WebCore::DisplayList::DrawNativeImage::~DrawNativeImage"));
}

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD2Ev(DrawNativeImage* self) {
    DestroyDrawNativeImage(Checked(self, "WebCore::DisplayList::DrawNativeImage::~DrawNativeImage"));
}

void APS5_VABI _ZN7WebCore11DisplayList15DrawNativeImageD0Ev(DrawNativeImage* self) {
    auto& item = Checked(self, "WebCore::DisplayList::DrawNativeImage::~DrawNativeImage");
    DestroyDrawNativeImage(item);
    DeleteItem(&item);
}

}
