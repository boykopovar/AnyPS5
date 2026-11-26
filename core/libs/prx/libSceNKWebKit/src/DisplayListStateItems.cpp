// Derived from Source/WebCore/platform/graphics/displaylists/DisplayListItems.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/DisplayListItemExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>

namespace {

using namespace NKWebKit::DisplayListAbi;

void APS5_VABI SaveCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList4SaveD1Ev(static_cast<Save*>(self));
}

void APS5_VABI SaveDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList4SaveD0Ev(static_cast<Save*>(self));
}

const ItemVtable SaveTable{SaveCompleteDestructor, SaveDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSave(Save* self) {
    auto& item = Checked(self, "WebCore::DisplayList::Save::Save");
    ConstructItem(item.item, &SaveTable, ItemType::Save);
}

void DestroySave(Save* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Save::~Save").item);
}

void APS5_VABI RestoreCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList7RestoreD1Ev(static_cast<Restore*>(self));
}

void APS5_VABI RestoreDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList7RestoreD0Ev(static_cast<Restore*>(self));
}

const ItemVtable RestoreTable{RestoreCompleteDestructor, RestoreDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructRestore(Restore* self) {
    auto& item = Checked(self, "WebCore::DisplayList::Restore::Restore");
    ConstructItem(item.item, &RestoreTable, ItemType::Restore);
}

void DestroyRestore(Restore* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Restore::~Restore").item);
}

void APS5_VABI TranslateCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList9TranslateD1Ev(static_cast<Translate*>(self));
}

void APS5_VABI TranslateDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList9TranslateD0Ev(static_cast<Translate*>(self));
}

const ItemVtable TranslateTable{TranslateCompleteDestructor, TranslateDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructTranslate(Translate* self, float x, float y) {
    auto& item = Checked(self, "WebCore::DisplayList::Translate::Translate");
    ConstructItem(item.item, &TranslateTable, ItemType::Translate);
    item.x = x;
    item.y = y;
}

void DestroyTranslate(Translate* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Translate::~Translate").item);
}

void APS5_VABI RotateCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList6RotateD1Ev(static_cast<Rotate*>(self));
}

void APS5_VABI RotateDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList6RotateD0Ev(static_cast<Rotate*>(self));
}

const ItemVtable RotateTable{RotateCompleteDestructor, RotateDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructRotate(Rotate* self, float angle) {
    auto& item = Checked(self, "WebCore::DisplayList::Rotate::Rotate");
    ConstructItem(item.item, &RotateTable, ItemType::Rotate);
    item.angle = angle;
}

void DestroyRotate(Rotate* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Rotate::~Rotate").item);
}

void APS5_VABI ScaleCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList5ScaleD1Ev(static_cast<Scale*>(self));
}

void APS5_VABI ScaleDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList5ScaleD0Ev(static_cast<Scale*>(self));
}

const ItemVtable ScaleTable{ScaleCompleteDestructor, ScaleDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructScale(Scale* self, const WebCore::FloatSize* size) {
    Checked(size, "WebCore::DisplayList::Scale::Scale");
    auto& item = Checked(self, "WebCore::DisplayList::Scale::Scale");
    ConstructItem(item.item, &ScaleTable, ItemType::Scale);
    item.size = *size;
}

void DestroyScale(Scale* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Scale::~Scale").item);
}

void APS5_VABI SetCTMCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList6SetCTMD1Ev(static_cast<SetCTM*>(self));
}

void APS5_VABI SetCTMDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList6SetCTMD0Ev(static_cast<SetCTM*>(self));
}

const ItemVtable SetCTMTable{SetCTMCompleteDestructor, SetCTMDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetCTM(SetCTM* self, const WebCore::AffineTransform* transform) {
    Checked(transform, "WebCore::DisplayList::SetCTM::SetCTM");
    auto& item = Checked(self, "WebCore::DisplayList::SetCTM::SetCTM");
    ConstructItem(item.item, &SetCTMTable, ItemType::SetCTM);
    item.transform = *transform;
}

void DestroySetCTM(SetCTM* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::SetCTM::~SetCTM").item);
}

void APS5_VABI ConcatenateCTMCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList14ConcatenateCTMD1Ev(static_cast<ConcatenateCTM*>(self));
}

void APS5_VABI ConcatenateCTMDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList14ConcatenateCTMD0Ev(static_cast<ConcatenateCTM*>(self));
}

const ItemVtable ConcatenateCTMTable{ConcatenateCTMCompleteDestructor, ConcatenateCTMDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructConcatenateCTM(ConcatenateCTM* self, const WebCore::AffineTransform* transform) {
    Checked(transform, "WebCore::DisplayList::ConcatenateCTM::ConcatenateCTM");
    auto& item = Checked(self, "WebCore::DisplayList::ConcatenateCTM::ConcatenateCTM");
    ConstructItem(item.item, &ConcatenateCTMTable, ItemType::ConcatenateCTM);
    item.transform = *transform;
}

void DestroyConcatenateCTM(ConcatenateCTM* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::ConcatenateCTM::~ConcatenateCTM").item);
}

void APS5_VABI SetLineCapCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList10SetLineCapD1Ev(static_cast<SetLineCap*>(self));
}

void APS5_VABI SetLineCapDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList10SetLineCapD0Ev(static_cast<SetLineCap*>(self));
}

const ItemVtable SetLineCapTable{SetLineCapCompleteDestructor, SetLineCapDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetLineCap(SetLineCap* self, int lineCap) {
    auto& item = Checked(self, "WebCore::DisplayList::SetLineCap::SetLineCap");
    ConstructItem(item.item, &SetLineCapTable, ItemType::SetLineCap);
    item.lineCap = lineCap;
}

void DestroySetLineCap(SetLineCap* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::SetLineCap::~SetLineCap").item);
}

void APS5_VABI SetLineDashCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11SetLineDashD1Ev(static_cast<SetLineDash*>(self));
}

void APS5_VABI SetLineDashDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11SetLineDashD0Ev(static_cast<SetLineDash*>(self));
}

const ItemVtable SetLineDashTable{SetLineDashCompleteDestructor, SetLineDashDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetLineDash(SetLineDash* self, const Vector<double>* dashArray, float dashOffset) {
    CheckVector(dashArray, "WebCore::DisplayList::SetLineDash::SetLineDash");
    auto& item = Checked(self, "WebCore::DisplayList::SetLineDash::SetLineDash");
    ConstructItem(item.item, &SetLineDashTable, ItemType::SetLineDash);
    CopyVector(item.dashArray, dashArray, "WebCore::DisplayList::SetLineDash::SetLineDash");
    item.dashOffset = dashOffset;
}

void DestroySetLineDash(SetLineDash* self) {
    auto& item = Checked(self, "WebCore::DisplayList::SetLineDash::~SetLineDash");
    DestroyVector(item.dashArray);
    DestroyItem(item.item);
}

void APS5_VABI SetLineJoinCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11SetLineJoinD1Ev(static_cast<SetLineJoin*>(self));
}

void APS5_VABI SetLineJoinDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11SetLineJoinD0Ev(static_cast<SetLineJoin*>(self));
}

const ItemVtable SetLineJoinTable{SetLineJoinCompleteDestructor, SetLineJoinDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetLineJoin(SetLineJoin* self, int lineJoin) {
    auto& item = Checked(self, "WebCore::DisplayList::SetLineJoin::SetLineJoin");
    ConstructItem(item.item, &SetLineJoinTable, ItemType::SetLineJoin);
    item.lineJoin = lineJoin;
}

void DestroySetLineJoin(SetLineJoin* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::SetLineJoin::~SetLineJoin").item);
}

void APS5_VABI SetMiterLimitCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList13SetMiterLimitD1Ev(static_cast<SetMiterLimit*>(self));
}

void APS5_VABI SetMiterLimitDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList13SetMiterLimitD0Ev(static_cast<SetMiterLimit*>(self));
}

const ItemVtable SetMiterLimitTable{SetMiterLimitCompleteDestructor, SetMiterLimitDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructSetMiterLimit(SetMiterLimit* self, float miterLimit) {
    auto& item = Checked(self, "WebCore::DisplayList::SetMiterLimit::SetMiterLimit");
    ConstructItem(item.item, &SetMiterLimitTable, ItemType::SetMiterLimit);
    item.miterLimit = miterLimit;
}

void DestroySetMiterLimit(SetMiterLimit* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::SetMiterLimit::~SetMiterLimit").item);
}

void APS5_VABI ClearShadowCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList11ClearShadowD1Ev(static_cast<ClearShadow*>(self));
}

void APS5_VABI ClearShadowDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList11ClearShadowD0Ev(static_cast<ClearShadow*>(self));
}

const ItemVtable ClearShadowTable{ClearShadowCompleteDestructor, ClearShadowDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructClearShadow(ClearShadow* self) {
    auto& item = Checked(self, "WebCore::DisplayList::ClearShadow::ClearShadow");
    ConstructItem(item.item, &ClearShadowTable, ItemType::ClearShadow);
}

void DestroyClearShadow(ClearShadow* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::ClearShadow::~ClearShadow").item);
}

void APS5_VABI ApplyDeviceScaleFactorCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD1Ev(static_cast<ApplyDeviceScaleFactor*>(self));
}

void APS5_VABI ApplyDeviceScaleFactorDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD0Ev(static_cast<ApplyDeviceScaleFactor*>(self));
}

const ItemVtable ApplyDeviceScaleFactorTable{ApplyDeviceScaleFactorCompleteDestructor, ApplyDeviceScaleFactorDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructApplyDeviceScaleFactor(ApplyDeviceScaleFactor* self, float scaleFactor) {
    auto& item = Checked(self, "WebCore::DisplayList::ApplyDeviceScaleFactor::ApplyDeviceScaleFactor");
    ConstructItem(item.item, &ApplyDeviceScaleFactorTable, ItemType::ApplyDeviceScaleFactor);
    item.scaleFactor = scaleFactor;
}

void DestroyApplyDeviceScaleFactor(ApplyDeviceScaleFactor* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::ApplyDeviceScaleFactor::~ApplyDeviceScaleFactor").item);
}

void APS5_VABI ClipCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList4ClipD1Ev(static_cast<Clip*>(self));
}

void APS5_VABI ClipDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList4ClipD0Ev(static_cast<Clip*>(self));
}

const ItemVtable ClipTable{ClipCompleteDestructor, ClipDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructClip(Clip* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::Clip::Clip");
    auto& item = Checked(self, "WebCore::DisplayList::Clip::Clip");
    ConstructItem(item.item, &ClipTable, ItemType::Clip);
    item.rect = *rect;
}

void DestroyClip(Clip* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::Clip::~Clip").item);
}

void APS5_VABI ClipOutCompleteDestructor(void* self) {
    _ZN7WebCore11DisplayList7ClipOutD1Ev(static_cast<ClipOut*>(self));
}

void APS5_VABI ClipOutDeletingDestructor(void* self) {
    _ZN7WebCore11DisplayList7ClipOutD0Ev(static_cast<ClipOut*>(self));
}

const ItemVtable ClipOutTable{ClipOutCompleteDestructor, ClipOutDeletingDestructor, ApplyWithoutGraphicsContext, IsNotDrawingItem};

void ConstructClipOut(ClipOut* self, const WebCore::FloatRect* rect) {
    Checked(rect, "WebCore::DisplayList::ClipOut::ClipOut");
    auto& item = Checked(self, "WebCore::DisplayList::ClipOut::ClipOut");
    ConstructItem(item.item, &ClipOutTable, ItemType::ClipOut);
    item.rect = *rect;
}

void DestroyClipOut(ClipOut* self) {
    DestroyItem(Checked(self, "WebCore::DisplayList::ClipOut::~ClipOut").item);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList4SaveC1Ev(Save* self) {
    ConstructSave(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4SaveC2Ev(Save* self) {
    ConstructSave(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4SaveD0Ev(Save* self) {
    DestroySave(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4SaveD1Ev(Save* self) {
    DestroySave(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4SaveD2Ev(Save* self) {
    DestroySave(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7RestoreC1Ev(Restore* self) {
    ConstructRestore(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7RestoreC2Ev(Restore* self) {
    ConstructRestore(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7RestoreD0Ev(Restore* self) {
    DestroyRestore(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7RestoreD1Ev(Restore* self) {
    DestroyRestore(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7RestoreD2Ev(Restore* self) {
    DestroyRestore(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9TranslateC1Eff(Translate* self, float x, float y) {
    ConstructTranslate(self, x, y);
}

void APS5_VABI _ZN7WebCore11DisplayList9TranslateC2Eff(Translate* self, float x, float y) {
    ConstructTranslate(self, x, y);
}

void APS5_VABI _ZN7WebCore11DisplayList9TranslateD0Ev(Translate* self) {
    DestroyTranslate(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9TranslateD1Ev(Translate* self) {
    DestroyTranslate(self);
}

void APS5_VABI _ZN7WebCore11DisplayList9TranslateD2Ev(Translate* self) {
    DestroyTranslate(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6RotateC1Ef(Rotate* self, float angle) {
    ConstructRotate(self, angle);
}

void APS5_VABI _ZN7WebCore11DisplayList6RotateC2Ef(Rotate* self, float angle) {
    ConstructRotate(self, angle);
}

void APS5_VABI _ZN7WebCore11DisplayList6RotateD0Ev(Rotate* self) {
    DestroyRotate(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6RotateD1Ev(Rotate* self) {
    DestroyRotate(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6RotateD2Ev(Rotate* self) {
    DestroyRotate(self);
}

void APS5_VABI _ZN7WebCore11DisplayList5ScaleC1ERKNS_9FloatSizeE(Scale* self, const WebCore::FloatSize* size) {
    ConstructScale(self, size);
}

void APS5_VABI _ZN7WebCore11DisplayList5ScaleC2ERKNS_9FloatSizeE(Scale* self, const WebCore::FloatSize* size) {
    ConstructScale(self, size);
}

void APS5_VABI _ZN7WebCore11DisplayList5ScaleD0Ev(Scale* self) {
    DestroyScale(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList5ScaleD1Ev(Scale* self) {
    DestroyScale(self);
}

void APS5_VABI _ZN7WebCore11DisplayList5ScaleD2Ev(Scale* self) {
    DestroyScale(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMC1ERKNS_15AffineTransformE(SetCTM* self, const WebCore::AffineTransform* transform) {
    ConstructSetCTM(self, transform);
}

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMC2ERKNS_15AffineTransformE(SetCTM* self, const WebCore::AffineTransform* transform) {
    ConstructSetCTM(self, transform);
}

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD0Ev(SetCTM* self) {
    DestroySetCTM(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD1Ev(SetCTM* self) {
    DestroySetCTM(self);
}

void APS5_VABI _ZN7WebCore11DisplayList6SetCTMD2Ev(SetCTM* self) {
    DestroySetCTM(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMC1ERKNS_15AffineTransformE(ConcatenateCTM* self, const WebCore::AffineTransform* transform) {
    ConstructConcatenateCTM(self, transform);
}

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMC2ERKNS_15AffineTransformE(ConcatenateCTM* self, const WebCore::AffineTransform* transform) {
    ConstructConcatenateCTM(self, transform);
}

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD0Ev(ConcatenateCTM* self) {
    DestroyConcatenateCTM(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD1Ev(ConcatenateCTM* self) {
    DestroyConcatenateCTM(self);
}

void APS5_VABI _ZN7WebCore11DisplayList14ConcatenateCTMD2Ev(ConcatenateCTM* self) {
    DestroyConcatenateCTM(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapC1ENS_7LineCapE(SetLineCap* self, int lineCap) {
    ConstructSetLineCap(self, lineCap);
}

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapC2ENS_7LineCapE(SetLineCap* self, int lineCap) {
    ConstructSetLineCap(self, lineCap);
}

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD0Ev(SetLineCap* self) {
    DestroySetLineCap(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD1Ev(SetLineCap* self) {
    DestroySetLineCap(self);
}

void APS5_VABI _ZN7WebCore11DisplayList10SetLineCapD2Ev(SetLineCap* self) {
    DestroySetLineCap(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashC1ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(SetLineDash* self, const Vector<double>* dashArray, float dashOffset) {
    ConstructSetLineDash(self, dashArray, dashOffset);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashC2ERKN3WTF6VectorIdLm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEf(SetLineDash* self, const Vector<double>* dashArray, float dashOffset) {
    ConstructSetLineDash(self, dashArray, dashOffset);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD0Ev(SetLineDash* self) {
    DestroySetLineDash(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD1Ev(SetLineDash* self) {
    DestroySetLineDash(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineDashD2Ev(SetLineDash* self) {
    DestroySetLineDash(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinC1ENS_8LineJoinE(SetLineJoin* self, int lineJoin) {
    ConstructSetLineJoin(self, lineJoin);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinC2ENS_8LineJoinE(SetLineJoin* self, int lineJoin) {
    ConstructSetLineJoin(self, lineJoin);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD0Ev(SetLineJoin* self) {
    DestroySetLineJoin(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD1Ev(SetLineJoin* self) {
    DestroySetLineJoin(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11SetLineJoinD2Ev(SetLineJoin* self) {
    DestroySetLineJoin(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitC1Ef(SetMiterLimit* self, float miterLimit) {
    ConstructSetMiterLimit(self, miterLimit);
}

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitC2Ef(SetMiterLimit* self, float miterLimit) {
    ConstructSetMiterLimit(self, miterLimit);
}

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD0Ev(SetMiterLimit* self) {
    DestroySetMiterLimit(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD1Ev(SetMiterLimit* self) {
    DestroySetMiterLimit(self);
}

void APS5_VABI _ZN7WebCore11DisplayList13SetMiterLimitD2Ev(SetMiterLimit* self) {
    DestroySetMiterLimit(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowC1Ev(ClearShadow* self) {
    ConstructClearShadow(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowC2Ev(ClearShadow* self) {
    ConstructClearShadow(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD0Ev(ClearShadow* self) {
    DestroyClearShadow(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD1Ev(ClearShadow* self) {
    DestroyClearShadow(self);
}

void APS5_VABI _ZN7WebCore11DisplayList11ClearShadowD2Ev(ClearShadow* self) {
    DestroyClearShadow(self);
}

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC1Ef(ApplyDeviceScaleFactor* self, float scaleFactor) {
    ConstructApplyDeviceScaleFactor(self, scaleFactor);
}

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorC2Ef(ApplyDeviceScaleFactor* self, float scaleFactor) {
    ConstructApplyDeviceScaleFactor(self, scaleFactor);
}

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD0Ev(ApplyDeviceScaleFactor* self) {
    DestroyApplyDeviceScaleFactor(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD1Ev(ApplyDeviceScaleFactor* self) {
    DestroyApplyDeviceScaleFactor(self);
}

void APS5_VABI _ZN7WebCore11DisplayList22ApplyDeviceScaleFactorD2Ev(ApplyDeviceScaleFactor* self) {
    DestroyApplyDeviceScaleFactor(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4ClipC1ERKNS_9FloatRectE(Clip* self, const WebCore::FloatRect* rect) {
    ConstructClip(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList4ClipC2ERKNS_9FloatRectE(Clip* self, const WebCore::FloatRect* rect) {
    ConstructClip(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList4ClipD0Ev(Clip* self) {
    DestroyClip(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4ClipD1Ev(Clip* self) {
    DestroyClip(self);
}

void APS5_VABI _ZN7WebCore11DisplayList4ClipD2Ev(Clip* self) {
    DestroyClip(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutC1ERKNS_9FloatRectE(ClipOut* self, const WebCore::FloatRect* rect) {
    ConstructClipOut(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutC2ERKNS_9FloatRectE(ClipOut* self, const WebCore::FloatRect* rect) {
    ConstructClipOut(self, rect);
}

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD0Ev(ClipOut* self) {
    DestroyClipOut(self);
    DeleteItem(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD1Ev(ClipOut* self) {
    DestroyClipOut(self);
}

void APS5_VABI _ZN7WebCore11DisplayList7ClipOutD2Ev(ClipOut* self) {
    DestroyClipOut(self);
}

}
