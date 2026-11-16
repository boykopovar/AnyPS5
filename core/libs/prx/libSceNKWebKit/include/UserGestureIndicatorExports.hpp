#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/UserGestureIndicatorAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore20UserGestureIndicatorC1EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE(WebCore::UserGestureIndicator* self, const NKWebKit::WebCoreAbi::UserGestureTokenRef& token, WebCore::GestureScope scope, WebCore::IsPropagatedFromFetch isPropagatedFromFetch);
void APS5_VABI _ZN7WebCore20UserGestureIndicatorC2EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE(WebCore::UserGestureIndicator* self, const NKWebKit::WebCoreAbi::UserGestureTokenRef& token, WebCore::GestureScope scope, WebCore::IsPropagatedFromFetch isPropagatedFromFetch);
void APS5_VABI _ZN7WebCore20UserGestureIndicatorD1Ev(WebCore::UserGestureIndicator* self);
void APS5_VABI _ZN7WebCore20UserGestureIndicatorD2Ev(WebCore::UserGestureIndicator* self);
void APS5_VABI _ZN7WebCore16UserGestureTokenD1Ev(WebCore::UserGestureToken* self);
void APS5_VABI _ZN7WebCore16UserGestureTokenD2Ev(WebCore::UserGestureToken* self);

}
