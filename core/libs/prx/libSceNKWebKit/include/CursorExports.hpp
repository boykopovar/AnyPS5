#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CursorAbi.hpp"

extern "C" {

const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore10handCursorEv();
const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore11iBeamCursorEv();
const NKWebKit::WebCoreAbi::Cursor& APS5_VABI _ZN7WebCore6Cursor8fromTypeENS0_4TypeE(NKWebKit::WebCoreAbi::CursorType type);

}
