/* Derived from Source/WebCore/platform/graphics/RoundedRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba */
/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Xidorn Quan (quanxunzhen@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS0_5RadiiE(WebCore::RoundedRect* self, const WebCore::LayoutRect& rect, const WebCore::RoundedRect::Radii& radii) {
    self->m_rect = rect;
    self->m_radii = radii;
}

void APS5_VABI _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS0_5RadiiE(WebCore::RoundedRect* self, const WebCore::LayoutRect& rect, const WebCore::RoundedRect::Radii& radii) {
    _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS0_5RadiiE(self, rect, radii);
}

}
