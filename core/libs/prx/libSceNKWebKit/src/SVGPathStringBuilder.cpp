// Derived from Source/WebCore/svg/SVGPathStringBuilder.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SVGPathStringBuilderExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <new>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::AbsoluteCoordinates;
using NKWebKit::WebCoreAbi::PathCoordinateMode;
using NKWebKit::WebCoreAbi::SVGPathStringBuilder;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using WebCore::FloatPoint;

SVGPathStringBuilder& Checked(SVGPathStringBuilder* self, const char* caller) {
    if (!self) throw std::invalid_argument(std::string("WebCore::SVGPathStringBuilder::") + caller + ": null this");
    return *self;
}

const FloatPoint& CheckedPoint(const FloatPoint* point, const char* caller) {
    if (!point) throw std::invalid_argument(std::string("WebCore::SVGPathStringBuilder::") + caller + ": null point");
    return *point;
}

void AppendFlag(StringBuilder& builder, bool flag) {
    const char text[] = {flag ? '1' : '0', ' '};
    NKWebKit::WtfAbi::AppendAdapted(builder, text, sizeof(text));
}

void AppendNumber(StringBuilder& builder, float number) {
    auto text = NKWebKit::WtfAbi::NumberToFixedPrecisionString(number, 6);
    text += ' ';
    NKWebKit::WtfAbi::AppendAdapted(builder, text.data(), static_cast<unsigned>(text.size()));
}

void AppendPoint(StringBuilder& builder, const FloatPoint& point) {
    AppendNumber(builder, point.m_x);
    AppendNumber(builder, point.m_y);
}

void AppendCommand(StringBuilder& builder, PathCoordinateMode mode, const char* absolute, const char* relative) {
    NKWebKit::WtfAbi::AppendCString(builder, mode == AbsoluteCoordinates ? absolute : relative);
}

void APS5_VABI IncrementPathSegmentCount(SVGPathStringBuilder* self) {
    Checked(self, "incrementPathSegmentCount");
}

bool APS5_VABI ContinueConsuming(SVGPathStringBuilder* self) {
    Checked(self, "continueConsuming");
    return true;
}

void APS5_VABI LineToHorizontal(SVGPathStringBuilder* self, float x, PathCoordinateMode mode) {
    auto& builder = Checked(self, "lineToHorizontal").stringBuilder;
    AppendCommand(builder, mode, "H ", "h ");
    AppendNumber(builder, x);
}

void APS5_VABI LineToVertical(SVGPathStringBuilder* self, float y, PathCoordinateMode mode) {
    auto& builder = Checked(self, "lineToVertical").stringBuilder;
    AppendCommand(builder, mode, "V ", "v ");
    AppendNumber(builder, y);
}

void APS5_VABI CurveToCubicSmooth(SVGPathStringBuilder* self, const FloatPoint* point2, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "curveToCubicSmooth").stringBuilder;
    const auto& second = CheckedPoint(point2, "curveToCubicSmooth");
    const auto& target = CheckedPoint(targetPoint, "curveToCubicSmooth");
    AppendCommand(builder, mode, "S ", "s ");
    AppendPoint(builder, second);
    AppendPoint(builder, target);
}

void APS5_VABI CurveToQuadraticSmooth(SVGPathStringBuilder* self, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "curveToQuadraticSmooth").stringBuilder;
    const auto& target = CheckedPoint(targetPoint, "curveToQuadraticSmooth");
    AppendCommand(builder, mode, "T ", "t ");
    AppendPoint(builder, target);
}

void APS5_VABI ArcTo(SVGPathStringBuilder* self, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "arcTo").stringBuilder;
    const auto& target = CheckedPoint(targetPoint, "arcTo");
    AppendCommand(builder, mode, "A ", "a ");
    AppendNumber(builder, r1);
    AppendNumber(builder, r2);
    AppendNumber(builder, angle);
    AppendFlag(builder, largeArcFlag);
    AppendFlag(builder, sweepFlag);
    AppendPoint(builder, target);
}

const void* const Vtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&IncrementPathSegmentCount),
    reinterpret_cast<const void*>(&ContinueConsuming),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder9closePathEv),
    reinterpret_cast<const void*>(&LineToHorizontal),
    reinterpret_cast<const void*>(&LineToVertical),
    reinterpret_cast<const void*>(&CurveToCubicSmooth),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE),
    reinterpret_cast<const void*>(&CurveToQuadraticSmooth),
    reinterpret_cast<const void*>(&ArcTo),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilderD1Ev),
    reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilderD0Ev),
};

void Construct(SVGPathStringBuilder* self) {
    auto& object = Checked(self, "SVGPathStringBuilder");
    object.vtable = &Vtable[2];
    new (&object.stringBuilder) StringBuilder{};
    object.stringBuilder.characters8 = nullptr;
    object.stringBuilder.overflowed = false;
    object.stringBuilder.shouldCrashOnOverflow = true;
    object.stringBuilder.length = 0;
    object.stringBuilder.is8Bit = true;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderC1Ev(SVGPathStringBuilder* self) {
    Construct(self);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderC2Ev(SVGPathStringBuilder* self) {
    Construct(self);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD1Ev(SVGPathStringBuilder* self) {
    NKWebKit::WtfAbi::Clear(Checked(self, "~SVGPathStringBuilder").stringBuilder);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD2Ev(SVGPathStringBuilder* self) {
    _ZN7WebCore20SVGPathStringBuilderD1Ev(self);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD0Ev(SVGPathStringBuilder* self) {
    _ZN7WebCore20SVGPathStringBuilderD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

String APS5_VABI _ZN7WebCore20SVGPathStringBuilder6resultEv(SVGPathStringBuilder* self) {
    auto& builder = Checked(self, "result").stringBuilder;
    unsigned size = static_cast<unsigned>(builder.length);
    if (!size) return String();
    NKWebKit::WtfAbi::Resize(builder, size - 1);
    return NKWebKit::WtfAbi::ToString(builder);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE(SVGPathStringBuilder* self, const FloatPoint* targetPoint, bool, PathCoordinateMode mode) {
    auto& builder = Checked(self, "moveTo").stringBuilder;
    const auto& target = CheckedPoint(targetPoint, "moveTo");
    AppendCommand(builder, mode, "M ", "m ");
    AppendPoint(builder, target);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE(SVGPathStringBuilder* self, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "lineTo").stringBuilder;
    const auto& target = CheckedPoint(targetPoint, "lineTo");
    AppendCommand(builder, mode, "L ", "l ");
    AppendPoint(builder, target);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE(SVGPathStringBuilder* self, const FloatPoint* point1, const FloatPoint* point2, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "curveToCubic").stringBuilder;
    const auto& first = CheckedPoint(point1, "curveToCubic");
    const auto& second = CheckedPoint(point2, "curveToCubic");
    const auto& target = CheckedPoint(targetPoint, "curveToCubic");
    AppendCommand(builder, mode, "C ", "c ");
    AppendPoint(builder, first);
    AppendPoint(builder, second);
    AppendPoint(builder, target);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE(SVGPathStringBuilder* self, const FloatPoint* point1, const FloatPoint* targetPoint, PathCoordinateMode mode) {
    auto& builder = Checked(self, "curveToQuadratic").stringBuilder;
    const auto& first = CheckedPoint(point1, "curveToQuadratic");
    const auto& target = CheckedPoint(targetPoint, "curveToQuadratic");
    AppendCommand(builder, mode, "Q ", "q ");
    AppendPoint(builder, first);
    AppendPoint(builder, target);
}

void APS5_VABI _ZN7WebCore20SVGPathStringBuilder9closePathEv(SVGPathStringBuilder* self) {
    NKWebKit::WtfAbi::AppendCString(Checked(self, "closePath").stringBuilder, "Z ");
}

}
