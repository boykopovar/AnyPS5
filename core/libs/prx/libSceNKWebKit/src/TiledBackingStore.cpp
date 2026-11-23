// Derived from Source/WebCore/platform/graphics/texmap/coordinated/TiledBackingStore.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 Copyright (C) 2010-2012 Nokia Corporation and/or its subsidiary(-ies)

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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/EnclosingRectExports.hpp"
#include "prx/libSceNKWebKit/include/TiledBackingStoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <stdexcept>

extern "C" {

WebCore::IntRect APS5_VABI _ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE(const NKWebKit::TiledBackingStoreAbi::TiledBackingStore* self, const WebCore::IntRect* rect) {
    if (!self || !rect) throw std::invalid_argument("WebCore::TiledBackingStore::mapToContents: null argument");
    const float contentsScale = self->contentsScale;
    return _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(WebCore::FloatRect{
        {rect->m_location.m_x / contentsScale, rect->m_location.m_y / contentsScale},
        {rect->m_size.m_width / contentsScale, rect->m_size.m_height / contentsScale}});
}

}
