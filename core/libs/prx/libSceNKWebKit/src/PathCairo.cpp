// Derived from WebKit/WebKit/Source/WebCore/platform/graphics/cairo/PathCairo.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
    Copyright (C) 2007 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005, 2007 Apple Inc. All Rights reserved.
                  2007 Alp Toker <alp@atoker.com>
                  2008 Dirk Schulze <krit@webkit.org>
                  2011, 2020 Igalia S.L.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PathExports.hpp"

extern "C" {

bool APS5_VABI _ZNK7WebCore4Path6isNullEv(const NKWebKit::WebCoreAbi::Path* self) {
    return self->path == nullptr;
}

}
