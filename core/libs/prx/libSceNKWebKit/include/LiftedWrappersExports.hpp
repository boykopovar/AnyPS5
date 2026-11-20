#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

std::uint64_t APS5_VABI JSRunLoop(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI JSSetOptions(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI WKBundleClearAllDatabases(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC10JSFunctionC1ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC10JSFunctionC2ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC11VMInspector14dumpCellMemoryEPNS_6JSCellE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC12CachePayload16makeEmptyPayloadEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC12CachePayload17makeMallocPayloadEON3WTF9MallocPtrIhNS1_10FastMallocEEEm(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC16CompleteSubspaceD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopBytecodesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopFunctionsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC17DeferredWorkTimerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC19HeapSnapshotBuilderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC19InlineWatchpointSet11inflateSlowEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC22EdenGCActivityCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC32JSDestructibleObjectHeapCellTypeD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC8Bindings10RootObjectD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3JSC8DebuggerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3WTF18ExternalStringImpl6createEPKDsjONS_8FunctionIFvPS0_PvjEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN3WTF18ExternalStringImpl6createEPKhjONS_8FunctionIFvPS0_PvjEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore12ISOWebVTTCueD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore12NamedNodeMap12setNamedItemERNS_4AttrE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore12PrintContextD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore13MIMETypeCacheD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore14ResourceLoader14cancelledErrorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore14XMLHttpRequestD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore15DatabaseTracker23trackerWithDatabasePathERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore15PasteboardImageC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore15PasteboardImageC2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16CalculationValue6createESt10unique_ptrINS_18CalcExpressionNodeESt14default_deleteIS2_EENS_10ValueRangeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16DeviceMotionData6createEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16HTMLMediaElement19originsInMediaCacheERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore16ScriptController30executeScriptIgnoringExceptionERKN3WTF6StringEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore17CredentialStorage24getFromPersistentStorageERKNS_15ProtectionSpaceE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore17HTMLAnchorElement4textEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore17NowPlayingManagerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore18DOMWindowExtensionD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore19MediaResourceLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore22MutableStyleProperties6createENS_13CSSParserModeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC2ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore24CoordinatedGraphicsLayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore4Page12setIsVisibleEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore5FrameD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore6Editor26toggleOverwriteModeEnabledEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore7Element9outerTextEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore8Document11createRangeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore9DOMWindowD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore9FrameView17willEndLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCore9FrameView19willStartLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_11EventRegionE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector14InjectedScriptaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector14InspectorAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector15RemoteInspector27clientCapabilitiesDidChangeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector20InspectorTargetAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector21InjectedScriptManagerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector21InspectorConsoleAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZN9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK14OpaqueJSString6stringEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore11MediaPlayer15contentMIMETypeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore11MediaPlayer17contentTypeCodecsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore12NamedNodeMap12getNamedItemERKN3WTF10AtomStringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore12NamedNodeMap14getNamedItemNSERKN3WTF10AtomStringES4_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore15JSDOMWindowBase22scriptExecutionContextEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore4Page9selectionEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint8_t APS5_VABI _ZNK7WebCore6Editor7canEditEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZNK7WebCore9FrameView19baseBackgroundColorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint32_t APS5_VABI _ZNK9Inspector20InspectorTargetAgent14connectionTypeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn112_N7WebCore9DOMWindowD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn16_N9Inspector14InspectorAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn16_N9Inspector20InspectorTargetAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn16_N9Inspector21InspectorConsoleAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn16_N9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn24_N7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn24_N7WebCore19MediaResourceLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn24_N9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn32_N7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn32_N7WebCore14XMLHttpRequestD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn64_N7WebCore9FrameView17willEndLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
std::uint64_t APS5_VABI _ZThn64_N7WebCore9FrameView19willStartLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);

}
