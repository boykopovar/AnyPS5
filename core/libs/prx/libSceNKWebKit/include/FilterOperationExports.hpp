#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"

extern "C" {

WTF::Ref<WebCore::DefaultFilterOperation> APS5_VABI _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(std::uint32_t representedType);
void APS5_VABI _ZN7WebCore22DefaultFilterOperationC1ENS_15FilterOperation13OperationTypeE(WebCore::DefaultFilterOperation* self, std::uint32_t representedType);
void APS5_VABI _ZN7WebCore22DefaultFilterOperationC2ENS_15FilterOperation13OperationTypeE(WebCore::DefaultFilterOperation* self, std::uint32_t representedType);
WTF::RefPtr<WebCore::FilterOperation> APS5_VABI _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(WebCore::InvertLightnessFilterOperation* self, const WebCore::FilterOperation* from, double progress, bool blendToPassthrough);
bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(const WebCore::InvertLightnessFilterOperation* self, WebCore::SRGBA<float>& color);
bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(const WebCore::InvertLightnessFilterOperation* self, WebCore::SRGBA<float>& color);

}
