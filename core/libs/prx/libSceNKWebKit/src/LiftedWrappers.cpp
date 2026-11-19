#include "prx/libSceNKWebKit/include/LiftedWrappersExports.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;
using NKWebKit::Lifted::Call;

extern "C" {

void _ZN3JSC11VMInspector22dumpCellMemoryToStreamEPNS_6JSCellERN3WTF11PrintStreamE();
void _ZN3JSC12HeapCellTypeD1Ev();
void _ZN3JSC14JSRunLoopTimerD1Ev();
void _ZN3JSC16CompleteSubspaceD1Ev();
void _ZN3JSC16SamplingProfiler18reportTopBytecodesERN3WTF11PrintStreamE();
void _ZN3JSC16SamplingProfiler18reportTopFunctionsERN3WTF11PrintStreamE();
void _ZN3JSC17DeferredWorkTimerD2Ev();
void _ZN3JSC19HeapSnapshotBuilderD1Ev();
void _ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectE();
void _ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectERKN3WTF6StringE();
void _ZN3JSC2VM14throwExceptionEPNS_14JSGlobalObjectENS_7JSValueE();
void _ZN3JSC7Options10initializeEv();
void _ZN3JSC7Options10setOptionsEPKc();
void _ZN3JSC8Bindings10RootObjectD1Ev();
void _ZN3JSC8DebuggerD1Ev();
void _ZN3JSC8JSCalleeC1ERNS_2VMEPNS_14JSGlobalObjectEPNS_9StructureE();
void _ZN3WTF10fastMallocEm();
void _ZN3WTF20initializeMainThreadEv();
void _ZN3WTF3URL10invalidateEv();
void _ZN3WTF7RunLoop3runEv();
void _ZN3WTF8dataFileEv();
void _ZN3WTF8fastFreeEPv();
void _ZN7WebCore11ContentType15codecsParameterEv();
void _ZN7WebCore11MediaPlayer19originsInMediaCacheERKN3WTF6StringE();
void _ZN7WebCore11MediaPlayerD1Ev();
void _ZN7WebCore12ISOWebVTTCueD2Ev();
void _ZN7WebCore12PrintContextD1Ev();
void _ZN7WebCore13MIMETypeCacheD2Ev();
void _ZN7WebCore14CredentialBaseC1Ev();
void _ZN7WebCore14DocumentLoaderC1ERKNS_15ResourceRequestERKNS_14SubstituteDataE();
void _ZN7WebCore14DocumentLoaderD1Ev();
void _ZN7WebCore14FrameSelection16updateAppearanceEv();
void _ZN7WebCore14JSVoidCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE();
void _ZN7WebCore14JSVoidCallbackD1Ev();
void _ZN7WebCore14SQLiteDatabaseC1Ev();
void _ZN7WebCore14ScrollableArea17willEndLiveResizeEv();
void _ZN7WebCore14ScrollableArea19willStartLiveResizeEv();
void _ZN7WebCore14XMLHttpRequestD1Ev();
void _ZN7WebCore15DatabaseTracker29deleteAllDatabasesImmediatelyEv();
void _ZN7WebCore15DatabaseTracker9singletonEv();
void _ZN7WebCore16JSStringCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE();
void _ZN7WebCore16JSStringCallbackD1Ev();
void _ZN7WebCore16ScriptController37executeScriptInWorldIgnoringExceptionERNS_15DOMWrapperWorldERKN3WTF6StringEb();
void _ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringEN3JSC7JSValueE();
void _ZN7WebCore17NowPlayingManagerD1Ev();
void _ZN7WebCore18DOMWindowExtensionD1Ev();
void _ZN7WebCore18TextureMapperLayerD1Ev();
void _ZN7WebCore19MediaResourceLoaderD1Ev();
void _ZN7WebCore21mainThreadNormalWorldEv();
void _ZN7WebCore24CoordinatedGraphicsLayerD1Ev();
void _ZN7WebCore27AuthenticationChallengeBaseC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE();
void _ZN7WebCore27AuthenticationChallengeBaseC1Ev();
void _ZN7WebCore4Page16setActivityStateEN3WTF9OptionSetINS_13ActivityState4FlagEEE();
void _ZN7WebCore5ColorC1ERKS0_();
void _ZN7WebCore5FrameD1Ev();
void _ZN7WebCore5Range6createERNS_8DocumentE();
void _ZN7WebCore7Element16getAttributeNodeERKN3WTF10AtomStringE();
void _ZN7WebCore7Element16setAttributeNodeERNS_4AttrE();
void _ZN7WebCore7Element18getAttributeNodeNSERKN3WTF10AtomStringES4_();
void _ZN7WebCore7Element9innerTextEv();
void _ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE();
void _ZN7WebCore9DOMWindowD1Ev();
void _ZN7WebCore9FrameView26adjustTiledBackingCoverageEv();
void _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE();
void _ZN9Inspector14InspectorAgentD2Ev();
void _ZN9Inspector15RemoteInspector16pushListingsSoonEv();
void _ZN9Inspector15RemoteInspector24updateClientCapabilitiesEv();
void _ZN9Inspector18InjectedScriptBaseaSERKS0_();
void _ZN9Inspector20InspectorTargetAgentD1Ev();
void _ZN9Inspector21InjectedScriptManagerD1Ev();
void _ZN9Inspector21InspectorConsoleAgentD1Ev();
void _ZN9Inspector28InspectorScriptProfilerAgentD1Ev();
void _ZNK7WebCore11ContentType13containerTypeEv();
void _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE();
void _ZNK7WebCore11FrameLoader14cancelledErrorERKNS_15ResourceRequestE();
void _ZNK7WebCore15FocusController18focusedOrMainFrameEv();
void _ZNK7WebCore16VisibleSelection19rootEditableElementEv();
void _ZNK7WebCore4Node11textContentEb();
void _ZNK7WebCore9DOMWindow8documentEv();
void _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv();
void _ZNKR3WTF6String12isolatedCopyEv();

std::uint64_t APS5_VABI JSRunLoop(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3WTF20initializeMainThreadEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF7RunLoop3runEv, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI JSSetOptions(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC7Options10initializeEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3JSC7Options10setOptionsEPKc, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI WKBundleClearAllDatabases(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN7WebCore15DatabaseTracker9singletonEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore15DatabaseTracker29deleteAllDatabasesImmediatelyEv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC10JSFunctionC1ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3JSC8JSCalleeC1ERNS_2VMEPNS_14JSGlobalObjectEPNS_9StructureE, a0, a1, a3, a4, a4, a5);
    Write<std::uint64_t>(a0 + 0x18, a2);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC10JSFunctionC2ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3JSC8JSCalleeC1ERNS_2VMEPNS_14JSGlobalObjectEPNS_9StructureE, a0, a1, a3, a4, a4, a5);
    Write<std::uint64_t>(a0 + 0x18, a2);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC11VMInspector14dumpCellMemoryEPNS_6JSCellE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF8dataFileEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3JSC11VMInspector22dumpCellMemoryToStreamEPNS_6JSCellERN3WTF11PrintStreamE, a0, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC12CachePayload16makeEmptyPayloadEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint32_t>(a0 + 0x10, std::uint32_t{0x1});
    Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, std::uint64_t{0x0}, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN3JSC12CachePayload17makeMallocPayloadEON3WTF9MallocPtrIhNS1_10FastMallocEEEm(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    Write<std::uint64_t>(a0 + 0x8, a2);
    Write<std::uint32_t>(a0 + 0x10, std::uint32_t{0x1});
    Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, std::uint64_t{0x0}, a1, a2, a3, a4, a5);
    Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZN3JSC16CompleteSubspaceD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC16CompleteSubspaceD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopBytecodesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF8dataFileEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3JSC16SamplingProfiler18reportTopBytecodesERN3WTF11PrintStreamE, a0, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopFunctionsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF8dataFileEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3JSC16SamplingProfiler18reportTopFunctionsERN3WTF11PrintStreamE, a0, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC17DeferredWorkTimerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC17DeferredWorkTimerD2Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC19HeapSnapshotBuilderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC19HeapSnapshotBuilderD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC19InlineWatchpointSet11inflateSlowEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x14}, a1, a2, a3, a4, a5);
    const std::uint64_t v5 = v4 + 0x6;
    Write<std::uint32_t>(v4, std::uint32_t{0x1});
    Write<std::uint8_t>(v4 + 0x4, static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1 >> 1) & std::uint8_t{0x3}));
    Write<std::uint8_t>(v4 + 0x5, std::uint8_t{0x0});
    Write<std::uint32_t>(v4 + 0xc, static_cast<std::uint32_t>(v5));
    const std::uint64_t v6 = v5 >> 32;
    Write<std::uint16_t>(v4 + 0x10, static_cast<std::uint16_t>(v6));
    Write<std::uint32_t>(v4 + 0x6, static_cast<std::uint32_t>(v5));
    Write<std::uint16_t>(v4 + 0xa, static_cast<std::uint16_t>(v6));
    Write<std::uint64_t>(a0, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectE, a0, a1, a2, a3, a4, a5);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    return Call<std::uint64_t>(_ZN3JSC2VM14throwExceptionEPNS_14JSGlobalObjectENS_7JSValueE, v2, a0, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectERKN3WTF6StringE, a0, a2, a2, a3, a4, a5);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    return Call<std::uint64_t>(_ZN3JSC2VM14throwExceptionEPNS_14JSGlobalObjectENS_7JSValueE, v2, a0, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC22EdenGCActivityCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC14JSRunLoopTimerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC32JSDestructibleObjectHeapCellTypeD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC12HeapCellTypeD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC8Bindings10RootObjectD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC8Bindings10RootObjectD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC8DebuggerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN3JSC8DebuggerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3WTF18ExternalStringImpl6createEPKDsjONS_8FunctionIFvPS0_PvjEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x20}, a1, a2, a3, a4, a5);
    Write<std::uint32_t>(v2, std::uint32_t{0x2});
    Write<std::uint32_t>(v2 + 0x4, static_cast<std::uint32_t>(a2));
    Write<std::uint64_t>(v2 + 0x8, a1);
    Write<std::uint32_t>(v2 + 0x10, std::uint32_t{0x0});
    const std::uint64_t v3 = Read<std::uint64_t>(a3);
    Write<std::uint64_t>(a3, std::uint64_t{0x0});
    Write<std::uint64_t>(v2 + 0x18, v3);
    const std::uint8_t v4 = Read<std::uint8_t>(v2 + 0x10);
    Write<std::uint8_t>(v2 + 0x10, static_cast<std::uint8_t>(v4 | std::uint8_t{0x3}));
    Write<std::uint64_t>(a0, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZN3WTF18ExternalStringImpl6createEPKhjONS_8FunctionIFvPS0_PvjEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x20}, a1, a2, a3, a4, a5);
    Write<std::uint32_t>(v2, std::uint32_t{0x2});
    Write<std::uint32_t>(v2 + 0x4, static_cast<std::uint32_t>(a2));
    Write<std::uint64_t>(v2 + 0x8, a1);
    Write<std::uint32_t>(v2 + 0x10, std::uint32_t{0x4});
    const std::uint64_t v3 = Read<std::uint64_t>(a3);
    Write<std::uint64_t>(a3, std::uint64_t{0x0});
    Write<std::uint64_t>(v2 + 0x18, v3);
    const std::uint8_t v4 = Read<std::uint8_t>(v2 + 0x10);
    Write<std::uint8_t>(v2 + 0x10, static_cast<std::uint8_t>(v4 | std::uint8_t{0x3}));
    Write<std::uint64_t>(a0, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore11MediaPlayerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore12ISOWebVTTCueD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore12ISOWebVTTCueD2Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore12NamedNodeMap12setNamedItemERNS_4AttrE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore7Element16setAttributeNodeERNS_4AttrE, a0, v1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore12PrintContextD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore12PrintContextD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore13MIMETypeCacheD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore13MIMETypeCacheD2Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14DocumentLoaderD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x20}, a1, a2, a3, a4, a5);
    Call<std::uint64_t>(_ZN7WebCore14JSVoidCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE, v1, a1, a2, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14JSVoidCallbackD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore14ResourceLoader14cancelledErrorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x20);
    const std::uint64_t v3 = Read<std::uint64_t>(v1 + 0xa0);
    Call<std::uint64_t>(_ZNK7WebCore11FrameLoader14cancelledErrorERKNS_15ResourceRequestE, a0, v3, (a1 + 0x158), a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore14XMLHttpRequestD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14XMLHttpRequestD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore15DatabaseTracker23trackerWithDatabasePathERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x88}, a1, a2, a3, a4, a5);
    Write<std::uint8_t>(v1, std::uint8_t{0x0});
    Write<std::uint64_t>(v1 + 0x8, std::uint64_t{0x0});
    Write<std::uint8_t>(v1 + 0x10, std::uint8_t{0x0});
    Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (v1 + 0x18), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Write<std::uint64_t>(v1 + 0x58, std::uint64_t{0x0});
    Call<std::uint64_t>(_ZNKR3WTF6String12isolatedCopyEv, (v1 + 0x60), a1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    WriteBytes<32>(v1 + 0x68, Bytes<32>{});
    Write<std::uint64_t>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore15PasteboardImageC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    WriteBytes<16>(a0, Bytes<16>{});
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x8}), a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x74, std::uint64_t{0x0});
    WriteBytes<32>(a0 + 0x30, Bytes<32>{});
    WriteBytes<32>(a0 + 0x50, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x70, std::uint16_t{0x0});
    return v2;
}

std::uint64_t APS5_VABI _ZN7WebCore15PasteboardImageC2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    WriteBytes<16>(a0, Bytes<16>{});
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x8}), a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x74, std::uint64_t{0x0});
    WriteBytes<32>(a0 + 0x30, Bytes<32>{});
    WriteBytes<32>(a0 + 0x50, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x70, std::uint16_t{0x0});
    return v2;
}

std::uint64_t APS5_VABI _ZN7WebCore16CalculationValue6createESt10unique_ptrINS_18CalcExpressionNodeESt14default_deleteIS2_EENS_10ValueRangeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x18}, a1, a2, a3, a4, a5);
    const std::uint64_t v3 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint32_t>(v2, std::uint32_t{0x1});
    Write<std::uint64_t>(v2 + 0x8, v3);
    Write<std::uint8_t>(v2 + 0x10, static_cast<std::uint8_t>(static_cast<std::uint8_t>(static_cast<std::uint32_t>(a2)) == std::uint8_t{0x1}));
    Write<std::uint64_t>(a0, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore16DeviceMotionData6createEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x30}, a1, a2, a3, a4, a5);
    Write<std::uint32_t>(v1, std::uint32_t{0x1});
    Write<std::uint8_t>(v1 + 0x28, std::uint8_t{0x0});
    WriteBytes<16>(v1 + 0x8, Bytes<16>{});
    WriteBytes<16>(v1 + 0x11, Bytes<16>{});
    Write<std::uint64_t>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore16HTMLMediaElement19originsInMediaCacheERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore11MediaPlayer19originsInMediaCacheERKN3WTF6StringE, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x20}, a1, a2, a3, a4, a5);
    Call<std::uint64_t>(_ZN7WebCore16JSStringCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE, v1, a1, a2, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallbackD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore16JSStringCallbackD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore16ScriptController30executeScriptIgnoringExceptionERKN3WTF6StringEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN7WebCore21mainThreadNormalWorldEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore16ScriptController37executeScriptInWorldIgnoringExceptionERNS_15DOMWrapperWorldERKN3WTF6StringEb, a0, v2, a1, static_cast<std::uint64_t>(static_cast<std::uint32_t>(a2)), std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore17CredentialStorage24getFromPersistentStorageERKNS_15ProtectionSpaceE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14CredentialBaseC1Ev, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore17HTMLAnchorElement4textEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a2;
    Call<std::uint64_t>(_ZNK7WebCore4Node11textContentEb, a0, a1, std::uint64_t{0x0}, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    Call<std::uint64_t>(_ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringEN3JSC7JSValueE, a0, a1, a2, std::uint64_t{0x0}, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore17NowPlayingManagerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore17NowPlayingManagerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore18DOMWindowExtensionD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore18DOMWindowExtensionD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore18TextureMapperLayerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore19MediaResourceLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore19MediaResourceLoaderD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0xa78}, a1, a2, a3, a4, a5);
    Call<std::uint64_t>(_ZN7WebCore14DocumentLoaderC1ERKNS_15ResourceRequestERKNS_14SubstituteDataE, v1, a2, a3, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore22MutableStyleProperties6createENS_13CSSParserModeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN3WTF10fastMallocEm, std::uint64_t{0x60}, a1, a2, a3, a4, a5);
    Write<std::uint32_t>(v2, std::uint32_t{0x1});
    Write<std::uint32_t>(v2 + 0x4, ((static_cast<std::uint32_t>(a1) & std::uint32_t{0x7}) | std::uint32_t{0x8}));
    Write<std::uint64_t>(v2 + 0x8, (v2 + 0x18));
    Write<std::uint64_t>(v2 + 0x10, std::uint64_t{0x4});
    Write<std::uint64_t>(v2 + 0x58, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore27AuthenticationChallengeBaseC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE, a0, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x1d0, std::uint64_t{0x0});
    Write<std::uint16_t>(a0 + 0x1d8, std::uint16_t{0x0});
    Write<std::uint64_t>(a0 + 0x1e0, std::uint64_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x1e0}), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore27AuthenticationChallengeBaseC1Ev, a0, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x1d0, std::uint64_t{0x0});
    Write<std::uint16_t>(a0 + 0x1d8, std::uint16_t{0x0});
    Write<std::uint64_t>(a0 + 0x1e0, std::uint64_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x1e0}), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC2ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore27AuthenticationChallengeBaseC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE, a0, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x1d0, std::uint64_t{0x0});
    Write<std::uint16_t>(a0 + 0x1d8, std::uint16_t{0x0});
    Write<std::uint64_t>(a0 + 0x1e0, std::uint64_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x1e0}), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore23AuthenticationChallengeC2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore27AuthenticationChallengeBaseC1Ev, a0, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0 + 0x1d0, std::uint64_t{0x0});
    Write<std::uint16_t>(a0 + 0x1d8, std::uint16_t{0x0});
    Write<std::uint64_t>(a0 + 0x1e0, std::uint64_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, (a0 + std::uint64_t{0x1e0}), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore24CoordinatedGraphicsLayerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore24CoordinatedGraphicsLayerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore4Page12setIsVisibleEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x214);
    const std::uint64_t v5 = Call<std::uint64_t>(_ZN7WebCore4Page16setActivityStateEN3WTF9OptionSetINS_13ActivityState4FlagEEE, a0, static_cast<std::uint64_t>((((static_cast<std::uint32_t>(a1) != 0) ? std::uint32_t{0xc} : std::uint32_t{0x20}) | (v1 & std::uint32_t{0xffffffd3}))), a2, std::uint64_t{0xc}, a4, a5);
    return v5;
}

std::uint64_t APS5_VABI _ZN7WebCore5FrameD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore5FrameD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore6Editor26toggleOverwriteModeEnabledEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    (void)a1;
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x64);
    const std::uint8_t v2 = static_cast<std::uint8_t>(v1 ^ std::uint8_t{0x1});
    Write<std::uint8_t>(a0 + 0x64, v2);
    const std::uint64_t v4 = Read<std::uint64_t>(a0);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x9a8);
    const std::uint8_t v6 = Read<std::uint8_t>(v5 + 0x14c);
    const std::uint8_t v7 = static_cast<std::uint8_t>(v6 & std::uint8_t{0xdf});
    const std::uint8_t v9 = static_cast<std::uint8_t>(v7 | static_cast<std::uint8_t>(v2 << 5));
    Write<std::uint8_t>(v5 + 0x14c, v9);
    const std::uint64_t v11 = Read<std::uint64_t>(v5 + 0x18);
    const std::uint64_t v12 = Read<std::uint64_t>(v11 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE, v12, std::uint64_t{0x0}, a2, ((((((v4 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v6)) & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v7)) & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v9)), a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore14FrameSelection16updateAppearanceEv, v5, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore7Element9outerTextEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore7Element9innerTextEv, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore8Document11createRangeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore5Range6createERNS_8DocumentE, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore9DOMWindowD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore9DOMWindowD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore9FrameView17willEndLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14ScrollableArea17willEndLiveResizeEv, (a0 + std::uint64_t{0x40}), a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore9FrameView26adjustTiledBackingCoverageEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore9FrameView19willStartLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14ScrollableArea19willStartLiveResizeEv, (a0 + std::uint64_t{0x40}), a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore9FrameView26adjustTiledBackingCoverageEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_11EventRegionE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN9Inspector14InjectedScriptaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector18InjectedScriptBaseaSERKS0_, a0, a1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZN9Inspector14InspectorAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector14InspectorAgentD2Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN9Inspector15RemoteInspector27clientCapabilitiesDidChangeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector15RemoteInspector24updateClientCapabilitiesEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN9Inspector15RemoteInspector16pushListingsSoonEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN9Inspector20InspectorTargetAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector20InspectorTargetAgentD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN9Inspector21InjectedScriptManagerD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector21InjectedScriptManagerD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN9Inspector21InspectorConsoleAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector21InspectorConsoleAgentD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN9Inspector28InspectorScriptProfilerAgentD1Ev, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZNK14OpaqueJSString6stringEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZNKR3WTF6String12isolatedCopyEv, a0, (a1 + std::uint64_t{0x8}), a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore11MediaPlayer15contentMIMETypeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZNK7WebCore11ContentType13containerTypeEv, a0, (a1 + std::uint64_t{0x88}), a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore11MediaPlayer17contentTypeCodecsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN7WebCore11ContentType15codecsParameterEv, a0, a1, a2, a3, a4, a5);
    Call<std::uint64_t>(_ZNK7WebCore11ContentType9parameterERKN3WTF6StringE, a0, (a1 + std::uint64_t{0x88}), v2, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore12NamedNodeMap12getNamedItemERKN3WTF10AtomStringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore7Element16getAttributeNodeERKN3WTF10AtomStringE, a0, v1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore12NamedNodeMap14getNamedItemNSERKN3WTF10AtomStringES4_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore7Element18getAttributeNodeNSERKN3WTF10AtomStringES4_, a0, v1, a2, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore15JSDOMWindowBase22scriptExecutionContextEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xeb8);
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore9DOMWindow8documentEv, v1, a1, a2, a3, a4, a5);
    return ((v2 != 0) ? (v2 + 0xb0) : v2);
}

std::uint64_t APS5_VABI _ZNK7WebCore4Page9selectionEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    const std::uint16_t v2 = Read<std::uint16_t>(v1);
    Write<std::uint16_t>(v1, static_cast<std::uint16_t>(v2 + 1));
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore15FocusController18focusedOrMainFrameEv, v1, a1, a2, a3, a4, a5);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0xc0);
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x9a8);
    const std::uint16_t v7 = Read<std::uint16_t>(v1);
    Write<std::uint16_t>(v1, static_cast<std::uint16_t>(v7 - 1));
    return (v6 + std::uint64_t{0x30});
}

std::uint8_t APS5_VABI _ZNK7WebCore6Editor7canEditEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x9a8);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore16VisibleSelection19rootEditableElementEv, (v2 + std::uint64_t{0x30}), a1, a2, a3, a4, a5);
    return static_cast<std::uint8_t>(v4 != 0);
}

std::uint64_t APS5_VABI _ZNK7WebCore9FrameView19baseBackgroundColorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore5ColorC1ERKS0_, a0, (a1 + std::uint64_t{0x2f8}), a2, a3, a4, a5);
    return a0;
}

std::uint32_t APS5_VABI _ZNK9Inspector20InspectorTargetAgent14connectionTypeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv, v1, a1, a2, a3, a4, a5);
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(v2));
}

std::uint64_t APS5_VABI _ZThn112_N7WebCore9DOMWindowD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffff90};
    Call<std::uint64_t>(_ZN7WebCore9DOMWindowD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn16_N9Inspector14InspectorAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xfffffffffffffff0};
    Call<std::uint64_t>(_ZN9Inspector14InspectorAgentD2Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn16_N9Inspector20InspectorTargetAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xfffffffffffffff0};
    Call<std::uint64_t>(_ZN9Inspector20InspectorTargetAgentD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn16_N9Inspector21InspectorConsoleAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xfffffffffffffff0};
    Call<std::uint64_t>(_ZN9Inspector21InspectorConsoleAgentD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn16_N9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xfffffffffffffff0};
    Call<std::uint64_t>(_ZN9Inspector28InspectorScriptProfilerAgentD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn24_N7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffffe8};
    Call<std::uint64_t>(_ZN7WebCore14DocumentLoaderD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn24_N7WebCore19MediaResourceLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffffe8};
    Call<std::uint64_t>(_ZN7WebCore19MediaResourceLoaderD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn24_N9Inspector28InspectorScriptProfilerAgentD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffffe8};
    Call<std::uint64_t>(_ZN9Inspector28InspectorScriptProfilerAgentD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn32_N7WebCore14DocumentLoaderD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffffe0};
    Call<std::uint64_t>(_ZN7WebCore14DocumentLoaderD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn32_N7WebCore14XMLHttpRequestD0Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xffffffffffffffe0};
    Call<std::uint64_t>(_ZN7WebCore14XMLHttpRequestD1Ev, v1, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn64_N7WebCore9FrameView17willEndLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14ScrollableArea17willEndLiveResizeEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore9FrameView26adjustTiledBackingCoverageEv, (a0 - 0x40), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZThn64_N7WebCore9FrameView19willStartLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Call<std::uint64_t>(_ZN7WebCore14ScrollableArea19willStartLiveResizeEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore9FrameView26adjustTiledBackingCoverageEv, (a0 - 0x40), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

}
