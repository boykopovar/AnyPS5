#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

std::uint32_t APS5_VABI WKAXObjectPage(std::uint64_t a0);
std::uint32_t APS5_VABI WKBundleFrameGetJavaScriptContext(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI WKBundleFrameGetPendingUnloadCount(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundleFrameStopLoading(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundleGarbageCollectJavaScriptObjects(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundlePageClearMainFrameName(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI WKBundlePageIsTrackingRepaints(std::uint64_t a0);
void APS5_VABI WKBundlePageListenForLayoutMilestones(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundlePageOverlayClear(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundlePageResetTrackedRepaints(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundlePageSetAppCacheMaximumSize(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
void APS5_VABI WKBundlePageSetPolicyClient(std::uint64_t a0, std::uint64_t a1);
std::uint64_t APS5_VABI WKBundlePageSetTracksRepaints(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundleSetTabKeyCyclesThroughElements(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2);
void APS5_VABI WKContextSetConnectionClient(std::uint64_t a0, std::uint64_t a1);
std::uint32_t APS5_VABI WKPageGetContext(std::uint64_t a0);
void APS5_VABI WKViewSetViewAccessibilityClient(std::uint64_t a0, std::uint64_t a1);
std::uint8_t APS5_VABI WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(std::uint64_t a0);
std::uint64_t APS5_VABI _ZN3JSC21MarkedJSValueRefArrayD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3WTF24AutomaticThreadConditionD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3WTF24AutomaticThreadConditionD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore13GraphicsLayer18setBackgroundColorERKNS_5ColorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer13setSolidColorERKNS_5ColorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore4Page31setRemoteInspectionNameOverrideERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore8Document56hasRequestedPageSpecificStorageAccessWithUserInteractionERKNS_17RegistrableDomainE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI _ZN7WebCore8Settings23setUsesBackForwardCacheEb(std::uint64_t a0, std::uint64_t a1);
std::uint64_t APS5_VABI _ZN7WebCore8Settings24setVisualViewportEnabledEb(std::uint64_t a0, std::uint64_t a1);
std::uint64_t APS5_VABI _ZN7WebCore8Settings27setLoadsImagesAutomaticallyEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3);
std::uint64_t APS5_VABI _ZN7WebCore8Settings32setAcceleratedCompositingEnabledEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3);
std::uint64_t APS5_VABI _ZN7WebCore8Settings32setShouldAllowUserInstalledFontsEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3);
std::uint64_t APS5_VABI _ZN7WebCore8Settings33setFontFallbackPrefersPictographsEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3);
std::uint64_t APS5_VABI _ZN7WebCore8Settings41setAcceleratedCompositedAnimationsEnabledEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3);
std::uint64_t APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI _ZNK7WebCore13HitTestResult14innerNodeFrameEv(std::uint64_t a0);
std::uint32_t APS5_VABI _ZNK7WebCore18DOMWindowExtension5frameEv(std::uint64_t a0);
std::uint32_t APS5_VABI _ZNK7WebCore20ScrollingCoordinator36coordinatesScrollingForOverflowLayerERKNS_11RenderLayerE(std::uint64_t a0, std::uint64_t a1);
std::uint64_t APS5_VABI _ZNK7WebCore4Node7baseURIEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI _ZNK7WebCore8Document4pageEv(std::uint64_t a0);
std::uint32_t APS5_VABI _ZNK7WebCore9DOMWindow5frameEv(std::uint64_t a0);

}
