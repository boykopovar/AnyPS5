#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CursorAbi.hpp"

extern "C" {

const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore10handCursorEv();
const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore11iBeamCursorEv();
const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore6Cursor8fromTypeENS0_4TypeE(NKWebKit::WebCoreAbi::CursorType type);

void APS5_VABI _ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE(NKWebKit::WebCoreAbi::Cursor* self, void* image, const WebCore::IntPoint* hotSpot);
void APS5_VABI _ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE(NKWebKit::WebCoreAbi::Cursor* self, void* image, const WebCore::IntPoint* hotSpot);
void APS5_VABI _ZNK7WebCore6Cursor19setAsPlatformCursorEv(const NKWebKit::WebCoreAbi::Cursor* self);

}
