// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/dom/DOMRectList.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/DOMRectListExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <new>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::DOMRect;
using NKWebKit::WebCoreAbi::DOMRectList;
using NKWebKit::WtfAbi::Vector;

const char* const className = "WebCore::DOMRectList";

DOMRectList* Checked(DOMRectList* self) {
    if (!self) throw std::invalid_argument(std::string(className) + ": null this");
    return self;
}

DOMRect* CreateDOMRect(double x, double y, double width, double height) {
    auto* rect = static_cast<DOMRect*>(_ZN3WTF10fastMallocEm(sizeof(DOMRect)));
    rect->wrapper = nullptr;
    rect->refCount = 1;
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
    return rect;
}

DOMRect* CreateBoundingBox(const WebCore::FloatQuad& quad) {
    WebCore::FloatRect box = _ZNK7WebCore9FloatQuad11boundingBoxEv(&quad);
    return CreateDOMRect(box.m_location.m_x, box.m_location.m_y, box.m_size.m_width, box.m_size.m_height);
}

DOMRect* CreateFromRect(const WebCore::FloatRect& rect) {
    return CreateDOMRect(rect.m_location.m_x, rect.m_location.m_y, rect.m_size.m_width, rect.m_size.m_height);
}

void ReleaseDOMRect(DOMRect* rect) {
    if (--rect->refCount == 0) {
        _ZN3WTF8fastFreeEPv(rect);
    }
}

template <typename T>
void Construct(DOMRectList* self, const Vector<T>* source, DOMRect* (*createItem)(const T&)) {
    Checked(self);
    if (!source) throw std::invalid_argument(std::string(className) + ": null source");
    if (source->size && !source->buffer) throw std::invalid_argument(std::string(className) + ": null item buffer");
    new (self) DOMRectList{};
    self->refCount = 1;
    if (source->size) {
        self->items.buffer = static_cast<DOMRect**>(_ZN3WTF10fastMallocEm(static_cast<std::size_t>(source->size) * sizeof(DOMRect*)));
        self->items.capacity = source->size;
        for (unsigned index = 0; index < source->size; ++index) {
            self->items.buffer[index] = createItem(source->buffer[index]);
        }
        self->items.size = source->size;
    }
}

void Destroy(DOMRectList* self) {
    auto* list = Checked(self);
    for (unsigned index = 0; index < list->items.size; ++index) {
        ReleaseDOMRect(list->items.buffer[index]);
    }
    list->items.clear();
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(DOMRectList* self, const Vector<WebCore::FloatQuad>* quads) {
    Construct(self, quads, CreateBoundingBox);
}

void APS5_VABI _ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(DOMRectList* self, const Vector<WebCore::FloatRect>* rects) {
    Construct(self, rects, CreateFromRect);
}

void APS5_VABI _ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(DOMRectList* self, const Vector<WebCore::FloatQuad>* quads) {
    Construct(self, quads, CreateBoundingBox);
}

void APS5_VABI _ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(DOMRectList* self, const Vector<WebCore::FloatRect>* rects) {
    Construct(self, rects, CreateFromRect);
}

void APS5_VABI _ZN7WebCore11DOMRectListD1Ev(DOMRectList* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore11DOMRectListD2Ev(DOMRectList* self) {
    Destroy(self);
}

}
