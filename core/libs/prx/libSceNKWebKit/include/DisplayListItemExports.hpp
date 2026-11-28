#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::DisplayListAbi {

struct Save {
    Item item;
};

static_assert(std::is_standard_layout_v<Save>);
static_assert(sizeof(Save) == 16);

struct Restore {
    Item item;
};

static_assert(std::is_standard_layout_v<Restore>);
static_assert(sizeof(Restore) == 16);

struct Translate {
    Item item;
    float x;
    float y;
};

static_assert(std::is_standard_layout_v<Translate>);
static_assert(sizeof(Translate) == 24);
static_assert(offsetof(Translate, x) == 16);
static_assert(offsetof(Translate, y) == 20);

struct Rotate {
    Item item;
    float angle;
};

static_assert(std::is_standard_layout_v<Rotate>);
static_assert(sizeof(Rotate) == 24);
static_assert(offsetof(Rotate, angle) == 16);

struct Scale {
    Item item;
    WebCore::FloatSize size;
};

static_assert(std::is_standard_layout_v<Scale>);
static_assert(sizeof(Scale) == 24);
static_assert(offsetof(Scale, size) == 16);

struct SetCTM {
    Item item;
    WebCore::AffineTransform transform;
};

static_assert(std::is_standard_layout_v<SetCTM>);
static_assert(sizeof(SetCTM) == 64);
static_assert(offsetof(SetCTM, transform) == 16);

struct ConcatenateCTM {
    Item item;
    WebCore::AffineTransform transform;
};

static_assert(std::is_standard_layout_v<ConcatenateCTM>);
static_assert(sizeof(ConcatenateCTM) == 64);
static_assert(offsetof(ConcatenateCTM, transform) == 16);

struct SetLineCap {
    Item item;
    int lineCap;
};

static_assert(std::is_standard_layout_v<SetLineCap>);
static_assert(sizeof(SetLineCap) == 24);
static_assert(offsetof(SetLineCap, lineCap) == 16);

struct SetLineDash {
    Item item;
    Vector<double> dashArray;
    float dashOffset;
};

static_assert(std::is_standard_layout_v<SetLineDash>);
static_assert(sizeof(SetLineDash) == 40);
static_assert(offsetof(SetLineDash, dashArray) == 16);
static_assert(offsetof(SetLineDash, dashOffset) == 32);

struct SetLineJoin {
    Item item;
    int lineJoin;
};

static_assert(std::is_standard_layout_v<SetLineJoin>);
static_assert(sizeof(SetLineJoin) == 24);
static_assert(offsetof(SetLineJoin, lineJoin) == 16);

struct SetMiterLimit {
    Item item;
    float miterLimit;
};

static_assert(std::is_standard_layout_v<SetMiterLimit>);
static_assert(sizeof(SetMiterLimit) == 24);
static_assert(offsetof(SetMiterLimit, miterLimit) == 16);

struct ClearShadow {
    Item item;
};

static_assert(std::is_standard_layout_v<ClearShadow>);
static_assert(sizeof(ClearShadow) == 16);

struct ApplyDeviceScaleFactor {
    Item item;
    float scaleFactor;
};

static_assert(std::is_standard_layout_v<ApplyDeviceScaleFactor>);
static_assert(sizeof(ApplyDeviceScaleFactor) == 24);
static_assert(offsetof(ApplyDeviceScaleFactor, scaleFactor) == 16);

struct Clip {
    Item item;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<Clip>);
static_assert(sizeof(Clip) == 32);
static_assert(offsetof(Clip, rect) == 16);

struct ClipOut {
    Item item;
    WebCore::FloatRect rect;
};

static_assert(std::is_standard_layout_v<ClipOut>);
static_assert(sizeof(ClipOut) == 32);
static_assert(offsetof(ClipOut, rect) == 16);

}  // namespace NKWebKit::DisplayListAbi

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(NKWebKit::DisplayListAbi::Item* self, NKWebKit::DisplayListAbi::ItemType type);
void APS5_VABI _ZN7WebCore11DisplayList4ItemD0Ev(NKWebKit::DisplayListAbi::Item* self);
void APS5_VABI _ZN7WebCore11DisplayList4ItemD1Ev(NKWebKit::DisplayListAbi::Item* self);
void APS5_VABI _ZN7WebCore11DisplayList4ItemD2Ev(NKWebKit::DisplayListAbi::Item* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(NKWebKit::DisplayListAbi::DrawingItem* self, NKWebKit::DisplayListAbi::ItemType type);
void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD0Ev(NKWebKit::DisplayListAbi::DrawingItem* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD1Ev(NKWebKit::DisplayListAbi::DrawingItem* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawingItemD2Ev(NKWebKit::DisplayListAbi::DrawingItem* self);

void APS5_VABI _ZN7WebCore11DisplayList4SaveC1Ev(NKWebKit::DisplayListAbi::Save* self);
void APS5_VABI _ZN7WebCore11DisplayList4SaveC2Ev(NKWebKit::DisplayListAbi::Save* self);
void APS5_VABI _ZN7WebCore11DisplayList4SaveD0Ev(NKWebKit::DisplayListAbi::Save* self);
void APS5_VABI _ZN7WebCore11DisplayList4SaveD1Ev(NKWebKit::DisplayListAbi::Save* self);
void APS5_VABI _ZN7WebCore11DisplayList4SaveD2Ev(NKWebKit::DisplayListAbi::Save* self);

void APS5_VABI _ZN7WebCore11DisplayList7RestoreC1Ev(NKWebKit::DisplayListAbi::Restore* self);
void APS5_VABI _ZN7WebCore11DisplayList7RestoreC2Ev(NKWebKit::DisplayListAbi::Restore* self);
void APS5_VABI _ZN7WebCore11DisplayList7RestoreD0Ev(NKWebKit::DisplayListAbi::Restore* self);
void APS5_VABI _ZN7WebCore11DisplayList7RestoreD1Ev(NKWebKit::DisplayListAbi::Restore* self);
void APS5_VABI _ZN7WebCore11DisplayList7RestoreD2Ev(NKWebKit::DisplayListAbi::Restore* self);

void APS5_VABI _ZN7WebCore11DisplayList9TranslateC1Eff(NKWebKit::DisplayListAbi::Translate* self, float x, float y);
void APS5_VABI _ZN7WebCore11DisplayList9TranslateC2Eff(NKWebKit::DisplayListAbi::Translate* self, float x, float y);
void APS5_VABI _ZN7WebCore11DisplayList9TranslateD0Ev(NKWebKit::DisplayListAbi::Translate* self);
void APS5_VABI _ZN7WebCore11DisplayList9TranslateD1Ev(NKWebKit::DisplayListAbi::Translate* self);
void APS5_VABI _ZN7WebCore11DisplayList9TranslateD2Ev(NKWebKit::DisplayListAbi::Translate* self);

void APS5_VABI _ZN7WebCore11DisplayList6RotateC1Ef(NKWebKit::DisplayListAbi::Rotate* self, float angle);
void APS5_VABI _ZN7WebCore11DisplayList6RotateC2Ef(NKWebKit::DisplayListAbi::Rotate* self, float angle);
void APS5_VABI _ZN7WebCore11DisplayList6RotateD0Ev(NKWebKit::DisplayListAbi::Rotate* self);
void APS5_VABI _ZN7WebCore11DisplayList6RotateD1Ev(NKWebKit::DisplayListAbi::Rotate* self);
void APS5_VABI _ZN7WebCore11DisplayList6RotateD2Ev(NKWebKit::DisplayListAbi::Rotate* self);

void APS5_VABI _ZN7WebCore11DisplayList5ScaleC1ERKNS_9FloatSizeE(NKWebKit::DisplayListAbi::Scale* self, const WebCore::FloatSize* size);
void APS5_VABI _ZN7WebCore11DisplayList5ScaleC2ERKNS_9FloatSizeE(NKWebKit::DisplayListAbi::Scale* self, const WebCore::FloatSize* size);
void APS5_VABI _ZN7WebCore11DisplayList5ScaleD0Ev(NKWebKit::DisplayListAbi::Scale* self);
void APS5_VABI _ZN7WebCore11DisplayList5ScaleD1Ev(NKWebKit::DisplayListAbi::Scale* self);
void APS5_VABI _ZN7WebCore11DisplayList5ScaleD2Ev(NKWebKit::DisplayListAbi::Scale* self);

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMC1ERKNS_15AffineTransformE(NKWebKit::DisplayListAbi::SetCTM* self, const WebCore::AffineTransform* transform);
void APS5_VABI _ZN7WebCore11DisplayList6SetCTMC2ERKNS_15AffineTransformE(NKWebKit::DisplayListAbi::SetCTM* self, const WebCore::AffineTransform* transform);
void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD0Ev(NKWebKit::DisplayListAbi::SetCTM* self);
void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD1Ev(NKWebKit::DisplayListAbi::SetCTM* self);
void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD2Ev(NKWebKit::DisplayListAbi::SetCTM* self);

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMC1ERKNS_15AffineTransformE(NKWebKit::DisplayListAbi::ConcatenateCTM* self, const WebCore::AffineTransform* transform);
void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMC2ERKNS_15AffineTransformE(NKWebKit::DisplayListAbi::ConcatenateCTM* self, const WebCore::AffineTransform* transform);
void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD0Ev(NKWebKit::DisplayListAbi::ConcatenateCTM* self);
void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD1Ev(NKWebKit::DisplayListAbi::ConcatenateCTM* self);
void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD2Ev(NKWebKit::DisplayListAbi::ConcatenateCTM* self);

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapC1ENS_7LineCapE(NKWebKit::DisplayListAbi::SetLineCap* self, int lineCap);
void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapC2ENS_7LineCapE(NKWebKit::DisplayListAbi::SetLineCap* self, int lineCap);
void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD0Ev(NKWebKit::DisplayListAbi::SetLineCap* self);
void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD1Ev(NKWebKit::DisplayListAbi::SetLineCap* self);
void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD2Ev(NKWebKit::DisplayListAbi::SetLineCap* self);

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(NKWebKit::DisplayListAbi::SetLineDash* self, const NKWebKit::DisplayListAbi::Vector<double>* dashArray, float dashOffset);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashC2ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(NKWebKit::DisplayListAbi::SetLineDash* self, const NKWebKit::DisplayListAbi::Vector<double>* dashArray, float dashOffset);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD0Ev(NKWebKit::DisplayListAbi::SetLineDash* self);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD1Ev(NKWebKit::DisplayListAbi::SetLineDash* self);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD2Ev(NKWebKit::DisplayListAbi::SetLineDash* self);

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinC1ENS_8LineJoinE(NKWebKit::DisplayListAbi::SetLineJoin* self, int lineJoin);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinC2ENS_8LineJoinE(NKWebKit::DisplayListAbi::SetLineJoin* self, int lineJoin);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD0Ev(NKWebKit::DisplayListAbi::SetLineJoin* self);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD1Ev(NKWebKit::DisplayListAbi::SetLineJoin* self);
void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD2Ev(NKWebKit::DisplayListAbi::SetLineJoin* self);

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitC1Ef(NKWebKit::DisplayListAbi::SetMiterLimit* self, float miterLimit);
void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitC2Ef(NKWebKit::DisplayListAbi::SetMiterLimit* self, float miterLimit);
void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD0Ev(NKWebKit::DisplayListAbi::SetMiterLimit* self);
void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD1Ev(NKWebKit::DisplayListAbi::SetMiterLimit* self);
void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD2Ev(NKWebKit::DisplayListAbi::SetMiterLimit* self);

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowC1Ev(NKWebKit::DisplayListAbi::ClearShadow* self);
void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowC2Ev(NKWebKit::DisplayListAbi::ClearShadow* self);
void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD0Ev(NKWebKit::DisplayListAbi::ClearShadow* self);
void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD1Ev(NKWebKit::DisplayListAbi::ClearShadow* self);
void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD2Ev(NKWebKit::DisplayListAbi::ClearShadow* self);

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC1Ef(NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor* self, float scaleFactor);
void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC2Ef(NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor* self, float scaleFactor);
void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD0Ev(NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor* self);
void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD1Ev(NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor* self);
void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD2Ev(NKWebKit::DisplayListAbi::ApplyDeviceScaleFactor* self);

void APS5_VABI _ZN7WebCore11DisplayList4ClipC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::Clip* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList4ClipC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::Clip* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList4ClipD0Ev(NKWebKit::DisplayListAbi::Clip* self);
void APS5_VABI _ZN7WebCore11DisplayList4ClipD1Ev(NKWebKit::DisplayListAbi::Clip* self);
void APS5_VABI _ZN7WebCore11DisplayList4ClipD2Ev(NKWebKit::DisplayListAbi::Clip* self);

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutC1ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::ClipOut* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList7ClipOutC2ERKNS_9FloatRectE(NKWebKit::DisplayListAbi::ClipOut* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD0Ev(NKWebKit::DisplayListAbi::ClipOut* self);
void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD1Ev(NKWebKit::DisplayListAbi::ClipOut* self);
void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD2Ev(NKWebKit::DisplayListAbi::ClipOut* self);

}
