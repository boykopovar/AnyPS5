#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLengthAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore6LengthC1EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE(WebCore::Length* self, WTF::Ref<NKWebKit::CalculationAbi::CalculationValue>&& value);
void APS5_VABI _ZN7WebCore6LengthC2EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE(WebCore::Length* self, WTF::Ref<NKWebKit::CalculationAbi::CalculationValue>&& value);
void APS5_VABI _ZNK7WebCore6Length3refEv(const WebCore::Length* self);
void APS5_VABI _ZNK7WebCore6Length5derefEv(const WebCore::Length* self);
float APS5_VABI _ZNK7WebCore6Length21nonNanCalculatedValueEi(const WebCore::Length* self, int maxValue);

}
