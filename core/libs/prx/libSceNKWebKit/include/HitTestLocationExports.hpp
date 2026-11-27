#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HitTestLocationAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(NKWebKit::WebCoreAbi::LegacyHitTestLocation* self, const WebCore::FloatPoint* point);
void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(NKWebKit::WebCoreAbi::LegacyHitTestLocation* self, const WebCore::FloatPoint* point);

}
