/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 ProFUSION embedded systems
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
 *
 * Derived from Source/WebCore/platform/graphics/cairo/NativeImageCairo.cpp@c55364d53f971f63a709976f168d932d558b8909
 * https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/cairo/NativeImageCairo.cpp
 * Derived from Source/WebCore/platform/graphics/cairo/CairoUtilities.cpp@c55364d53f971f63a709976f168d932d558b8909
 * https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/cairo/CairoUtilities.cpp
 */

#include "prx/libSceNKWebKit/include/NativeImageExports.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int kCairoSurfaceTypeImage = 0;

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

CairoSurface* ReadGuestSurfacePointer(const NKWebKit::WtfAbi::RefPtr<CairoSurface>* image, const char* caller) {
    if (!image)
        NotImplemented_nid_no_patch(caller);
    CairoSurface* surface = nullptr;
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(&surface, &image->ptr, sizeof(surface)))
        NotImplemented_nid_no_patch(caller);
    return surface;
}

void ProbeGuestSurface(const CairoSurface* surface, const char* caller) {
    std::array<unsigned char, 32> probe{};
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(probe.data(), surface, probe.size()))
        NotImplemented_nid_no_patch(caller);
}

}  // namespace

extern "C" {

WebCore::IntSize APS5_VABI _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(
    const NKWebKit::WtfAbi::RefPtr<CairoSurface>* image) {
    CairoSurface* surface = ReadGuestSurfacePointer(image, __func__);
    if (!surface)
        return WebCore::IntSize{0, 0};
    ProbeGuestSurface(surface, __func__);
    if (cairo_surface_get_type(surface) == kCairoSurfaceTypeImage)
        return WebCore::IntSize{cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface)};
    return WebCore::IntSize{0, 0};
}

}
