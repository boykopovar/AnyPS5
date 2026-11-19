#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/SVGPathStringBuilderAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore20SVGPathStringBuilderC1Ev(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilderC2Ev(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD0Ev(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD1Ev(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilderD2Ev(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20SVGPathStringBuilder6resultEv(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self, const WebCore::FloatPoint* targetPoint, bool closed, NKWebKit::WebCoreAbi::PathCoordinateMode mode);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self, const WebCore::FloatPoint* targetPoint, NKWebKit::WebCoreAbi::PathCoordinateMode mode);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2, const WebCore::FloatPoint* targetPoint, NKWebKit::WebCoreAbi::PathCoordinateMode mode);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* targetPoint, NKWebKit::WebCoreAbi::PathCoordinateMode mode);
void APS5_VABI _ZN7WebCore20SVGPathStringBuilder9closePathEv(NKWebKit::WebCoreAbi::SVGPathStringBuilder* self);

}
