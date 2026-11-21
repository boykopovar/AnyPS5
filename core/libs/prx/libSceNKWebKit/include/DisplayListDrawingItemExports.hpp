#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::DisplayListAbi {

struct DocumentMarkerLineStyle {
    std::uint8_t mode;
    bool shouldUseDarkAppearance;
};

static_assert(std::is_standard_layout_v<DocumentMarkerLineStyle>);
static_assert(sizeof(DocumentMarkerLineStyle) == 2);

struct BeginTransparencyLayer {
    Item item;
    OptionalFloatRect extent;
    float opacity;
};

static_assert(std::is_standard_layout_v<BeginTransparencyLayer>);
static_assert(sizeof(BeginTransparencyLayer) == 40);
static_assert(offsetof(BeginTransparencyLayer, opacity) == 36);

struct EndTransparencyLayer {
    Item item;
    OptionalFloatRect extent;
};

static_assert(std::is_standard_layout_v<EndTransparencyLayer>);
static_assert(sizeof(EndTransparencyLayer) == 40);

struct DrawRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    float borderThickness;
};

static_assert(std::is_standard_layout_v<DrawRect>);
static_assert(sizeof(DrawRect) == 56);
static_assert(offsetof(DrawRect, rect) == 36);
static_assert(offsetof(DrawRect, borderThickness) == 52);

struct DrawLine {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatPoint point1;
    WebCore::FloatPoint point2;
};

static_assert(std::is_standard_layout_v<DrawLine>);
static_assert(sizeof(DrawLine) == 56);
static_assert(offsetof(DrawLine, point1) == 36);
static_assert(offsetof(DrawLine, point2) == 44);

struct DrawLinesForText {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatPoint blockLocation;
    WebCore::FloatSize localAnchor;
    Vector<double> widths;
    float thickness;
    bool printing;
    bool doubleLines;
};

static_assert(std::is_standard_layout_v<DrawLinesForText>);
static_assert(sizeof(DrawLinesForText) == 80);
static_assert(offsetof(DrawLinesForText, blockLocation) == 36);
static_assert(offsetof(DrawLinesForText, localAnchor) == 44);
static_assert(offsetof(DrawLinesForText, widths) == 56);
static_assert(offsetof(DrawLinesForText, thickness) == 72);
static_assert(offsetof(DrawLinesForText, printing) == 76);
static_assert(offsetof(DrawLinesForText, doubleLines) == 77);

struct DrawDotsForDocumentMarker {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    DocumentMarkerLineStyle style;
};

static_assert(std::is_standard_layout_v<DrawDotsForDocumentMarker>);
static_assert(sizeof(DrawDotsForDocumentMarker) == 56);
static_assert(offsetof(DrawDotsForDocumentMarker, rect) == 36);
static_assert(offsetof(DrawDotsForDocumentMarker, style) == 52);

struct DrawEllipse {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<DrawEllipse>);
static_assert(sizeof(DrawEllipse) == 56);
static_assert(offsetof(DrawEllipse, rect) == 36);

struct FillRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<FillRect>);
static_assert(sizeof(FillRect) == 56);
static_assert(offsetof(FillRect, rect) == 36);

struct FillEllipse {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<FillEllipse>);
static_assert(sizeof(FillEllipse) == 56);
static_assert(offsetof(FillEllipse, rect) == 36);

struct StrokeRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    float lineWidth;
};

static_assert(std::is_standard_layout_v<StrokeRect>);
static_assert(sizeof(StrokeRect) == 56);
static_assert(offsetof(StrokeRect, rect) == 36);
static_assert(offsetof(StrokeRect, lineWidth) == 52);

struct StrokeEllipse {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<StrokeEllipse>);
static_assert(sizeof(StrokeEllipse) == 56);
static_assert(offsetof(StrokeEllipse, rect) == 36);

struct ClearRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<ClearRect>);
static_assert(sizeof(ClearRect) == 56);
static_assert(offsetof(ClearRect, rect) == 36);

}  // namespace NKWebKit::DisplayListAbi

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerC1Ef(NKWebKit::DisplayListAbi::BeginTransparencyLayer* self, float opacity);
void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerC2Ef(NKWebKit::DisplayListAbi::BeginTransparencyLayer* self, float opacity);
void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD0Ev(NKWebKit::DisplayListAbi::BeginTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD1Ev(NKWebKit::DisplayListAbi::BeginTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList22BeginTransparencyLayerD2Ev(NKWebKit::DisplayListAbi::BeginTransparencyLayer* self);

void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerC1Ev(NKWebKit::DisplayListAbi::EndTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerC2Ev(NKWebKit::DisplayListAbi::EndTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD0Ev(NKWebKit::DisplayListAbi::EndTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD1Ev(NKWebKit::DisplayListAbi::EndTransparencyLayer* self);
void APS5_VABI _ZN7WebCore11DisplayList20EndTransparencyLayerD2Ev(NKWebKit::DisplayListAbi::EndTransparencyLayer* self);

void APS5_VABI _ZN7WebCore11DisplayList8DrawRectC1ERKNS_9FloatRectEf(NKWebKit::DisplayListAbi::DrawRect* self, const WebCore::FloatRect* rect, float borderThickness);
void APS5_VABI _ZN7WebCore11DisplayList8DrawRectC2ERKNS_9FloatRectEf(NKWebKit::DisplayListAbi::DrawRect* self, const WebCore::FloatRect* rect, float borderThickness);
void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD0Ev(NKWebKit::DisplayListAbi::DrawRect* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD1Ev(NKWebKit::DisplayListAbi::DrawRect* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawRectD2Ev(NKWebKit::DisplayListAbi::DrawRect* self);

void APS5_VABI _ZN7WebCore11DisplayList8DrawLineC1ERKNS_10FloatPointES4_(NKWebKit::DisplayListAbi::DrawLine* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2);
void APS5_VABI _ZN7WebCore11DisplayList8DrawLineC2ERKNS_10FloatPointES4_(NKWebKit::DisplayListAbi::DrawLine* self, const WebCore::FloatPoint* point1, const WebCore::FloatPoint* point2);
void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD0Ev(NKWebKit::DisplayListAbi::DrawLine* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD1Ev(NKWebKit::DisplayListAbi::DrawLine* self);
void APS5_VABI _ZN7WebCore11DisplayList8DrawLineD2Ev(NKWebKit::DisplayListAbi::DrawLine* self);

void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextC1ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(NKWebKit::DisplayListAbi::DrawLinesForText* self, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, float thickness, const NKWebKit::DisplayListAbi::Vector<double>* widths, bool printing, bool doubleLines);
void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextC2ERKNS_10FloatPointERKNS_9FloatSizeEfRKN3WTF6VectorIdLm0ENS8_15CrashOnOverflowELm16ENS8_10FastMallocEEEbb(NKWebKit::DisplayListAbi::DrawLinesForText* self, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor, float thickness, const NKWebKit::DisplayListAbi::Vector<double>* widths, bool printing, bool doubleLines);
void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD0Ev(NKWebKit::DisplayListAbi::DrawLinesForText* self);
void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD1Ev(NKWebKit::DisplayListAbi::DrawLinesForText* self);
void APS5_VABI _ZN7WebCore11DisplayList16DrawLinesForTextD2Ev(NKWebKit::DisplayListAbi::DrawLinesForText* self);

void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC1ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker* self, const WebCore::FloatRect* rect, NKWebKit::DisplayListAbi::DocumentMarkerLineStyle style);
void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerC2ERKNS_9FloatRectENS_23DocumentMarkerLineStyleE(NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker* self, const WebCore::FloatRect* rect, NKWebKit::DisplayListAbi::DocumentMarkerLineStyle style);
void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD0Ev(NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker* self);
void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD1Ev(NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker* self);
void APS5_VABI _ZN7WebCore11DisplayList25DrawDotsForDocumentMarkerD2Ev(NKWebKit::DisplayListAbi::DrawDotsForDocumentMarker* self);

void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::DrawEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::DrawEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD0Ev(NKWebKit::DisplayListAbi::DrawEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD1Ev(NKWebKit::DisplayListAbi::DrawEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawEllipseD2Ev(NKWebKit::DisplayListAbi::DrawEllipse* self);

void APS5_VABI _ZN7WebCore11DisplayList8FillRectC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::FillRect* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList8FillRectC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::FillRect* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList8FillRectD0Ev(NKWebKit::DisplayListAbi::FillRect* self);
void APS5_VABI _ZN7WebCore11DisplayList8FillRectD1Ev(NKWebKit::DisplayListAbi::FillRect* self);
void APS5_VABI _ZN7WebCore11DisplayList8FillRectD2Ev(NKWebKit::DisplayListAbi::FillRect* self);

void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::FillEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::FillEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD0Ev(NKWebKit::DisplayListAbi::FillEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD1Ev(NKWebKit::DisplayListAbi::FillEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList11FillEllipseD2Ev(NKWebKit::DisplayListAbi::FillEllipse* self);

void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectC1ERKNS_9FloatRectEf(NKWebKit::DisplayListAbi::StrokeRect* self, const WebCore::FloatRect* rect, float lineWidth);
void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectC2ERKNS_9FloatRectEf(NKWebKit::DisplayListAbi::StrokeRect* self, const WebCore::FloatRect* rect, float lineWidth);
void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD0Ev(NKWebKit::DisplayListAbi::StrokeRect* self);
void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD1Ev(NKWebKit::DisplayListAbi::StrokeRect* self);
void APS5_VABI _ZN7WebCore11DisplayList10StrokeRectD2Ev(NKWebKit::DisplayListAbi::StrokeRect* self);

void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::StrokeEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::StrokeEllipse* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD0Ev(NKWebKit::DisplayListAbi::StrokeEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD1Ev(NKWebKit::DisplayListAbi::StrokeEllipse* self);
void APS5_VABI _ZN7WebCore11DisplayList13StrokeEllipseD2Ev(NKWebKit::DisplayListAbi::StrokeEllipse* self);

void APS5_VABI _ZN7WebCore11DisplayList9ClearRectC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::ClearRect* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList9ClearRectC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::ClearRect* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD0Ev(NKWebKit::DisplayListAbi::ClearRect* self);
void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD1Ev(NKWebKit::DisplayListAbi::ClearRect* self);
void APS5_VABI _ZN7WebCore11DisplayList9ClearRectD2Ev(NKWebKit::DisplayListAbi::ClearRect* self);

}
