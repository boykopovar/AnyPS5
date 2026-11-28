// Derived from Source/WebCore/svg/SVGPathStringBuilder.h@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

enum PathCoordinateMode : int {
    AbsoluteCoordinates,
    RelativeCoordinates,
};

struct SVGPathStringBuilder {
    const void* const* vtable;
    NKWebKit::WtfAbi::StringBuilder stringBuilder;
};

static_assert(sizeof(WebCore::FloatPoint) == 8);
static_assert(sizeof(SVGPathStringBuilder) == 48);
static_assert(alignof(SVGPathStringBuilder) == 8);
static_assert(offsetof(SVGPathStringBuilder, vtable) == 0);
static_assert(offsetof(SVGPathStringBuilder, stringBuilder) == 8);

}  // namespace NKWebKit::WebCoreAbi
