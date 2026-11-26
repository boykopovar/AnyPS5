// Derived from Source/WebCore/platform/graphics/Path.cpp and Source/WebCore/platform/graphics/cairo/PathCairo.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 *                     2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/PathExports.hpp"
#include "prx/libcairo/include/Bridge.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::PathAbi::Path;
using NKWebKit::PathAbi::PathElement;
using NKWebKit::PathAbi::PathElementFunction;

constexpr std::int32_t CairoFormatA8 = 2;
constexpr float CircleControlPoint = 0.447715f;
constexpr unsigned InitialElementCapacity = 16;

Path& Checked(Path* path, const char* function) {
    if (!path) throw std::invalid_argument(std::string(function) + " requires a path");
    return *path;
}

const Path& Checked(const Path* path, const char* function) {
    if (!path) throw std::invalid_argument(std::string(function) + " requires a path");
    return *path;
}

void FreeElements(Path& path) {
    if (path.m_elements.buffer) _ZN3WTF8fastFreeEPv(path.m_elements.buffer);
    path.m_elements.buffer = nullptr;
    path.m_elements.capacity = 0;
    path.m_elements.size = 0;
}

void CheckElements(const Path& path, const char* function) {
    if (path.m_elements.size > path.m_elements.capacity || (path.m_elements.capacity && !path.m_elements.buffer))
        throw std::invalid_argument(std::string(function) + " requires valid path elements");
}

void CopyElements(Path& to, const Path& from, const char* function) {
    CheckElements(from, function);
    FreeElements(to);
    if (!from.m_elements.size) return;
    if (from.m_elements.size > std::numeric_limits<std::uint32_t>::max() / sizeof(PathElement))
        throw std::length_error(std::string(function) + " has too many path elements");
    const std::size_t bytes = std::size_t(from.m_elements.size) * sizeof(PathElement);
    to.m_elements.buffer = static_cast<PathElement*>(_ZN3WTF10fastMallocEm(bytes));
    to.m_elements.capacity = from.m_elements.size;
    to.m_elements.size = from.m_elements.size;
    std::memcpy(to.m_elements.buffer, from.m_elements.buffer, bytes);
}

void AppendElement(Path& path, PathElement::Type type, const WebCore::FloatPoint* points, unsigned count, const char* function) {
    if (!path.m_elementsEngaged) return;
    if (path.m_elements.size == path.m_elements.capacity) {
        const unsigned capacity = path.m_elements.capacity ? path.m_elements.capacity * 2 : InitialElementCapacity;
        if (capacity < path.m_elements.capacity || capacity > std::numeric_limits<std::uint32_t>::max() / sizeof(PathElement))
            throw std::length_error(std::string(function) + " has too many path elements");
        auto* buffer = static_cast<PathElement*>(_ZN3WTF10fastMallocEm(std::size_t(capacity) * sizeof(PathElement)));
        if (path.m_elements.size) std::memcpy(buffer, path.m_elements.buffer, std::size_t(path.m_elements.size) * sizeof(PathElement));
        if (path.m_elements.buffer) _ZN3WTF8fastFreeEPv(path.m_elements.buffer);
        path.m_elements.buffer = buffer;
        path.m_elements.capacity = capacity;
    }
    PathElement& element = path.m_elements.buffer[path.m_elements.size++];
    element = {};
    for (unsigned i = 0; i < count; ++i) element.points[i] = points[i];
    element.type = type;
}

CairoContext* EnsureCairo(Path& path, const char* function) {
    if (path.m_path) return static_cast<CairoContext*>(path.m_path);
    CairoSurface* surface = CairoImageSurfaceCreate_nid_no_patch(CairoFormatA8, 1, 1);
    if (!surface) throw std::runtime_error(std::string(function) + " could not create a cairo surface");
    path.m_path = CairoCreate_nid_no_patch(surface);
    CairoSurfaceDestroy_nid_no_patch(surface);
    if (!path.m_path) throw std::runtime_error(std::string(function) + " could not create a cairo context");
    return static_cast<CairoContext*>(path.m_path);
}

void CopyCairo(Path& to, const Path& from, const char* function) {
    CairoContext* context = EnsureCairo(to, function);
    CairoMatrix matrix{};
    CairoGetMatrix_nid_no_patch(static_cast<CairoContext*>(from.m_path), &matrix);
    CairoSetMatrix_nid_no_patch(context, &matrix);
    CairoPath* copy = CairoCopyPath_nid_no_patch(static_cast<CairoContext*>(from.m_path));
    if (!copy) throw std::runtime_error(std::string(function) + " could not copy a cairo path");
    CairoAppendPath_nid_no_patch(context, copy);
    CairoPathDestroy_nid_no_patch(copy);
}

void Initialize(Path& path) {
    path.m_path = nullptr;
    path.m_elements.buffer = nullptr;
    path.m_elements.capacity = 0;
    path.m_elements.size = 0;
    path.m_elementsEngaged = true;
}

void Destroy(Path& path) {
    if (path.m_path) CairoDestroy_nid_no_patch(static_cast<CairoContext*>(path.m_path));
    path.m_path = nullptr;
    FreeElements(path);
    path.m_elementsEngaged = false;
}

void CopyPath(Path& to, const Path& from, const char* function) {
    Initialize(to);
    if (!from.m_path) return;
    CopyCairo(to, from, function);
    to.m_elementsEngaged = from.m_elementsEngaged;
    if (from.m_elementsEngaged) CopyElements(to, from, function);
}

void MovePath(Path& to, Path& from) {
    to.m_path = std::exchange(from.m_path, nullptr);
    to.m_elements = std::exchange(from.m_elements, {nullptr, 0, 0});
    to.m_elementsEngaged = from.m_elementsEngaged;
}

Path& AssignPath(Path& to, const Path& from) {
    if (&to == &from) return to;
    if (!from.m_path) {
        if (to.m_path) CairoDestroy_nid_no_patch(static_cast<CairoContext*>(to.m_path));
        to.m_path = nullptr;
        FreeElements(to);
        to.m_elementsEngaged = true;
        return to;
    }
    Path copy;
    try {
        CopyPath(copy, from, "WebCore::Path::operator=");
    } catch (...) {
        Destroy(copy);
        throw;
    }
    if (to.m_path) CairoDestroy_nid_no_patch(static_cast<CairoContext*>(to.m_path));
    FreeElements(to);
    MovePath(to, copy);
    return to;
}

Path& MoveAssignPath(Path& to, Path& from) {
    if (&to == &from) return to;
    if (to.m_path) CairoDestroy_nid_no_patch(static_cast<CairoContext*>(to.m_path));
    FreeElements(to);
    MovePath(to, from);
    return to;
}

void Clear(Path& path) {
    if (!path.m_path) {
        FreeElements(path);
        path.m_elementsEngaged = true;
        return;
    }
    CairoIdentityMatrix_nid_no_patch(static_cast<CairoContext*>(path.m_path));
    CairoNewPath_nid_no_patch(static_cast<CairoContext*>(path.m_path));
    FreeElements(path);
    path.m_elementsEngaged = true;
}

void AddRect(Path& path, const WebCore::FloatRect& rect) {
    CairoContext* context = EnsureCairo(path, "WebCore::Path::addRoundedRect");
    CairoRectangle_nid_no_patch(context, rect.x(), rect.y(), rect.width(), rect.height());
    const WebCore::FloatPoint topLeft{rect.x(), rect.y()};
    const WebCore::FloatPoint topRight{rect.x() + rect.width(), rect.y()};
    const WebCore::FloatPoint bottomRight{rect.x() + rect.width(), rect.y() + rect.height()};
    const WebCore::FloatPoint bottomLeft{rect.x(), rect.y() + rect.height()};
    AppendElement(path, PathElement::Type::MoveToPoint, &topLeft, 1, "WebCore::Path::addRoundedRect");
    AppendElement(path, PathElement::Type::AddLineToPoint, &topRight, 1, "WebCore::Path::addRoundedRect");
    AppendElement(path, PathElement::Type::AddLineToPoint, &bottomRight, 1, "WebCore::Path::addRoundedRect");
    AppendElement(path, PathElement::Type::AddLineToPoint, &bottomLeft, 1, "WebCore::Path::addRoundedRect");
    AppendElement(path, PathElement::Type::CloseSubpath, nullptr, 0, "WebCore::Path::addRoundedRect");
    double x = 0;
    double y = 0;
    if (CairoHasCurrentPoint_nid_no_patch(context)) {
        CairoGetCurrentPoint_nid_no_patch(context, &x, &y);
        const WebCore::FloatPoint current{static_cast<float>(x), static_cast<float>(y)};
        AppendElement(path, PathElement::Type::MoveToPoint, &current, 1, "WebCore::Path::addRoundedRect");
    }
}

bool IsEmpty(const WebCore::FloatRect& rect) {
    return rect.width() <= 0 || rect.height() <= 0;
}

bool IsRenderable(const WebCore::FloatRoundedRect& roundedRect) {
    const auto& radii = roundedRect.m_radii;
    return radii.m_topLeft.m_width >= 0 && radii.m_topLeft.m_height >= 0
        && radii.m_bottomLeft.m_width >= 0 && radii.m_bottomLeft.m_height >= 0
        && radii.m_topRight.m_width >= 0 && radii.m_topRight.m_height >= 0
        && radii.m_bottomRight.m_width >= 0 && radii.m_bottomRight.m_height >= 0
        && radii.m_topLeft.m_width + radii.m_topRight.m_width <= roundedRect.m_rect.width()
        && radii.m_bottomLeft.m_width + radii.m_bottomRight.m_width <= roundedRect.m_rect.width()
        && radii.m_topLeft.m_height + radii.m_bottomLeft.m_height <= roundedRect.m_rect.height()
        && radii.m_topRight.m_height + radii.m_bottomRight.m_height <= roundedRect.m_rect.height();
}

void AddBeziersForRoundedRect(Path& path, const WebCore::FloatRect& rect, const WebCore::FloatSize& topLeftRadius,
                              const WebCore::FloatSize& topRightRadius, const WebCore::FloatSize& bottomLeftRadius,
                              const WebCore::FloatSize& bottomRightRadius) {
    const float maxX = rect.x() + rect.width();
    const float maxY = rect.y() + rect.height();
    const WebCore::FloatPoint start{rect.x() + topLeftRadius.m_width, rect.y()};
    _ZN7WebCore4Path6moveToERKNS_10FloatPointE(&path, &start);

    const WebCore::FloatPoint topRightStart{maxX - topRightRadius.m_width, rect.y()};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path, &topRightStart);
    if (topRightRadius.m_width > 0 || topRightRadius.m_height > 0) {
        const WebCore::FloatPoint control1{maxX - topRightRadius.m_width * CircleControlPoint, rect.y()};
        const WebCore::FloatPoint control2{maxX, rect.y() + topRightRadius.m_height * CircleControlPoint};
        const WebCore::FloatPoint end{maxX, rect.y() + topRightRadius.m_height};
        _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(&path, &control1, &control2, &end);
    }

    const WebCore::FloatPoint bottomRightStart{maxX, maxY - bottomRightRadius.m_height};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path, &bottomRightStart);
    if (bottomRightRadius.m_width > 0 || bottomRightRadius.m_height > 0) {
        const WebCore::FloatPoint control1{maxX, maxY - bottomRightRadius.m_height * CircleControlPoint};
        const WebCore::FloatPoint control2{maxX - bottomRightRadius.m_width * CircleControlPoint, maxY};
        const WebCore::FloatPoint end{maxX - bottomRightRadius.m_width, maxY};
        _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(&path, &control1, &control2, &end);
    }

    const WebCore::FloatPoint bottomLeftStart{rect.x() + bottomLeftRadius.m_width, maxY};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path, &bottomLeftStart);
    if (bottomLeftRadius.m_width > 0 || bottomLeftRadius.m_height > 0) {
        const WebCore::FloatPoint control1{rect.x() + bottomLeftRadius.m_width * CircleControlPoint, maxY};
        const WebCore::FloatPoint control2{rect.x(), maxY - bottomLeftRadius.m_height * CircleControlPoint};
        const WebCore::FloatPoint end{rect.x(), maxY - bottomLeftRadius.m_height};
        _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(&path, &control1, &control2, &end);
    }

    const WebCore::FloatPoint topLeftEnd{rect.x(), rect.y() + topLeftRadius.m_height};
    _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(&path, &topLeftEnd);
    if (topLeftRadius.m_width > 0 || topLeftRadius.m_height > 0) {
        const WebCore::FloatPoint control1{rect.x(), rect.y() + topLeftRadius.m_height * CircleControlPoint};
        const WebCore::FloatPoint control2{rect.x() + topLeftRadius.m_width * CircleControlPoint, rect.y()};
        const WebCore::FloatPoint end{rect.x() + topLeftRadius.m_width, rect.y()};
        _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(&path, &control1, &control2, &end);
    }

    _ZN7WebCore4Path12closeSubpathEv(&path);
}

void Invoke(const PathElementFunction& function, const PathElement& element) {
    auto* wrapper = function.callableWrapper;
    if (!wrapper || !wrapper->vtable) throw std::invalid_argument("WebCore::Path::apply requires a callable");
    wrapper->vtable->call(wrapper, &element);
}

void ApplyCairoPath(const Path& path, const PathElementFunction& function) {
    std::unique_ptr<CairoPath, void (*)(CairoPath*)> copy(CairoCopyPath_nid_no_patch(static_cast<CairoContext*>(path.m_path)), CairoPathDestroy_nid_no_patch);
    if (!copy) return;
    for (int i = 0; i < copy->num_data; i += copy->data[i].header.length) {
        const CairoPathData& data = copy->data[i];
        PathElement element{};
        switch (data.header.type) {
        case 0:
            element.type = PathElement::Type::MoveToPoint;
            element.points[0] = {static_cast<float>(copy->data[i + 1].point.x), static_cast<float>(copy->data[i + 1].point.y)};
            break;
        case 1:
            element.type = PathElement::Type::AddLineToPoint;
            element.points[0] = {static_cast<float>(copy->data[i + 1].point.x), static_cast<float>(copy->data[i + 1].point.y)};
            break;
        case 2:
            element.type = PathElement::Type::AddCurveToPoint;
            element.points[0] = {static_cast<float>(copy->data[i + 1].point.x), static_cast<float>(copy->data[i + 1].point.y)};
            element.points[1] = {static_cast<float>(copy->data[i + 2].point.x), static_cast<float>(copy->data[i + 2].point.y)};
            element.points[2] = {static_cast<float>(copy->data[i + 3].point.x), static_cast<float>(copy->data[i + 3].point.y)};
            break;
        case 3:
            element.type = PathElement::Type::CloseSubpath;
            break;
        default:
            continue;
        }
        Invoke(function, element);
    }
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore4PathC1Ev(Path* self) {
    Initialize(Checked(self, "WebCore::Path::Path"));
}

void APS5_VABI _ZN7WebCore4PathC2Ev(Path* self) {
    Initialize(Checked(self, "WebCore::Path::Path"));
}

void APS5_VABI _ZN7WebCore4PathC1EOS0_(Path* self, Path* other) {
    MovePath(Checked(self, "WebCore::Path::Path"), Checked(other, "WebCore::Path::Path"));
}

void APS5_VABI _ZN7WebCore4PathC2EOS0_(Path* self, Path* other) {
    MovePath(Checked(self, "WebCore::Path::Path"), Checked(other, "WebCore::Path::Path"));
}

void APS5_VABI _ZN7WebCore4PathC1ERKS0_(Path* self, const Path* other) {
    CopyPath(Checked(self, "WebCore::Path::Path"), Checked(other, "WebCore::Path::Path"), "WebCore::Path::Path");
}

void APS5_VABI _ZN7WebCore4PathC2ERKS0_(Path* self, const Path* other) {
    CopyPath(Checked(self, "WebCore::Path::Path"), Checked(other, "WebCore::Path::Path"), "WebCore::Path::Path");
}

void APS5_VABI _ZN7WebCore4PathD1Ev(Path* self) {
    Destroy(Checked(self, "WebCore::Path::~Path"));
}

void APS5_VABI _ZN7WebCore4PathD2Ev(Path* self) {
    Destroy(Checked(self, "WebCore::Path::~Path"));
}

Path& APS5_VABI _ZN7WebCore4PathaSERKS0_(Path* self, const Path* other) {
    return AssignPath(Checked(self, "WebCore::Path::operator="), Checked(other, "WebCore::Path::operator="));
}

Path& APS5_VABI _ZN7WebCore4PathaSEOS0_(Path* self, Path* other) {
    return MoveAssignPath(Checked(self, "WebCore::Path::operator="), Checked(other, "WebCore::Path::operator="));
}

void APS5_VABI _ZN7WebCore4Path6moveToERKNS_10FloatPointE(Path* self, const WebCore::FloatPoint* point) {
    auto& path = Checked(self, "WebCore::Path::moveTo");
    if (!point) throw std::invalid_argument("WebCore::Path::moveTo requires a point");
    CairoMoveTo_nid_no_patch(EnsureCairo(path, "WebCore::Path::moveTo"), point->m_x, point->m_y);
    AppendElement(path, PathElement::Type::MoveToPoint, point, 1, "WebCore::Path::moveTo");
}

void APS5_VABI _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(Path* self, const WebCore::FloatPoint* point) {
    auto& path = Checked(self, "WebCore::Path::addLineTo");
    if (!point) throw std::invalid_argument("WebCore::Path::addLineTo requires a point");
    CairoLineTo_nid_no_patch(EnsureCairo(path, "WebCore::Path::addLineTo"), point->m_x, point->m_y);
    AppendElement(path, PathElement::Type::AddLineToPoint, point, 1, "WebCore::Path::addLineTo");
}

void APS5_VABI _ZN7WebCore4Path14addQuadCurveToERKNS_10FloatPointES3_(Path* self, const WebCore::FloatPoint* controlPoint, const WebCore::FloatPoint* endPoint) {
    auto& path = Checked(self, "WebCore::Path::addQuadCurveTo");
    if (!controlPoint || !endPoint) throw std::invalid_argument("WebCore::Path::addQuadCurveTo requires two points");
    CairoContext* context = EnsureCairo(path, "WebCore::Path::addQuadCurveTo");
    double x = 0;
    double y = 0;
    CairoGetCurrentPoint_nid_no_patch(context, &x, &y);
    const double controlX = controlPoint->m_x;
    const double controlY = controlPoint->m_y;
    const double endX = endPoint->m_x;
    const double endY = endPoint->m_y;
    CairoCurveTo_nid_no_patch(context,
        x + 2.0 / 3.0 * (controlX - x), y + 2.0 / 3.0 * (controlY - y),
        endX + 2.0 / 3.0 * (controlX - endX), endY + 2.0 / 3.0 * (controlY - endY),
        endX, endY);
    const WebCore::FloatPoint points[2] = {*controlPoint, *endPoint};
    AppendElement(path, PathElement::Type::AddQuadCurveToPoint, points, 2, "WebCore::Path::addQuadCurveTo");
}

void APS5_VABI _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(Path* self, const WebCore::FloatPoint* controlPoint1, const WebCore::FloatPoint* controlPoint2, const WebCore::FloatPoint* endPoint) {
    auto& path = Checked(self, "WebCore::Path::addBezierCurveTo");
    if (!controlPoint1 || !controlPoint2 || !endPoint) throw std::invalid_argument("WebCore::Path::addBezierCurveTo requires three points");
    CairoCurveTo_nid_no_patch(EnsureCairo(path, "WebCore::Path::addBezierCurveTo"),
        controlPoint1->m_x, controlPoint1->m_y, controlPoint2->m_x, controlPoint2->m_y, endPoint->m_x, endPoint->m_y);
    const WebCore::FloatPoint points[3] = {*controlPoint1, *controlPoint2, *endPoint};
    AppendElement(path, PathElement::Type::AddCurveToPoint, points, 3, "WebCore::Path::addBezierCurveTo");
}

void APS5_VABI _ZN7WebCore4Path12closeSubpathEv(Path* self) {
    auto& path = Checked(self, "WebCore::Path::closeSubpath");
    CairoContext* context = EnsureCairo(path, "WebCore::Path::closeSubpath");
    CairoClosePath_nid_no_patch(context);
    if (!path.m_elementsEngaged) return;
    AppendElement(path, PathElement::Type::CloseSubpath, nullptr, 0, "WebCore::Path::closeSubpath");
    double x = 0;
    double y = 0;
    if (CairoHasCurrentPoint_nid_no_patch(context)) {
        CairoGetCurrentPoint_nid_no_patch(context, &x, &y);
        const WebCore::FloatPoint current{static_cast<float>(x), static_cast<float>(y)};
        AppendElement(path, PathElement::Type::MoveToPoint, &current, 1, "WebCore::Path::closeSubpath");
    }
}

void APS5_VABI _ZN7WebCore4Path5clearEv(Path* self) {
    Clear(Checked(self, "WebCore::Path::clear"));
}

void APS5_VABI _ZN7WebCore4Path14addRoundedRectERKNS_16FloatRoundedRectENS0_19RoundedRectStrategyE(Path* self, const WebCore::FloatRoundedRect* roundedRect, std::uint8_t) {
    auto& path = Checked(self, "WebCore::Path::addRoundedRect");
    if (!roundedRect) throw std::invalid_argument("WebCore::Path::addRoundedRect requires a rectangle");
    if (IsEmpty(roundedRect->m_rect)) return;
    if (!IsRenderable(*roundedRect)) {
        AddRect(path, roundedRect->m_rect);
        return;
    }
    const auto& radii = roundedRect->m_radii;
    AddBeziersForRoundedRect(path, roundedRect->m_rect, radii.m_topLeft, radii.m_topRight, radii.m_bottomLeft, radii.m_bottomRight);
}

void APS5_VABI _ZN7WebCore4Path14addRoundedRectERKNS_9FloatRectERKNS_9FloatSizeENS0_19RoundedRectStrategyE(Path* self, const WebCore::FloatRect* rect, const WebCore::FloatSize* roundingRadii, std::uint8_t strategy) {
    auto& path = Checked(self, "WebCore::Path::addRoundedRect");
    if (!rect || !roundingRadii) throw std::invalid_argument("WebCore::Path::addRoundedRect requires a rectangle and radii");
    if (IsEmpty(*rect)) return;
    WebCore::FloatSize radius = *roundingRadii;
    const float halfWidth = rect->width() / 2;
    const float halfHeight = rect->height() / 2;
    if (radius.m_width < 0) radius.m_width = radius.m_height < 0 ? 0 : radius.m_height;
    if (radius.m_height < 0) radius.m_height = radius.m_width;
    if (radius.m_width > halfWidth) radius.m_width = halfWidth;
    if (radius.m_height > halfHeight) radius.m_height = halfHeight;
    WebCore::FloatRoundedRect rounded{*rect, {radius, radius, radius, radius}};
    _ZN7WebCore4Path14addRoundedRectERKNS_16FloatRoundedRectENS0_19RoundedRectStrategyE(&path, &rounded, strategy);
}

std::size_t APS5_VABI _ZNK7WebCore4Path12elementCountEv(const Path* self) {
    const auto& path = Checked(self, "WebCore::Path::elementCount");
    if (!path.m_path) return 0;
    if (path.m_elementsEngaged) {
        CheckElements(path, "WebCore::Path::elementCount");
        return path.m_elements.size;
    }
    CairoPath* copy = CairoCopyPath_nid_no_patch(static_cast<CairoContext*>(path.m_path));
    if (!copy) return 0;
    std::size_t count = 0;
    for (int i = 0; i < copy->num_data; i += copy->data[i].header.length) ++count;
    CairoPathDestroy_nid_no_patch(copy);
    return count;
}

void APS5_VABI _ZNK7WebCore4Path5applyERKN3WTF8FunctionIFvRKNS_11PathElementEEEE(const Path* self, const PathElementFunction* function) {
    const auto& path = Checked(self, "WebCore::Path::apply");
    if (!function || !function->callableWrapper) throw std::invalid_argument("WebCore::Path::apply requires a callable");
    if (!path.m_path) return;
    if (!path.m_elementsEngaged) {
        ApplyCairoPath(path, *function);
        return;
    }
    CheckElements(path, "WebCore::Path::apply");
    for (std::uint32_t i = 0; i < path.m_elements.size; ++i) Invoke(*function, path.m_elements.buffer[i]);
}

WebCore::FloatRect APS5_VABI _ZNK7WebCore4Path16fastBoundingRectEv(const Path* self) {
    const auto& path = Checked(self, "WebCore::Path::fastBoundingRect");
    if (!path.m_path) return {};
    double x1 = 0;
    double y1 = 0;
    double x2 = 0;
    double y2 = 0;
    CairoPathExtents_nid_no_patch(static_cast<CairoContext*>(path.m_path), &x1, &y1, &x2, &y2);
    return {{static_cast<float>(x1), static_cast<float>(y1)}, {static_cast<float>(x2 - x1), static_cast<float>(y2 - y1)}};
}

}
