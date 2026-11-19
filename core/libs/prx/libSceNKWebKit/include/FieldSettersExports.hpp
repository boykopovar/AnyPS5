#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI _ZN3JSC14ProtoCallFrame12setCodeBlockEPNS_9CodeBlockE(void* self, std::uintptr_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame12setThisValueENS_7JSValueE(void* self, std::uintptr_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame15setGlobalObjectEPNS_14JSGlobalObjectE(void* self, std::uintptr_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame17setPaddedArgCountEj(void* self, std::uint32_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame29setArgumentCountIncludingThisEi(void* self, std::uint32_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame9setCalleeEPNS_8JSObjectE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore13GraphicsLayer9setClientERNS_19GraphicsLayerClientE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore18TextureMapperLayer15setBackingStoreEPNS_25TextureMapperBackingStoreE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore18TextureMapperLayer16setContentsLayerEPNS_26TextureMapperPlatformLayerE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore23ApplicationCacheStorage14setMaximumSizeEl(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore23ApplicationCacheStorage21setDefaultOriginQuotaEl(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeC1EPNS_15SQLiteStatementE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore29SQLiteStatementAutoResetScopeC2EPNS_15SQLiteStatementE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector21CSSFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector21CSSFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector21InspectorConsoleAgent12setHeapAgentEPNS_18InspectorHeapAgentE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector22HeapFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector22HeapFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24RemoteControllableTarget19setTargetIdentifierEj(void* self, std::uint32_t value);
void APS5_VABI _ZN9Inspector24TargetFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24TargetFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25BrowserFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25BrowserFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25ConsoleFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25ConsoleFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25RuntimeFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector25RuntimeFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26DatabaseFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26DatabaseFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27InspectorFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27InspectorFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27LayerTreeFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector27LayerTreeFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector28DOMStorageFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector28DOMStorageFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector32ScriptProfilerFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector32ScriptProfilerFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcherC1ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcherC2ERNS_14FrontendRouterE(void* self, std::uintptr_t value);
void APS5_VABI _ZN3JSC17DeferredWorkTimer16stopRunningTasksEv(void* self);
void APS5_VABI _ZN3JSC18GCActivityCallback10setEnabledEb(void* self, std::uint8_t value);
void APS5_VABI _ZN3JSC22FullGCActivityCallback16setDidGCRecentlyEv(void* self);
void APS5_VABI _ZN7WebCore10ScrollView23setPaintsEntireContentsEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore12EventHandler23setImmediateActionStageENS_20ImmediateActionStageE(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore15DatabaseManager14setIsAvailableEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore18TextureMapperLayer5setIDEj(void* self, std::uint32_t value);
void APS5_VABI _ZN7WebCore19InspectorController26setInspectorFrontendClientEPNS_23InspectorFrontendClientE(void* self, std::uintptr_t value);
void APS5_VABI _ZN7WebCore21NetworkStorageSession31setThirdPartyCookieBlockingModeENS_28ThirdPartyCookieBlockingModeE(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore21NetworkStorageSession32setResourceLoadStatisticsEnabledEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore21NetworkStorageSession44setResourceLoadStatisticsDebugLoggingEnabledEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore4Page19setDelegatesScalingEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore4Page27enableICECandidateFilteringEv(void* self);
void APS5_VABI _ZN7WebCore4Page28disableICECandidateFilteringEv(void* self);
void APS5_VABI _ZN7WebCore4Page37setInLowQualityImageInterpolationModeEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore8Document24setShouldCreateRenderersEb(void* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore9FrameView16setPaintBehaviorEN3WTF9OptionSetINS_13PaintBehaviorEEE(void* self, std::uint16_t value);
void APS5_VABI _ZN7WebCore9FrameView29setShouldUpdateWhileOffscreenEb(void* self, std::uint8_t value);
void APS5_VABI _ZN9Inspector15InspectorTarget5pauseEv(void* self);
}
