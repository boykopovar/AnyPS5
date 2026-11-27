// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/texmap/TextureMapperFPSCounter.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012, 2013 Company 100, Inc.
    Copyright (C) 2012, 2013 basysKom GmbH

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "prx/libSceNKWebKit/include/TextureMapperFPSCounterExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstring>
#include <stdexcept>

namespace {

void Construct(NKWebKit::TextureMapperAbi::TextureMapperFPSCounter* self) {
    if (!self) throw std::invalid_argument("null WebCore::TextureMapperFPSCounter");
    *self = {false, 0, 0, 0, 0};
    const char* environment = getenv_nid_postfix("WEBKIT_SHOW_FPS");
    const NKWebKit::WtfAbi::String showFPSEnvironment = environment
        ? _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(environment), static_cast<unsigned>(std::strlen(environment)))
        : NKWebKit::WtfAbi::String();
    bool ok = false;
    self->fpsInterval = _ZNK3WTF6String8toDoubleEPb(&showFPSEnvironment, &ok);
    if (ok && self->fpsInterval) {
        self->isShowingFPS = true;
        self->fpsTimestamp = _ZN3WTF13MonotonicTime3nowEv().m_value;
    }
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore23TextureMapperFPSCounterC1Ev(NKWebKit::TextureMapperAbi::TextureMapperFPSCounter* self) {
    Construct(self);
}

void APS5_VABI _ZN7WebCore23TextureMapperFPSCounterC2Ev(NKWebKit::TextureMapperAbi::TextureMapperFPSCounter* self) {
    Construct(self);
}

}
