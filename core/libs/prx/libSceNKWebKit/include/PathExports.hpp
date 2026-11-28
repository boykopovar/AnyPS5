#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/PathAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore4PathC1Ev(NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZN7WebCore4PathC2Ev(NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZN7WebCore4PathC1ERKS0_(NKWebKit::PathAbi::Path* self, const NKWebKit::PathAbi::Path* other);
NKWebKit::PathAbi::Path& APS5_VABI _ZN7WebCore4PathaSERKS0_(NKWebKit::PathAbi::Path* self, const NKWebKit::PathAbi::Path* other);
NKWebKit::PathAbi::Path& APS5_VABI _ZN7WebCore4PathaSEOS0_(NKWebKit::PathAbi::Path* self, NKWebKit::PathAbi::Path* other);
void APS5_VABI _ZN7WebCore4PathC1EOS0_(NKWebKit::PathAbi::Path* self, NKWebKit::PathAbi::Path* other);
void APS5_VABI _ZN7WebCore4PathC2EOS0_(NKWebKit::PathAbi::Path* self, NKWebKit::PathAbi::Path* other);
void APS5_VABI _ZN7WebCore4PathD1Ev(NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZN7WebCore4Path6moveToERKNS_10FloatPointE(NKWebKit::PathAbi::Path* self, const WebCore::FloatPoint* point);
void APS5_VABI _ZN7WebCore4Path9addLineToERKNS_10FloatPointE(NKWebKit::PathAbi::Path* self, const WebCore::FloatPoint* point);
void APS5_VABI _ZN7WebCore4Path14addQuadCurveToERKNS_10FloatPointES3_(NKWebKit::PathAbi::Path* self, const WebCore::FloatPoint* controlPoint, const WebCore::FloatPoint* endPoint);
void APS5_VABI _ZN7WebCore4Path16addBezierCurveToERKNS_10FloatPointES3_S3_(NKWebKit::PathAbi::Path* self, const WebCore::FloatPoint* controlPoint1, const WebCore::FloatPoint* controlPoint2, const WebCore::FloatPoint* endPoint);
void APS5_VABI _ZN7WebCore4Path12closeSubpathEv(NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZN7WebCore4Path5clearEv(NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZN7WebCore4Path14addRoundedRectERKNS_16FloatRoundedRectENS0_19RoundedRectStrategyE(NKWebKit::PathAbi::Path* self, const WebCore::FloatRoundedRect* roundedRect, std::uint8_t strategy);
void APS5_VABI _ZN7WebCore4Path14addRoundedRectERKNS_9FloatRectERKNS_9FloatSizeENS0_19RoundedRectStrategyE(NKWebKit::PathAbi::Path* self, const WebCore::FloatRect* rect, const WebCore::FloatSize* roundingRadii, std::uint8_t strategy);
std::size_t APS5_VABI _ZNK7WebCore4Path12elementCountEv(const NKWebKit::PathAbi::Path* self);
void APS5_VABI _ZNK7WebCore4Path5applyERKN3WTF8FunctionIFvRKNS_11PathElementEEEE(const NKWebKit::PathAbi::Path* self, const NKWebKit::PathAbi::PathElementFunction* function);
WebCore::FloatRect APS5_VABI _ZNK7WebCore4Path16fastBoundingRectEv(const NKWebKit::PathAbi::Path* self);

void APS5_VABI _ZN7WebCore11DisplayList8FillPathC1ERKNS_4PathE(NKWebKit::PathAbi::FillPath* self, const NKWebKit::PathAbi::Path* path);
void APS5_VABI _ZN7WebCore11DisplayList8FillPathD0Ev(NKWebKit::PathAbi::FillPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8FillPathD1Ev(NKWebKit::PathAbi::FillPath* self);

void APS5_VABI _ZN7WebCore11DisplayList10StrokePathC1ERKNS_4PathE(NKWebKit::PathAbi::StrokePath* self, const NKWebKit::PathAbi::Path* path);
void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD0Ev(NKWebKit::PathAbi::StrokePath* self);
void APS5_VABI _ZN7WebCore11DisplayList10StrokePathD1Ev(NKWebKit::PathAbi::StrokePath* self);

void APS5_VABI _ZN7WebCore11DisplayList8DrawPathC1ERKNS_4PathE(NKWebKit::PathAbi::DrawPath* self, const NKWebKit::PathAbi::Path* path);
void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD0Ev(NKWebKit::PathAbi::DrawPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawPathD1Ev(NKWebKit::PathAbi::DrawPath* self);

void APS5_VABI _ZN7WebCore11DisplayList8ClipPathC1ERKNS_4PathENS_8WindRuleE(NKWebKit::PathAbi::ClipPath* self, const NKWebKit::PathAbi::Path* path, std::uint8_t windRule);
void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD0Ev(NKWebKit::PathAbi::ClipPath* self);
void APS5_VABI _ZN7WebCore11DisplayList8ClipPathD1Ev(NKWebKit::PathAbi::ClipPath* self);

void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathC1ERKNS_4PathE(NKWebKit::PathAbi::ClipOutToPath* self, const NKWebKit::PathAbi::Path* path);
void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD0Ev(NKWebKit::PathAbi::ClipOutToPath* self);
void APS5_VABI _ZN7WebCore11DisplayList13ClipOutToPathD1Ev(NKWebKit::PathAbi::ClipOutToPath* self);

void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathC1ERKNS_4PathEffRKNS_5ColorE(NKWebKit::PathAbi::DrawFocusRingPath* self, const NKWebKit::PathAbi::Path* path, float width, float offset, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD0Ev(NKWebKit::PathAbi::DrawFocusRingPath* self);
void APS5_VABI _ZN7WebCore11DisplayList17DrawFocusRingPathD1Ev(NKWebKit::PathAbi::DrawFocusRingPath* self);

}
