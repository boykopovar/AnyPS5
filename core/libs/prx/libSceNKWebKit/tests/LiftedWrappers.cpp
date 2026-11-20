#include "prx/libSceNKWebKit/include/LiftedWrappersExports.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LiftedWrappers check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Wrapper = std::uint64_t (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

static std::uint64_t APS5_VABI Thunk0(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return JSRunLoop(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk1(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return JSSetOptions(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleClearAllDatabases(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk3(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC10JSFunctionC1ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk4(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC10JSFunctionC2ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk5(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC11VMInspector14dumpCellMemoryEPNS_6JSCellE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk6(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC12CachePayload16makeEmptyPayloadEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk7(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC12CachePayload17makeMallocPayloadEON3WTF9MallocPtrIhNS1_10FastMallocEEEm(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk8(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC16CompleteSubspaceD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk9(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC16SamplingProfiler18reportTopBytecodesEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk10(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC16SamplingProfiler18reportTopFunctionsEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk11(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC17DeferredWorkTimerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk12(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC19HeapSnapshotBuilderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk13(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC19InlineWatchpointSet11inflateSlowEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk14(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk15(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk16(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC22EdenGCActivityCallbackD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk17(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC32JSDestructibleObjectHeapCellTypeD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk18(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC8Bindings10RootObjectD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk19(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC8DebuggerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk20(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3WTF18ExternalStringImpl6createEPKDsjONS_8FunctionIFvPS0_PvjEEE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk21(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3WTF18ExternalStringImpl6createEPKhjONS_8FunctionIFvPS0_PvjEEE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk22(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore11MediaPlayerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk23(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore12ISOWebVTTCueD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk24(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore12NamedNodeMap12setNamedItemERNS_4AttrE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk25(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore12PrintContextD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk26(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore13MIMETypeCacheD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk27(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore14DocumentLoaderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk28(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore14JSVoidCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk29(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore14JSVoidCallbackD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk30(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore14ResourceLoader14cancelledErrorEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk31(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore14XMLHttpRequestD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk32(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore15DatabaseTracker23trackerWithDatabasePathERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk33(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore15PasteboardImageC1Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk34(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore15PasteboardImageC2Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk35(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16CalculationValue6createESt10unique_ptrINS_18CalcExpressionNodeESt14default_deleteIS2_EENS_10ValueRangeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk36(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16DeviceMotionData6createEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk37(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16HTMLMediaElement19originsInMediaCacheERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk38(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16JSStringCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk39(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16JSStringCallbackD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk40(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore16ScriptController30executeScriptIgnoringExceptionERKN3WTF6StringEb(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk41(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore17CredentialStorage24getFromPersistentStorageERKNS_15ProtectionSpaceE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk42(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore17HTMLAnchorElement4textEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk43(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk44(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore17NowPlayingManagerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk45(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore18DOMWindowExtensionD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk46(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore18TextureMapperLayerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk47(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore19MediaResourceLoaderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk48(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk49(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore22MutableStyleProperties6createENS_13CSSParserModeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk50(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23AuthenticationChallengeC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk51(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23AuthenticationChallengeC1Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk52(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23AuthenticationChallengeC2ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk53(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23AuthenticationChallengeC2Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk54(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore24CoordinatedGraphicsLayerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk55(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4Page12setIsVisibleEb(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk56(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore5FrameD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk57(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore6Editor26toggleOverwriteModeEnabledEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk58(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore7Element9outerTextEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk59(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Document11createRangeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk60(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore9DOMWindowD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk61(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore9FrameView17willEndLiveResizeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk62(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore9FrameView19willStartLiveResizeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk63(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_11EventRegionE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk64(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector14InjectedScriptaSERKS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk65(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector14InspectorAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk66(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector15RemoteInspector27clientCapabilitiesDidChangeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk67(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector20InspectorTargetAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk68(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector21InjectedScriptManagerD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk69(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector21InspectorConsoleAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk70(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN9Inspector28InspectorScriptProfilerAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk71(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK14OpaqueJSString6stringEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk72(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore11MediaPlayer15contentMIMETypeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk73(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore11MediaPlayer17contentTypeCodecsEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk74(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore12NamedNodeMap12getNamedItemERKN3WTF10AtomStringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk75(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore12NamedNodeMap14getNamedItemNSERKN3WTF10AtomStringES4_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk76(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore15JSDOMWindowBase22scriptExecutionContextEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk77(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore4Page9selectionEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk78(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore6Editor7canEditEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk79(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore9FrameView19baseBackgroundColorEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk80(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK9Inspector20InspectorTargetAgent14connectionTypeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk81(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn112_N7WebCore9DOMWindowD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk82(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn16_N9Inspector14InspectorAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk83(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn16_N9Inspector20InspectorTargetAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk84(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn16_N9Inspector21InspectorConsoleAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk85(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn16_N9Inspector28InspectorScriptProfilerAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk86(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn24_N7WebCore14DocumentLoaderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk87(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn24_N7WebCore19MediaResourceLoaderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk88(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn24_N9Inspector28InspectorScriptProfilerAgentD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk89(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn32_N7WebCore14DocumentLoaderD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk90(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn32_N7WebCore14XMLHttpRequestD0Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk91(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn64_N7WebCore9FrameView17willEndLiveResizeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk92(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZThn64_N7WebCore9FrameView19willStartLiveResizeEv(a0, a1, a2, a3, a4, a5);
}

#if defined(__linux__)
#include <dlfcn.h>
#include <sys/mman.h>

namespace {

constexpr std::size_t ArenaSize = 1 << 18;
constexpr std::uint64_t ArenaAddress = 0x5a0000000000ull;

struct Random {
    std::uint64_t state;
    std::uint64_t Next() {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
};

std::uint64_t Pointer(Random& random, std::uint64_t base) {
    return base + 0x1000 + (random.Next() % (ArenaSize - 0x2000)) / 8 * 8;
}

struct Digest {
    std::uint64_t value = 0xcbf29ce484222325ull;
    std::uint64_t base = 0;
    void Word(std::uint64_t word) {
        if (word - base < ArenaSize) word = 0xa7e0000000000000ull | (word - base);
        for (int i = 0; i < 8; ++i) value = (value ^ static_cast<unsigned char>(word >> (8 * i))) * 0x100000001b3ull;
    }
};

struct CallRecord {
    std::uint32_t id;
    std::uint64_t args[6];
};

bool armed = false;
Random mockRandom{0};
std::uint64_t arenaBase = 0;
std::vector<CallRecord> calls;

}  // namespace

static std::uint64_t Mock(std::uint32_t id, const char* name, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (!armed) {
        auto* real = reinterpret_cast<Wrapper>(dlsym(RTLD_NEXT, name));
        Require(real != nullptr);
        return real(a0, a1, a2, a3, a4, a5);
    }
    calls.push_back({id, {a0, a1, a2, a3, a4, a5}});
    const std::uint64_t pick = mockRandom.Next() % 100;
    const std::uint64_t value = pick < 50 ? Pointer(mockRandom, arenaBase) : pick < 75 ? 0 : mockRandom.Next();
    mockRandom.Next();
    return value;
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC11VMInspector22dumpCellMemoryToStreamEPNS_6JSCellERN3WTF11PrintStreamE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(0, "_ZN3JSC11VMInspector22dumpCellMemoryToStreamEPNS_6JSCellERN3WTF11PrintStreamE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC12HeapCellTypeD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(1, "_ZN3JSC12HeapCellTypeD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC14JSRunLoopTimerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(2, "_ZN3JSC14JSRunLoopTimerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC16CompleteSubspaceD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(3, "_ZN3JSC16CompleteSubspaceD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopBytecodesERN3WTF11PrintStreamE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(4, "_ZN3JSC16SamplingProfiler18reportTopBytecodesERN3WTF11PrintStreamE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC16SamplingProfiler18reportTopFunctionsERN3WTF11PrintStreamE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(5, "_ZN3JSC16SamplingProfiler18reportTopFunctionsERN3WTF11PrintStreamE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC17DeferredWorkTimerD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(6, "_ZN3JSC17DeferredWorkTimerD2Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC19HeapSnapshotBuilderD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(7, "_ZN3JSC19HeapSnapshotBuilderD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(8, "_ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(9, "_ZN3JSC22createOutOfMemoryErrorEPNS_14JSGlobalObjectERKN3WTF6StringE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC2VM14throwExceptionEPNS_14JSGlobalObjectENS_7JSValueE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(10, "_ZN3JSC2VM14throwExceptionEPNS_14JSGlobalObjectENS_7JSValueE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC7Options10initializeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(11, "_ZN3JSC7Options10initializeEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC7Options10setOptionsEPKc(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(12, "_ZN3JSC7Options10setOptionsEPKc", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC8Bindings10RootObjectD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(13, "_ZN3JSC8Bindings10RootObjectD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC8DebuggerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(14, "_ZN3JSC8DebuggerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC8JSCalleeC1ERNS_2VMEPNS_14JSGlobalObjectEPNS_9StructureE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(15, "_ZN3JSC8JSCalleeC1ERNS_2VMEPNS_14JSGlobalObjectEPNS_9StructureE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF10fastMallocEm(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(16, "_ZN3WTF10fastMallocEm", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF20initializeMainThreadEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(17, "_ZN3WTF20initializeMainThreadEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF3URL10invalidateEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(18, "_ZN3WTF3URL10invalidateEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF7RunLoop3runEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(19, "_ZN3WTF7RunLoop3runEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF8dataFileEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(20, "_ZN3WTF8dataFileEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF8fastFreeEPv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(21, "_ZN3WTF8fastFreeEPv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11ContentType15codecsParameterEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(22, "_ZN7WebCore11ContentType15codecsParameterEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer19originsInMediaCacheERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(23, "_ZN7WebCore11MediaPlayer19originsInMediaCacheERKN3WTF6StringE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(24, "_ZN7WebCore11MediaPlayerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore12ISOWebVTTCueD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(25, "_ZN7WebCore12ISOWebVTTCueD2Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore12PrintContextD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(26, "_ZN7WebCore12PrintContextD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore13MIMETypeCacheD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(27, "_ZN7WebCore13MIMETypeCacheD2Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14CredentialBaseC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(28, "_ZN7WebCore14CredentialBaseC1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14DocumentLoaderC1ERKNS_15ResourceRequestERKNS_14SubstituteDataE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(29, "_ZN7WebCore14DocumentLoaderC1ERKNS_15ResourceRequestERKNS_14SubstituteDataE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14DocumentLoaderD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(30, "_ZN7WebCore14DocumentLoaderD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14FrameSelection16updateAppearanceEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(31, "_ZN7WebCore14FrameSelection16updateAppearanceEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(32, "_ZN7WebCore14JSVoidCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14JSVoidCallbackD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(33, "_ZN7WebCore14JSVoidCallbackD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14SQLiteDatabaseC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(34, "_ZN7WebCore14SQLiteDatabaseC1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14ScrollableArea17willEndLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(35, "_ZN7WebCore14ScrollableArea17willEndLiveResizeEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14ScrollableArea19willStartLiveResizeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(36, "_ZN7WebCore14ScrollableArea19willStartLiveResizeEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14XMLHttpRequestD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(37, "_ZN7WebCore14XMLHttpRequestD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore15DatabaseTracker29deleteAllDatabasesImmediatelyEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(38, "_ZN7WebCore15DatabaseTracker29deleteAllDatabasesImmediatelyEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore15DatabaseTracker9singletonEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(39, "_ZN7WebCore15DatabaseTracker9singletonEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(40, "_ZN7WebCore16JSStringCallbackC1EPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16JSStringCallbackD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(41, "_ZN7WebCore16JSStringCallbackD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16ScriptController37executeScriptInWorldIgnoringExceptionERNS_15DOMWrapperWorldERKN3WTF6StringEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(42, "_ZN7WebCore16ScriptController37executeScriptInWorldIgnoringExceptionERNS_15DOMWrapperWorldERKN3WTF6StringEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringEN3JSC7JSValueE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(43, "_ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringEN3JSC7JSValueE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore17NowPlayingManagerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(44, "_ZN7WebCore17NowPlayingManagerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore18DOMWindowExtensionD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(45, "_ZN7WebCore18DOMWindowExtensionD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(46, "_ZN7WebCore18TextureMapperLayerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore19MediaResourceLoaderD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(47, "_ZN7WebCore19MediaResourceLoaderD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore21mainThreadNormalWorldEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(48, "_ZN7WebCore21mainThreadNormalWorldEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore24CoordinatedGraphicsLayerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(49, "_ZN7WebCore24CoordinatedGraphicsLayerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore27AuthenticationChallengeBaseC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(50, "_ZN7WebCore27AuthenticationChallengeBaseC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore27AuthenticationChallengeBaseC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(51, "_ZN7WebCore27AuthenticationChallengeBaseC1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore4Page16setActivityStateEN3WTF9OptionSetINS_13ActivityState4FlagEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(52, "_ZN7WebCore4Page16setActivityStateEN3WTF9OptionSetINS_13ActivityState4FlagEEE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore5ColorC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(53, "_ZN7WebCore5ColorC1ERKS0_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore5FrameD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(54, "_ZN7WebCore5FrameD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore5Range6createERNS_8DocumentE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(55, "_ZN7WebCore5Range6createERNS_8DocumentE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore7Element16getAttributeNodeERKN3WTF10AtomStringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(56, "_ZN7WebCore7Element16getAttributeNodeERKN3WTF10AtomStringE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore7Element16setAttributeNodeERNS_4AttrE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(57, "_ZN7WebCore7Element16setAttributeNodeERNS_4AttrE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore7Element18getAttributeNodeNSERKN3WTF10AtomStringES4_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(58, "_ZN7WebCore7Element18getAttributeNodeNSERKN3WTF10AtomStringES4_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore7Element9innerTextEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(59, "_ZN7WebCore7Element9innerTextEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(60, "_ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9DOMWindowD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(61, "_ZN7WebCore9DOMWindowD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9FrameView26adjustTiledBackingCoverageEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(62, "_ZN7WebCore9FrameView26adjustTiledBackingCoverageEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(63, "_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector14InspectorAgentD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(64, "_ZN9Inspector14InspectorAgentD2Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector15RemoteInspector16pushListingsSoonEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(65, "_ZN9Inspector15RemoteInspector16pushListingsSoonEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector15RemoteInspector24updateClientCapabilitiesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(66, "_ZN9Inspector15RemoteInspector24updateClientCapabilitiesEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector18InjectedScriptBaseaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(67, "_ZN9Inspector18InjectedScriptBaseaSERKS0_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector20InspectorTargetAgentD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(68, "_ZN9Inspector20InspectorTargetAgentD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector21InjectedScriptManagerD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(69, "_ZN9Inspector21InjectedScriptManagerD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector21InspectorConsoleAgentD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(70, "_ZN9Inspector21InspectorConsoleAgentD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector28InspectorScriptProfilerAgentD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(71, "_ZN9Inspector28InspectorScriptProfilerAgentD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore11ContentType13containerTypeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(72, "_ZNK7WebCore11ContentType13containerTypeEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(73, "_ZNK7WebCore11ContentType9parameterERKN3WTF6StringE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore11FrameLoader14cancelledErrorERKNS_15ResourceRequestE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(74, "_ZNK7WebCore11FrameLoader14cancelledErrorERKNS_15ResourceRequestE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore15FocusController18focusedOrMainFrameEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(75, "_ZNK7WebCore15FocusController18focusedOrMainFrameEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore16VisibleSelection19rootEditableElementEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(76, "_ZNK7WebCore16VisibleSelection19rootEditableElementEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore4Node11textContentEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(77, "_ZNK7WebCore4Node11textContentEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore9DOMWindow8documentEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(78, "_ZNK7WebCore9DOMWindow8documentEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(79, "_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNKR3WTF6String12isolatedCopyEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(80, "_ZNKR3WTF6String12isolatedCopyEv", a0, a1, a2, a3, a4, a5);
}

namespace {

struct Seed {
    std::uint32_t seed;
    std::uint64_t digest;
};

struct Case {
    const char* name;
    Wrapper wrapper;
    int ret;
    std::vector<Seed> seeds;
};

const std::vector<Case> Cases = {
    {"JSRunLoop", &Thunk0, 4, {{1u, 0x3256a3f524a8f055ull}, {2u, 0x86bce92b9259dac2ull}, {3u, 0xe530fe637fcd28dcull}, {4u, 0xf1d601a7b3849adfull}, {5u, 0x27b6dc28d68634c0ull}, {6u, 0xfdb2d839c94bbd57ull}, {7u, 0x7ddd692b282921ccull}, {8u, 0x25c06c71cde51fe5ull}}},
    {"JSSetOptions", &Thunk1, 4, {{1u, 0xe9e1bf3f3f6851b7ull}, {2u, 0xaee484cde05d2c03ull}, {3u, 0xf04e9026a07327cfull}, {4u, 0x75895a36302db578ull}, {5u, 0xa340515a843c9d14ull}, {6u, 0x9a0343d5950d1e09ull}, {7u, 0x6851388e4c547e42ull}, {8u, 0x13175a5f9fb8bf0full}}},
    {"WKBundleClearAllDatabases", &Thunk2, 4, {{1u, 0xbe689398019ca49eull}, {2u, 0xc274681f72894c16ull}, {3u, 0x53e6edc70ac6464aull}, {4u, 0x891a1df590775414ull}, {5u, 0x282103b8f1490a9dull}, {6u, 0xe697f00e0d193cadull}, {7u, 0x754b5bbcbe40ddbdull}, {8u, 0xfbbca29e20f94c1aull}}},
    {"_ZN3JSC10JSFunctionC1ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE", &Thunk3, 4, {{1u, 0xf446737e2cd326c1ull}, {2u, 0xd8eca457d11f4751ull}, {3u, 0xcc268bb6c7b7271dull}, {4u, 0x319770b7d9d97b01ull}, {5u, 0x68b49e8ae88f20f7ull}, {6u, 0xcea107405fe3fcfaull}, {7u, 0x021c94d3c10c38b5ull}, {8u, 0x3f4e016945459ddfull}}},
    {"_ZN3JSC10JSFunctionC2ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE", &Thunk4, 4, {{1u, 0xf446737e2cd326c1ull}, {2u, 0xd8eca457d11f4751ull}, {3u, 0xcc268bb6c7b7271dull}, {4u, 0x319770b7d9d97b01ull}, {5u, 0x68b49e8ae88f20f7ull}, {6u, 0xcea107405fe3fcfaull}, {7u, 0x021c94d3c10c38b5ull}, {8u, 0x3f4e016945459ddfull}}},
    {"_ZN3JSC11VMInspector14dumpCellMemoryEPNS_6JSCellE", &Thunk5, 4, {{1u, 0x33cc9c65be8bff18ull}, {2u, 0x4c6f5485ec948ddfull}, {3u, 0x49ec565345db73d9ull}, {4u, 0x7770815e4fcf1f5full}, {5u, 0x92a16ad61a2715b5ull}, {6u, 0xece728eab7f5074bull}, {7u, 0xc7715a67e587186full}, {8u, 0xda47d62509493f58ull}}},
    {"_ZN3JSC12CachePayload16makeEmptyPayloadEv", &Thunk6, 4, {{1u, 0x0d95dc54efebdf1dull}, {2u, 0x5c00bcee7cf60decull}, {3u, 0xc54e1ab97cda994dull}, {4u, 0xeb883a58faf9fd48ull}, {5u, 0x7cbf551ba6d185dbull}, {6u, 0x77d4e4f2de2e369dull}, {7u, 0xa20c3d3f3a930d3aull}, {8u, 0xec8774e1294af8caull}}},
    {"_ZN3JSC12CachePayload17makeMallocPayloadEON3WTF9MallocPtrIhNS1_10FastMallocEEEm", &Thunk7, 4, {{1u, 0xab98c84542a5a30full}, {2u, 0x52b4568a5212c996ull}, {4u, 0x7a433e939fb5c8aaull}, {5u, 0x41185e6553a2d984ull}, {7u, 0x8fddb13df51bb57cull}, {9u, 0x7be0d6d997fb12d8ull}, {10u, 0x6c6d3fe9df9fb019ull}, {11u, 0xc05ea792ad0607cbull}}},
    {"_ZN3JSC16CompleteSubspaceD0Ev", &Thunk8, 4, {{1u, 0x0e0f1151edebeca6ull}, {2u, 0xd1578d65f176a356ull}, {3u, 0x68e86a6c6fa1c0b2ull}, {4u, 0x8351124638540e25ull}, {5u, 0x584dcfc44b6d6485ull}, {6u, 0x95fd15391db83430ull}, {7u, 0xda847e52f88b350full}, {8u, 0xc2493e1cf1f8fd3eull}}},
    {"_ZN3JSC16SamplingProfiler18reportTopBytecodesEv", &Thunk9, 4, {{1u, 0x8c5c50b701e30b74ull}, {2u, 0x21634c5efa802dcbull}, {3u, 0x4eae517fd2d462d5ull}, {4u, 0xc28841bb5ce5f013ull}, {5u, 0x227450b53fba2af9ull}, {6u, 0x0f8dead463431b07ull}, {7u, 0x9b86fda410e8c0d3ull}, {8u, 0xcf2b645270cde24cull}}},
    {"_ZN3JSC16SamplingProfiler18reportTopFunctionsEv", &Thunk10, 4, {{1u, 0x6cd1f66d67dc2f2dull}, {2u, 0xb3449cc4bc1e023aull}, {3u, 0x819863b4f06755ecull}, {4u, 0xb9cc4a9b23141536ull}, {5u, 0x797df87509dac810ull}, {6u, 0x7d3e03ff48f67bfaull}, {7u, 0xc099d156f72fe426ull}, {8u, 0xb29944346024508dull}}},
    {"_ZN3JSC17DeferredWorkTimerD0Ev", &Thunk11, 4, {{1u, 0x56f702e0d9eea7d7ull}, {2u, 0xccfaf8ee5404136bull}, {3u, 0xa9472932241f4dbbull}, {4u, 0xc1cd70bca0f9e0a4ull}, {5u, 0x2be6604ab39c7880ull}, {6u, 0x59ef3c759d755171ull}, {7u, 0x2e9ee55e7b05458eull}, {8u, 0xc09c074b7eb057f7ull}}},
    {"_ZN3JSC19HeapSnapshotBuilderD0Ev", &Thunk12, 4, {{1u, 0x51f937dd11ab1e1aull}, {2u, 0x281820ef2f5ba722ull}, {3u, 0x4e0adae18a2a7676ull}, {4u, 0x7d2dcd17a86a7ce9ull}, {5u, 0x418d88068bd00a19ull}, {6u, 0x59df031e0a8491bcull}, {7u, 0x4367336e52fc2bb3ull}, {8u, 0xa954d522310c776aull}}},
    {"_ZN3JSC19InlineWatchpointSet11inflateSlowEv", &Thunk13, 4, {{2u, 0x57c687ba5fb7efdbull}, {3u, 0x5a79774e31e600b3ull}, {5u, 0xd7688f988a082946ull}, {11u, 0xed5efbc4bd12e744ull}, {13u, 0xc0ce1381a0835454ull}, {14u, 0xbcbf5628d4de6183ull}, {15u, 0x40a83e55fb241d10ull}, {18u, 0x45e7fa7de836f515ull}}},
    {"_ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeE", &Thunk14, 4, {{1u, 0x7a734dc9482aac7bull}, {2u, 0x746ae18fccdff964ull}, {4u, 0x9f62dd196f94cb92ull}, {5u, 0x6d86cecd8e81663full}, {7u, 0x12f40fe0951d366full}, {9u, 0x77741a6e489976f4ull}, {10u, 0x9ad9e2c6beb46538ull}, {11u, 0x359f83f5959c87a0ull}}},
    {"_ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeERKN3WTF6StringE", &Thunk15, 4, {{1u, 0xba60c0a49cbaa667ull}, {2u, 0xce8008ddf38e3367ull}, {4u, 0x178542225d2c2dc0ull}, {5u, 0x43676c51e94c71d0ull}, {7u, 0x124d5b617a6d6704ull}, {9u, 0xb1613d7014bfa555ull}, {10u, 0x5e5b90c0cba2b7c8ull}, {11u, 0x9c8961b6665866ecull}}},
    {"_ZN3JSC22EdenGCActivityCallbackD0Ev", &Thunk16, 4, {{1u, 0x185c2f0701552663ull}, {2u, 0x4ebf356b6ebefe0full}, {3u, 0xccd87ad04c659b57ull}, {4u, 0x7db3adc948959b60ull}, {5u, 0x762b57b54c6281fcull}, {6u, 0x27ce8565282e1a05ull}, {7u, 0x3db0e8fc36f40ecaull}, {8u, 0xbdb983247fea647bull}}},
    {"_ZN3JSC32JSDestructibleObjectHeapCellTypeD0Ev", &Thunk17, 4, {{1u, 0x7746c25f150c523cull}, {2u, 0x085d1124f1e87e48ull}, {3u, 0x7b78592c80fe5514ull}, {4u, 0xe58d73f98ef4a7b7ull}, {5u, 0x25fcce1b18f1b183ull}, {6u, 0xf5e6d7fe4592f966ull}, {7u, 0xe66979a4c511ef75ull}, {8u, 0x1ebe382bdb643888ull}}},
    {"_ZN3JSC8Bindings10RootObjectD0Ev", &Thunk18, 4, {{1u, 0xcd81f237bcf559b8ull}, {2u, 0xdd4d77ac14fa581cull}, {3u, 0x5c019573499a5280ull}, {4u, 0xb3de941f4cb0ab03ull}, {5u, 0xb57f27f538acec2full}, {6u, 0x5d81a590d3f5130aull}, {7u, 0x15d23c5c9dc1cb81ull}, {8u, 0x8e80cc44b3c9145cull}}},
    {"_ZN3JSC8DebuggerD0Ev", &Thunk19, 4, {{1u, 0x56b886a345226c7full}, {2u, 0x2c7c9c23c0e70c73ull}, {3u, 0xe61f5a46bc0f3a73ull}, {4u, 0x9bea8a5458afb75cull}, {5u, 0xcbdf64f15d64f2f8ull}, {6u, 0x4a152952a1c05fd9ull}, {7u, 0x88ac1cdcf5270be6ull}, {8u, 0xecf57aee806dd8cfull}}},
    {"_ZN3WTF18ExternalStringImpl6createEPKDsjONS_8FunctionIFvPS0_PvjEEE", &Thunk20, 4, {{2u, 0x80b1bf9372db87b7ull}, {3u, 0xf6928bdbc554f2eaull}, {5u, 0xa4970098ad9d450eull}, {11u, 0xf8b5f3a1b9185b61ull}, {13u, 0x913b03e3115cc28aull}, {14u, 0x2b5de3e982cfc8b5ull}, {15u, 0x2bb8b5151d25551full}, {18u, 0xfc17664d89de6dbaull}}},
    {"_ZN3WTF18ExternalStringImpl6createEPKhjONS_8FunctionIFvPS0_PvjEEE", &Thunk21, 4, {{2u, 0x093821643e170743ull}, {3u, 0xc3386dd32138362eull}, {5u, 0x8fd7d3295065b8c2ull}, {11u, 0x7c517646768ea8a5ull}, {13u, 0x881d43a1055f3a36ull}, {14u, 0x2a8f57040d8d16f9ull}, {15u, 0xb191518c3a917b63ull}, {18u, 0xa9f49b533ff501ceull}}},
    {"_ZN7WebCore11MediaPlayerD0Ev", &Thunk22, 4, {{1u, 0xa82f203eb7caf3c1ull}, {2u, 0x9c854a261dd6aa11ull}, {3u, 0xda30eabccbf8b239ull}, {4u, 0xfa77c69a31c6a6eaull}, {5u, 0x7536bccf72d89592ull}, {6u, 0x5cf116fe35059f53ull}, {7u, 0xcae41a93284c3ff8ull}, {8u, 0x982beb0d7e875cbdull}}},
    {"_ZN7WebCore12ISOWebVTTCueD0Ev", &Thunk23, 4, {{1u, 0x1ab29697c4dc3854ull}, {2u, 0xe227c6b9d4e39700ull}, {3u, 0xdb692aca8c1e66fcull}, {4u, 0x2fa6335ca914a79full}, {5u, 0x3f081940b3d9802bull}, {6u, 0xbf2bf54bef1370beull}, {7u, 0x77bdb0908a76d5bdull}, {8u, 0x0fa23db1e25b0a50ull}}},
    {"_ZN7WebCore12NamedNodeMap12setNamedItemERNS_4AttrE", &Thunk24, 4, {{1u, 0xb68fd6e7ff800c8dull}, {2u, 0xf52ce3a241b1b400ull}, {4u, 0xac575f8f651e5269ull}, {5u, 0x07731428f0ec81d7ull}, {7u, 0xa5ec077fe2f6eb25ull}, {9u, 0x7f22e243b6668d4cull}, {10u, 0xebb6faa4e0b55aafull}, {11u, 0xb3d0a4c1915611d8ull}}},
    {"_ZN7WebCore12PrintContextD0Ev", &Thunk25, 4, {{1u, 0x6d77d132f56cc53bull}, {2u, 0xc0aa38f1b4191c27ull}, {3u, 0x7f850fcbe8c3843full}, {4u, 0x1b3fa0a3b65a15e8ull}, {5u, 0x1b714b4ec42413e4ull}, {6u, 0xc3979ce0751d0cfdull}, {7u, 0xc9908e50d41a3852ull}, {8u, 0x135534e5ca8df3a3ull}}},
    {"_ZN7WebCore13MIMETypeCacheD0Ev", &Thunk26, 4, {{1u, 0xd4d65099c3a93abeull}, {2u, 0xab262a1f448c8b2eull}, {3u, 0x560b0d214eb6253aull}, {4u, 0xfd9131d201b4ca2dull}, {5u, 0xfc68ac8a3c13d7edull}, {6u, 0xf8142be385d96b88ull}, {7u, 0x1b7c660a56dba7f7ull}, {8u, 0xcfe80d8b7eeb4fa6ull}}},
    {"_ZN7WebCore14DocumentLoaderD0Ev", &Thunk27, 4, {{1u, 0x8fa803437440476full}, {2u, 0x5c6a53d293f59423ull}, {3u, 0x65f8d6c1949fe823ull}, {4u, 0x14d891e2520cb2ecull}, {5u, 0xf9aef61111f5e8e8ull}, {6u, 0x7de93fe0ffdbf049ull}, {7u, 0x98395ba82339e7b6ull}, {8u, 0x0040ac03e4a954ffull}}},
    {"_ZN7WebCore14JSVoidCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE", &Thunk28, 4, {{1u, 0x32c65350c113c12cull}, {2u, 0xd24d07971866482aull}, {3u, 0xf68ab652c4f37383ull}, {4u, 0xda3f0124649cdfebull}, {5u, 0xd324830e2f6c5cacull}, {6u, 0xc73c402597d15bf8ull}, {7u, 0xb7a94219b53d0254ull}, {8u, 0x98c88351ff625003ull}}},
    {"_ZN7WebCore14JSVoidCallbackD0Ev", &Thunk29, 4, {{1u, 0x9040697cabcd9f9cull}, {2u, 0x8c5da49325e155e8ull}, {3u, 0x02d31b4506a6a1b4ull}, {4u, 0x0117a963569eee57ull}, {5u, 0x1a712e60dbbbe323ull}, {6u, 0x5db6864435ca3a46ull}, {7u, 0x44f36f7c9cfed2d5ull}, {8u, 0x3be60bb6221a95e8ull}}},
    {"_ZN7WebCore14ResourceLoader14cancelledErrorEv", &Thunk30, 4, {{1u, 0x1461bed3928e8b77ull}, {2u, 0x41b7d883c415605eull}, {5u, 0xf1268b85dced0c0bull}, {9u, 0xe4a0eebf6ad500e9ull}, {10u, 0x6c96871e4978ff4eull}, {11u, 0xe4a83844a5ebc285ull}, {15u, 0x294993a24283cb9cull}, {16u, 0xf5a79bc706de1e40ull}}},
    {"_ZN7WebCore14XMLHttpRequestD0Ev", &Thunk31, 4, {{1u, 0x262605bac921efb0ull}, {2u, 0xd1e7d74d43f00c34ull}, {3u, 0x9cafcb5cdfd46948ull}, {4u, 0x107205df22ff3a8bull}, {5u, 0xc39c4c4f84dd7f37ull}, {6u, 0xaa1d1c8b9877c3a2ull}, {7u, 0x140cacde3f0976e9ull}, {8u, 0x9dc28eb38cdbdba4ull}}},
    {"_ZN7WebCore15DatabaseTracker23trackerWithDatabasePathERKN3WTF6StringE", &Thunk32, 4, {{2u, 0xbc7adbafc4950ee9ull}, {3u, 0x8fd7ef8b187ee53bull}, {5u, 0x08f7d87353306359ull}, {11u, 0x3e3a2de2dee25d87ull}, {13u, 0x1ea971b92209921bull}, {14u, 0xb938fe02efc8163bull}, {15u, 0x6707bd03e8de579cull}, {18u, 0xb4b0f86e5ab7ffa5ull}}},
    {"_ZN7WebCore15PasteboardImageC1Ev", &Thunk33, 4, {{1u, 0xb1b90f9e23c163e9ull}, {2u, 0x62ec81a2f14220caull}, {3u, 0x27a11fd2b52f7571ull}, {4u, 0x091c6f6b2833f0e1ull}, {5u, 0x53eebe80c7ff94cfull}, {6u, 0x0c72237a65c1d54aull}, {7u, 0x158911dfdddb1c7dull}, {8u, 0x4e2ab1cdf9a03dc3ull}}},
    {"_ZN7WebCore15PasteboardImageC2Ev", &Thunk34, 4, {{1u, 0xb1b90f9e23c163e9ull}, {2u, 0x62ec81a2f14220caull}, {3u, 0x27a11fd2b52f7571ull}, {4u, 0x091c6f6b2833f0e1ull}, {5u, 0x53eebe80c7ff94cfull}, {6u, 0x0c72237a65c1d54aull}, {7u, 0x158911dfdddb1c7dull}, {8u, 0x4e2ab1cdf9a03dc3ull}}},
    {"_ZN7WebCore16CalculationValue6createESt10unique_ptrINS_18CalcExpressionNodeESt14default_deleteIS2_EENS_10ValueRangeE", &Thunk35, 4, {{2u, 0x99b1bcd28277f422ull}, {5u, 0xa8a8814df21a9a2dull}, {11u, 0x16490892ad5d2125ull}, {13u, 0x4d3f77748f2333e6ull}, {14u, 0x4bd4453a78deb3a2ull}, {15u, 0x18d03d053f39f7c5ull}, {18u, 0xce03b6fc75dd5a14ull}, {19u, 0x9d9a6ef271963dbcull}}},
    {"_ZN7WebCore16DeviceMotionData6createEv", &Thunk36, 4, {{2u, 0x53703bd5e8d6aee3ull}, {3u, 0x9197422b95a3b54bull}, {5u, 0xdc41129958908a49ull}, {11u, 0x8bbf86dda2bec1b2ull}, {13u, 0x1cb6fe0bf2c28c2dull}, {14u, 0xcc806ec45a353fb3ull}, {15u, 0x051056e5e0a68748ull}, {18u, 0x0739d8c998c1356aull}}},
    {"_ZN7WebCore16HTMLMediaElement19originsInMediaCacheERKN3WTF6StringE", &Thunk37, 4, {{1u, 0x4e71fe9a66317427ull}, {2u, 0x64163321bfe140b3ull}, {3u, 0x06c572486904817cull}, {4u, 0xe3dc8dd9111d6270ull}, {5u, 0x209b50aa6355265aull}, {6u, 0x076cc853a4217a83ull}, {7u, 0xa365720dc48d250eull}, {8u, 0x6d21743f7cc94212ull}}},
    {"_ZN7WebCore16JSStringCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE", &Thunk38, 4, {{1u, 0x529041215b781284ull}, {2u, 0x0d8ada8b055787d2ull}, {3u, 0xc754e60fca3c8f8bull}, {4u, 0x87dd329225fdf203ull}, {5u, 0x9ccd9df07d03a3b4ull}, {6u, 0xd4277119a1687680ull}, {7u, 0x3d6a031c3db0599cull}, {8u, 0xa3c81868a52a7efbull}}},
    {"_ZN7WebCore16JSStringCallbackD0Ev", &Thunk39, 4, {{1u, 0x0dac57a901fb8244ull}, {2u, 0xedb1298a2f44aff0ull}, {3u, 0x8083d74e5c2d1b2cull}, {4u, 0xc5827c75335866efull}, {5u, 0x721e287f6de4769bull}, {6u, 0x2184b798529ff7aeull}, {7u, 0x398166214ddb0acdull}, {8u, 0x9706bdcd2f4a1c80ull}}},
    {"_ZN7WebCore16ScriptController30executeScriptIgnoringExceptionERKN3WTF6StringEb", &Thunk40, 4, {{1u, 0xa5a98d14d3a22f76ull}, {2u, 0xc8ed70a494d78e53ull}, {3u, 0x221faed633927111ull}, {4u, 0x9e6a524c657d3cfaull}, {5u, 0x12c615ab28e7d6dcull}, {6u, 0x2d479d0037c40eacull}, {7u, 0xc777c9bbeb65f34eull}, {8u, 0x14ceb6155fbd6e2eull}}},
    {"_ZN7WebCore17CredentialStorage24getFromPersistentStorageERKNS_15ProtectionSpaceE", &Thunk41, 4, {{1u, 0x7888311b377f3cb0ull}, {2u, 0x2a1461b6999b8a14ull}, {3u, 0xf16b1dad93884263ull}, {4u, 0x7902300aef09c69full}, {5u, 0x588201f14850db49ull}, {6u, 0x6a196c4e0d00b6f4ull}, {7u, 0xe0ea2893ea1bae7dull}, {8u, 0xbb71662ea9009a2dull}}},
    {"_ZN7WebCore17HTMLAnchorElement4textEv", &Thunk42, 4, {{1u, 0xd38ccf01eb30f1eeull}, {2u, 0x5f8d416577baff4eull}, {3u, 0xd1a911b34f8a3291ull}, {4u, 0x2f5ef64946c907b5ull}, {5u, 0xc537d0a2ecb55b56ull}, {6u, 0xefec1a29cac2d133ull}, {7u, 0x94fb3fb57aa935fbull}, {8u, 0x6739dbc3433479ccull}}},
    {"_ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringE", &Thunk43, 4, {{1u, 0xd8d85456c751581aull}, {2u, 0xd7328e70da61fbb2ull}, {3u, 0x659d1d665eba84baull}, {4u, 0x165fa6b502ca36a4ull}, {5u, 0x9c0d086510aca8d3ull}, {6u, 0x658f444bb03fd4ccull}, {7u, 0xebd6e0fd5c003806ull}, {8u, 0x0522866a30300ff5ull}}},
    {"_ZN7WebCore17NowPlayingManagerD0Ev", &Thunk44, 4, {{1u, 0xdabdcdfd01024395ull}, {2u, 0xe5da9b5403a5ae7dull}, {3u, 0xdad30a8fd3efecedull}, {4u, 0x842d5f5db32cae6eull}, {5u, 0x49507fd2349c49f6ull}, {6u, 0x63684f9be41ddedfull}, {7u, 0x54e75b3ef48cda6cull}, {8u, 0x04c127dc67842f19ull}}},
    {"_ZN7WebCore18DOMWindowExtensionD0Ev", &Thunk45, 4, {{1u, 0xdd9a301fb256e918ull}, {2u, 0x4a455041bb62eb3cull}, {3u, 0x510e9103bd2cd320ull}, {4u, 0x1b3c5f8862a0aa23ull}, {5u, 0x2577a1230d6fe8cfull}, {6u, 0xeac95a7feba6d8eaull}, {7u, 0xbd696dc819a39a61ull}, {8u, 0xe4a606a969ea92bcull}}},
    {"_ZN7WebCore18TextureMapperLayerD0Ev", &Thunk46, 4, {{1u, 0xeeef6c443c2857dfull}, {2u, 0x757b5989153f1193ull}, {3u, 0xad1e1808df567f13ull}, {4u, 0xbcd8934639e5cffcull}, {5u, 0x959e120311b8b698ull}, {6u, 0x77c927cdd66fe539ull}, {7u, 0x14e1dba7841716c6ull}, {8u, 0xc9cb5fca649497afull}}},
    {"_ZN7WebCore19MediaResourceLoaderD0Ev", &Thunk47, 4, {{1u, 0xeadbe1e63396fb42ull}, {2u, 0xf2394a02ce98178aull}, {3u, 0x1d92eaee7456650eull}, {4u, 0x69749f4afc53d521ull}, {5u, 0xde0c05e17f640371ull}, {6u, 0x577948bff4704344ull}, {7u, 0x9618f16a2301148bull}, {8u, 0x96a93cf5c9da49a2ull}}},
    {"_ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE", &Thunk48, 4, {{1u, 0x7ee448fc998a9df8ull}, {2u, 0x59e9e04e818df359ull}, {3u, 0x2ee117d3495fc116ull}, {4u, 0x95671b3d8cf3b764ull}, {5u, 0x6e4bb4affb839f0eull}, {6u, 0x245eba7cac26b457ull}, {7u, 0x98481f3c20767ff6ull}, {8u, 0xdc484116423a240eull}}},
    {"_ZN7WebCore22MutableStyleProperties6createENS_13CSSParserModeE", &Thunk49, 4, {{2u, 0x487363355c7defc4ull}, {3u, 0x84ad79f1d67014f4ull}, {5u, 0xab93f89b86da82f8ull}, {11u, 0xd59b2072c802de20ull}, {13u, 0xdad8f7902715a15aull}, {14u, 0xe1ab9c543c6f8e6dull}, {15u, 0x5f82fba4e4f17d12ull}, {18u, 0x6700fff6aa10f1acull}}},
    {"_ZN7WebCore23AuthenticationChallengeC1ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE", &Thunk50, 4, {{1u, 0x81bf87e229c6f16eull}, {2u, 0x1389a3767a4ff276ull}, {3u, 0xed975b721e57f032ull}, {4u, 0xaa21b17ff9951eabull}, {5u, 0x0be68817d42de3b0ull}, {6u, 0xde7f1910e93ee7a8ull}, {7u, 0x9603155909c2b882ull}, {8u, 0x818cf3b56299afb7ull}}},
    {"_ZN7WebCore23AuthenticationChallengeC1Ev", &Thunk51, 4, {{1u, 0x8e998e2b901b296bull}, {2u, 0xe2d3eb26fa2dd7e3ull}, {3u, 0x28b505b1f0f3f2fbull}, {4u, 0x50000bd1fdfca132ull}, {5u, 0xecf9ae43a1cf9a39ull}, {6u, 0x53ec6bbfcd03c4cdull}, {7u, 0x8240e3c76528d983ull}, {8u, 0xb3e45153df57378eull}}},
    {"_ZN7WebCore23AuthenticationChallengeC2ERKNS_15ProtectionSpaceERKNS_10CredentialEjRKNS_16ResourceResponseERKNS_13ResourceErrorE", &Thunk52, 4, {{1u, 0x81bf87e229c6f16eull}, {2u, 0x1389a3767a4ff276ull}, {3u, 0xed975b721e57f032ull}, {4u, 0xaa21b17ff9951eabull}, {5u, 0x0be68817d42de3b0ull}, {6u, 0xde7f1910e93ee7a8ull}, {7u, 0x9603155909c2b882ull}, {8u, 0x818cf3b56299afb7ull}}},
    {"_ZN7WebCore23AuthenticationChallengeC2Ev", &Thunk53, 4, {{1u, 0x8e998e2b901b296bull}, {2u, 0xe2d3eb26fa2dd7e3ull}, {3u, 0x28b505b1f0f3f2fbull}, {4u, 0x50000bd1fdfca132ull}, {5u, 0xecf9ae43a1cf9a39ull}, {6u, 0x53ec6bbfcd03c4cdull}, {7u, 0x8240e3c76528d983ull}, {8u, 0xb3e45153df57378eull}}},
    {"_ZN7WebCore24CoordinatedGraphicsLayerD0Ev", &Thunk54, 4, {{1u, 0x60e3572aac9ba84cull}, {2u, 0x78074034b87cb558ull}, {3u, 0xc3cdc8057f275964ull}, {4u, 0xce5f842fb77c1327ull}, {5u, 0xdb172fe827f98c53ull}, {6u, 0x2f8a1068b8ccdbb6ull}, {7u, 0x54cda9b127b357a5ull}, {8u, 0x54364b9aa7395958ull}}},
    {"_ZN7WebCore4Page12setIsVisibleEb", &Thunk55, 4, {{1u, 0x0a5318058f98da56ull}, {2u, 0x2db51ae92177cb27ull}, {3u, 0xe65de2876a6f6f7eull}, {4u, 0xfe2638455d4157b5ull}, {5u, 0x858b51acf57055ebull}, {6u, 0x2287e87451b539b2ull}, {7u, 0xc544d2551fb8cc45ull}, {8u, 0x0a648d686d97e666ull}}},
    {"_ZN7WebCore5FrameD0Ev", &Thunk56, 4, {{1u, 0xe8265fe5f0008167ull}, {2u, 0x275f006e8e20df3bull}, {3u, 0xdf08079624259c4bull}, {4u, 0x6c3d25619d76b894ull}, {5u, 0xc6c83d4a25a59610ull}, {6u, 0xb2ae90407c973c41ull}, {7u, 0x0b6643b8f96868beull}, {8u, 0x9d971c8b51ed1fc7ull}}},
    {"_ZN7WebCore6Editor26toggleOverwriteModeEnabledEv", &Thunk57, 4, {{58u, 0x9041f6c02b830aaaull}, {69u, 0x3481a231c8e18ad0ull}, {92u, 0xd72a77b27b717262ull}, {99u, 0x38c99bc2390602bdull}, {100u, 0x76a577a1bb633e4full}, {146u, 0x973b7ddfd21f311dull}, {154u, 0x21bf99170128e743ull}, {164u, 0xbf99d820fc31a07eull}}},
    {"_ZN7WebCore7Element9outerTextEv", &Thunk58, 4, {{1u, 0x33d7f89705211dc3ull}, {2u, 0x79da6327a0d877dfull}, {3u, 0x62ac6dc2fbd90a60ull}, {4u, 0xf71784db30f954bcull}, {5u, 0x96f6e7b43d1d4116ull}, {6u, 0x4aec6878a4ebfc87ull}, {7u, 0xabe189bca932ef12ull}, {8u, 0x05a99b7e61ae7286ull}}},
    {"_ZN7WebCore8Document11createRangeEv", &Thunk59, 4, {{1u, 0x03590d09d8044b07ull}, {2u, 0xd483f12ee33a7093ull}, {3u, 0xe77e55cc6b7f641cull}, {4u, 0xad405fc0fc3c6dd0ull}, {5u, 0x16042af62a1e167aull}, {6u, 0x4a99a77ea08be3e3ull}, {7u, 0xfba48c24a0bdc02eull}, {8u, 0xe63364ee2c2e0d72ull}}},
    {"_ZN7WebCore9DOMWindowD0Ev", &Thunk60, 4, {{1u, 0x450885610de7de48ull}, {2u, 0xa012b8869b6f536cull}, {3u, 0xcf4696f7ffb05710ull}, {4u, 0x01466edd06e46db3ull}, {5u, 0x2cde5a44995fcdbfull}, {6u, 0x0992824876e00c1aull}, {7u, 0xfe6f5b54c3f20031ull}, {8u, 0xb4b32157b2635decull}}},
    {"_ZN7WebCore9FrameView17willEndLiveResizeEv", &Thunk61, 4, {{1u, 0xd6290d31a2cc738dull}, {2u, 0x7289b5e43884e925ull}, {3u, 0xd02d0b63748b39b9ull}, {4u, 0xe9ca106c8694fdb2ull}, {5u, 0xf261ba87cdecbd4dull}, {6u, 0xc1f6f4c269088317ull}, {7u, 0xd78dfbc06c07a2cfull}, {8u, 0x52953fd0309a2dfdull}}},
    {"_ZN7WebCore9FrameView19willStartLiveResizeEv", &Thunk62, 4, {{1u, 0x1bd6775db3dc97e6ull}, {2u, 0xab17f0d809d4fc86ull}, {3u, 0x764f452bbcda1d0eull}, {4u, 0x747f16ecaf4acec9ull}, {5u, 0xa90d9237ea6a5ba6ull}, {6u, 0x8b2c7a767e68d1d0ull}, {7u, 0x3b502290c6ae3d54ull}, {8u, 0xd96a933a41b25db2ull}}},
    {"_ZN7WebCorelsERN3WTF10TextStreamERKNS_11EventRegionE", &Thunk63, 4, {{1u, 0x7d3c4dd7aab43b4full}, {2u, 0x34761879755a613bull}, {3u, 0x7dbed01ac603bca4ull}, {4u, 0x265abc48e6f5f7c8ull}, {5u, 0xa53ba78ffb658572ull}, {6u, 0x72111bf7b749b11bull}, {7u, 0x2cdb04de4332fe56ull}, {8u, 0x3b2c62e8562b4edaull}}},
    {"_ZN9Inspector14InjectedScriptaSERKS0_", &Thunk64, 4, {{1u, 0x8eef75ba85d5220bull}, {2u, 0xcae9336265618cc7ull}, {3u, 0x706aae0b121be6a8ull}, {4u, 0x445830d96b433c74ull}, {5u, 0xadef809d9834922eull}, {6u, 0x176ae4517a616adfull}, {7u, 0x34e7751d99be8bfaull}, {8u, 0x1b09f7498c8e64ceull}}},
    {"_ZN9Inspector14InspectorAgentD0Ev", &Thunk65, 4, {{1u, 0x926527c3e179f589ull}, {2u, 0x4b21090ffb3c8339ull}, {3u, 0xea64022e72ee91d1ull}, {4u, 0x3cfe5c5fad67fac2ull}, {5u, 0xc61acead1aa2652aull}, {6u, 0x3de9d8222f8c065bull}, {7u, 0xbebe57c6c88b39d0ull}, {8u, 0xe1f464b82f4f1335ull}}},
    {"_ZN9Inspector15RemoteInspector27clientCapabilitiesDidChangeEv", &Thunk66, 4, {{1u, 0xad7b2223dce8661full}, {2u, 0x926c1b5e879dde23ull}, {3u, 0xe8f2ea10bcabc153ull}, {4u, 0xc5f2570894dc2094ull}, {5u, 0xe8bdc76743469ee8ull}, {6u, 0xac9f995cbca73171ull}, {7u, 0xe917fa23f84ff38eull}, {8u, 0xb30ed6d5bcf521dfull}}},
    {"_ZN9Inspector20InspectorTargetAgentD0Ev", &Thunk67, 4, {{1u, 0xc53623dc1de1e18dull}, {2u, 0x570874767924ea15ull}, {3u, 0x6ec5465b2fb190f5ull}, {4u, 0xe564f9cea8b65956ull}, {5u, 0x2456a58fd0b1e4feull}, {6u, 0x3b2ca887563e87b7ull}, {7u, 0x77b156d6857fdf94ull}, {8u, 0x69868b3b33d27481ull}}},
    {"_ZN9Inspector21InjectedScriptManagerD0Ev", &Thunk68, 4, {{1u, 0x3615cb59938a8110ull}, {2u, 0x88b7c8be78e78754ull}, {3u, 0x29d006bc52b888e8ull}, {4u, 0x2cd43a990f1b53abull}, {5u, 0xc639b326e8eb3a57ull}, {6u, 0xa5ff59803652c382ull}, {7u, 0xed8d4b3f5d24c449ull}, {8u, 0x3dedd6a030cefd04ull}}},
    {"_ZN9Inspector21InspectorConsoleAgentD0Ev", &Thunk69, 4, {{1u, 0x11b32df04c621017ull}, {2u, 0x8d382d4277dc122bull}, {3u, 0x0523a358956d9c7bull}, {4u, 0x8a69704a077a1ae4ull}, {5u, 0xf8f2db8be8d9e8c0ull}, {6u, 0x6f50cd41f2243331ull}, {7u, 0x96aef6f57fc0a3ceull}, {8u, 0xeb07328c0944b837ull}}},
    {"_ZN9Inspector28InspectorScriptProfilerAgentD0Ev", &Thunk70, 4, {{1u, 0x6df77aeed7275c5aull}, {2u, 0x2eea256188cea5e2ull}, {3u, 0x9fe6615f0cb71536ull}, {4u, 0xadcd70adb9ef6529ull}, {5u, 0x91ebe6bdfaddd959ull}, {6u, 0x73218ec67a5f2b7cull}, {7u, 0xe38ad768d215a4f3ull}, {8u, 0xd3a4c85da801f6aaull}}},
    {"_ZNK14OpaqueJSString6stringEv", &Thunk71, 4, {{1u, 0xcb9c6246cfbe89b4ull}, {2u, 0x913d42d16eb33c48ull}, {3u, 0x7a4cfcde475cc58full}, {4u, 0x0463d63bd2d6bb83ull}, {5u, 0xa555ebadc024dfbdull}, {6u, 0x0a595fe2a85fff80ull}, {7u, 0x2bb29e25774ff179ull}, {8u, 0x01f1b87a7e67cac9ull}}},
    {"_ZNK7WebCore11MediaPlayer15contentMIMETypeEv", &Thunk72, 4, {{1u, 0xea4f1eead09a874full}, {2u, 0x1376893b45acd590ull}, {3u, 0x60521dd51e994d77ull}, {4u, 0xcce93fd7a479a5d0ull}, {5u, 0x6fc09b62551a1682ull}, {6u, 0x3776748089a9a938ull}, {7u, 0x30540ccca597dafaull}, {8u, 0x2177867af8077040ull}}},
    {"_ZNK7WebCore11MediaPlayer17contentTypeCodecsEv", &Thunk73, 4, {{1u, 0xf5707aed0d386035ull}, {2u, 0x5d358c3925709de5ull}, {3u, 0xf228be926a7af057ull}, {4u, 0xb486e01b20da4cf5ull}, {5u, 0x265aee3256658a52ull}, {6u, 0xddf3cd7a9de44d1aull}, {7u, 0x52276cb7826a6d51ull}, {8u, 0xdaff9d33eb90ebf9ull}}},
    {"_ZNK7WebCore12NamedNodeMap12getNamedItemERKN3WTF10AtomStringE", &Thunk74, 4, {{1u, 0xed2a9be4a7338020ull}, {2u, 0x812701ed41ee451dull}, {4u, 0x1299ff9ec7f5c14cull}, {5u, 0x4a2b3d4ef58cb122ull}, {7u, 0xbb64809e4f48240cull}, {9u, 0xf8f5a53d1e43df3dull}, {10u, 0x2455f08beaf65cf2ull}, {11u, 0xf5243251ec518ad9ull}}},
    {"_ZNK7WebCore12NamedNodeMap14getNamedItemNSERKN3WTF10AtomStringES4_", &Thunk75, 4, {{1u, 0x3576e96d6b5ad242ull}, {2u, 0x14bc3deaf4725683ull}, {4u, 0xec60a88b0e6cd65eull}, {5u, 0x364d171149cda9c4ull}, {7u, 0x6a9dcdf9f8e62cf2ull}, {9u, 0x51a8d7d183b299abull}, {10u, 0xa44bdf7223ba40f0ull}, {11u, 0x2dba74846069dd83ull}}},
    {"_ZNK7WebCore15JSDOMWindowBase22scriptExecutionContextEv", &Thunk76, 4, {{1u, 0xde32c1d98e128c42ull}, {2u, 0x3f63be09f7056073ull}, {3u, 0x4f6ef47961c73b3dull}, {4u, 0xc55d4a57e3506bd8ull}, {5u, 0x67c7ddd21938b85eull}, {6u, 0xcfe172b201a861c7ull}, {7u, 0xb5ab8f84890ab5d0ull}, {8u, 0xd5168f81a6c97543ull}}},
    {"_ZNK7WebCore4Page9selectionEv", &Thunk77, 4, {{19u, 0x694ca79cdb502ec7ull}, {21u, 0xe626b7b17c692c57ull}, {23u, 0x86f4c1a3214a898eull}, {33u, 0x9f8ec968e36c95d5ull}, {38u, 0xdc41ade7f9b45f11ull}, {50u, 0x7f1e8e4e9148bc3aull}, {81u, 0x1431e705150859e0ull}, {84u, 0x74035cf79d4f1368ull}}},
    {"_ZNK7WebCore6Editor7canEditEv", &Thunk78, 1, {{1u, 0x735ae2537002eeecull}, {3u, 0x3bf37beed0f6e32aull}, {4u, 0x1df750aaf8074386ull}, {5u, 0x265fc60d0b1670a2ull}, {6u, 0x6ba3f3f5d7265ab1ull}, {8u, 0xe832f18710eeea38ull}, {9u, 0xef13861780b86bd6ull}, {16u, 0x094952d8914bc5a8ull}}},
    {"_ZNK7WebCore9FrameView19baseBackgroundColorEv", &Thunk79, 4, {{1u, 0x540e0f0e864d8908ull}, {2u, 0xe7568807927ed8b4ull}, {3u, 0xaa8d0f0a16bb9110ull}, {4u, 0x054a3770c0095dffull}, {5u, 0x52438278345eb139ull}, {6u, 0x2b7bf3e4cea92cb8ull}, {7u, 0xd6eb5e96e914a5e1ull}, {8u, 0x1afd4d6cca1a31e3ull}}},
    {"_ZNK9Inspector20InspectorTargetAgent14connectionTypeEv", &Thunk80, 3, {{1u, 0x09986184c45c93c3ull}, {2u, 0x7ec6e1a1cceb1d73ull}, {3u, 0xf19b709677199c13ull}, {4u, 0xf2611534b6738de4ull}, {5u, 0x47cb5e9da4d9b0afull}, {6u, 0x045c9f12e6cae7c0ull}, {7u, 0xd6a9e85294c20075ull}, {8u, 0x8638c16de77c9149ull}}},
    {"_ZThn112_N7WebCore9DOMWindowD0Ev", &Thunk81, 4, {{1u, 0xe2898a9830b230f4ull}, {2u, 0xdde1d7a078ce6c4cull}, {3u, 0x84e8560004cb47f0ull}, {4u, 0x9071adeeeadb316full}, {5u, 0x684dd6d0e2e57f3full}, {6u, 0x68660ce265c0c0a6ull}, {7u, 0x88d527489645c631ull}, {8u, 0x2b31de46475dfdccull}}},
    {"_ZThn16_N9Inspector14InspectorAgentD0Ev", &Thunk82, 4, {{1u, 0x4d9170aa28bba829ull}, {2u, 0x9da87797bf48ead9ull}, {3u, 0x4bc6cada73c94ff1ull}, {4u, 0x8ae168efb2e93a02ull}, {5u, 0x8c0d5f581aa723caull}, {6u, 0x93ef2070bfc2dff7ull}, {7u, 0x5eb1c13aa4a36850ull}, {8u, 0x6eab720bec34d215ull}}},
    {"_ZThn16_N9Inspector20InspectorTargetAgentD0Ev", &Thunk83, 4, {{1u, 0x6f32b31ec7cefaadull}, {2u, 0x0f41b5a9f7de7955ull}, {3u, 0x7181dca0083167f5ull}, {4u, 0x5eb7a7e7556bad76ull}, {5u, 0x381063453a78841eull}, {6u, 0x4bb122cc6eb473a3ull}, {7u, 0x6e2aa41ccd10c054ull}, {8u, 0xbed72ad70d22fc01ull}}},
    {"_ZThn16_N9Inspector21InspectorConsoleAgentD0Ev", &Thunk84, 4, {{1u, 0xbd8db97219111bf7ull}, {2u, 0x9c637338b60fb9abull}, {3u, 0x272e413b0a6528dbull}, {4u, 0xac6b0648568b80c4ull}, {5u, 0x2c482307070e4e80ull}, {6u, 0x502a0348762f0c25ull}, {7u, 0x221e814870de076eull}, {8u, 0xd566f1b9c3007817ull}}},
    {"_ZThn16_N9Inspector28InspectorScriptProfilerAgentD0Ev", &Thunk85, 4, {{1u, 0x1c536d3f101902baull}, {2u, 0x3d3d07708ec04422ull}, {3u, 0x50be738c442bba16ull}, {4u, 0x2954d7a70c5be089ull}, {5u, 0xe0226f1c5dcc10b9ull}, {6u, 0x10766e3b710d7db0ull}, {7u, 0x62543ff0fdd17ed3ull}, {8u, 0x87c67530ae820d2aull}}},
    {"_ZThn24_N7WebCore14DocumentLoaderD0Ev", &Thunk86, 4, {{1u, 0x825a6e0f07d76cdfull}, {2u, 0x27422bda96b2cd63ull}, {3u, 0xf6bade8f643c5e63ull}, {4u, 0x66bf20655b3b5c0cull}, {5u, 0xce42e7721c8d63e8ull}, {6u, 0x2599860db22a6c8dull}, {7u, 0xf6144342be8c2cf6ull}, {8u, 0xc96ad06dc63cbe5full}}},
    {"_ZThn24_N7WebCore19MediaResourceLoaderD0Ev", &Thunk87, 4, {{1u, 0xe83ac351747493f2ull}, {2u, 0x7c6ba9e01201c89aull}, {3u, 0x7cc00b40c706427eull}, {4u, 0xcb5eb7450287fbd1ull}, {5u, 0xb58979af9c15e041ull}, {6u, 0xa58108cf908f8888ull}, {7u, 0xb64f5d249282c69bull}, {8u, 0x335fad3d075676d2ull}}},
    {"_ZThn24_N9Inspector28InspectorScriptProfilerAgentD0Ev", &Thunk88, 4, {{1u, 0x9db873f9d427556aull}, {2u, 0x23c57a7abbe93292ull}, {3u, 0x1c028e4e7f4c3fa6ull}, {4u, 0x9d9040cee4c084d9ull}, {5u, 0xf8bea45f375d44a9ull}, {6u, 0x715bdf7b6371a260ull}, {7u, 0x239922b7120fb8e3ull}, {8u, 0x1c2c9120856259faull}}},
    {"_ZThn32_N7WebCore14DocumentLoaderD0Ev", &Thunk89, 4, {{1u, 0x046f53fee5e4dd6full}, {2u, 0x315815e4370048e3ull}, {3u, 0x95f8f877141a5de3ull}, {4u, 0xcbb784953658cb6cull}, {5u, 0x45fd9f8313fb1fe8ull}, {6u, 0x7296a9af5a73d51dull}, {7u, 0x1c82b821942744f6ull}, {8u, 0x4eb642ba3a7ff57full}}},
    {"_ZThn32_N7WebCore14XMLHttpRequestD0Ev", &Thunk90, 4, {{1u, 0x372b05b6be988530ull}, {2u, 0xf08ad23117c4cd34ull}, {3u, 0x4c4bbf2c99de80c8ull}, {4u, 0x2d51703b2ffa47cbull}, {5u, 0x670c532db8d371f7ull}, {6u, 0xb50435606630608eull}, {7u, 0xbfd8de0dd3f684e9ull}, {8u, 0xd4796856898296e4ull}}},
    {"_ZThn64_N7WebCore9FrameView17willEndLiveResizeEv", &Thunk91, 4, {{1u, 0x1285b353e00ee39aull}, {2u, 0xb7ce126dd412f9a5ull}, {3u, 0xd990b9ba65118939ull}, {4u, 0x5a0bd972d2f8deb2ull}, {5u, 0x9da6ac8e0ac76446ull}, {6u, 0xf2a7ceff77df2914ull}, {7u, 0x81da07bf99882ab4ull}, {8u, 0x97f3e94e6341b17dull}}},
    {"_ZThn64_N7WebCore9FrameView19willStartLiveResizeEv", &Thunk92, 4, {{1u, 0x38eccde1a8021fa9ull}, {2u, 0xf978f177b4c9bd86ull}, {3u, 0x01e9d45cbaec198eull}, {4u, 0x18f008d0ce7603c9ull}, {5u, 0xa85e5aefc2024fadull}, {6u, 0x95c7ee2dc5c066c3ull}, {7u, 0xb5743a853cfe926full}, {8u, 0xc800691c068f69b2ull}}},
};

std::uint64_t RunCase(const Case& c, std::uint32_t seed, unsigned char* arena) {
    Random random{seed};
    arenaBase = reinterpret_cast<std::uint64_t>(arena);
    for (std::size_t i = 0; i < ArenaSize; i += 8) {
        const std::uint64_t pick = random.Next() % 100;
        std::uint64_t value;
        if (pick < 45) value = Pointer(random, arenaBase);
        else if (pick < 60) value = 0;
        else if (pick < 75) value = random.Next() % 17;
        else value = random.Next();
        std::memcpy(arena + i, &value, 8);
    }
    std::uint64_t args[6];
    for (auto& arg : args) {
        const std::uint64_t pick = random.Next() % 100;
        arg = pick < 85 ? Pointer(random, arenaBase) : pick < 92 ? 0 : random.Next() % 4096;
    }
    mockRandom = Random{random.Next()};
    calls.clear();
    armed = true;
    const std::uint64_t result = c.wrapper(args[0], args[1], args[2], args[3], args[4], args[5]);
    armed = false;
    Digest digest;
    digest.base = arenaBase;
    for (const auto& call : calls) {
        digest.Word(call.id);
        for (const auto arg : call.args) digest.Word(arg);
    }
    for (std::size_t i = 0; i < ArenaSize; i += 8) {
        std::uint64_t word;
        std::memcpy(&word, arena + i, 8);
        digest.Word(word);
    }
    const std::uint64_t masks[] = {0, 0xff, 0xffff, 0xffffffffull, ~0ull};
    digest.Word(result & masks[c.ret]);
    return digest.value;
}

}  // namespace

static void TestRecordedBehaviour() {
    void* mapped = mmap(reinterpret_cast<void*>(ArenaAddress), ArenaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (mapped != reinterpret_cast<void*>(ArenaAddress)) {
        std::fprintf(stderr, "could not map the recording arena at %#llx: %s\n", static_cast<unsigned long long>(ArenaAddress), std::strerror(errno));
        Require(false);
    }
    auto* arena = static_cast<unsigned char*>(mapped);
    for (const Case& c : Cases) {
        for (const Seed& s : c.seeds) {
            if (RunCase(c, s.seed, arena) != s.digest) {
                std::fprintf(stderr, "%s seed %u digest mismatch\n", c.name, s.seed);
                Require(false);
            }
        }
    }
    munmap(arena, ArenaSize);
}
#endif

static void TestNullObjects() {
    for (const Wrapper wrapper : {&Thunk13, &Thunk76, &Thunk77, &Thunk78, &Thunk80}) {
        bool threw = false;
        try {
            wrapper(0, 0, 0, 0, 0, 0);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
}

int main() {
#if defined(__linux__)
    TestRecordedBehaviour();
#endif
    TestNullObjects();
    return 0;
}
