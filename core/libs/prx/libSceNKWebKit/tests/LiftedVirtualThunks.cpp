#include "prx/libSceNKWebKit/include/LiftedVirtualThunksExports.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#define LIFTED_RECORDED_RUN 1
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LiftedVirtualThunks check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Thunk = std::uint64_t (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, double);

namespace {

std::uint64_t APS5_VABI Thunk0(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKBundlePageOverlaySetAccessibilityClient(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk1(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKPageRunBeforeUnloadConfirmPanelResultListenerCall(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKPageRunJavaScriptAlertResultListenerCall(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk3(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKPageRunJavaScriptConfirmResultListenerCall(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk4(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKUserContentExtensionStoreRemove(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk5(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKWebsiteDataStoreClearAppBoundSession(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk6(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKWebsiteDataStoreHasAppBoundSession(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk7(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk8(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN3JSC8Bindings13RuntimeObjectD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk9(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN3JSC8Bindings13RuntimeObjectD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk10(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN3JSC8Debugger23setBreakpointsActivatedEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk11(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN3WTF21MemoryPressureHandler26endSimulatedMemoryPressureEv(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk12(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN3WTF22CrossThreadTaskHandler21setCompletionCallbackEONS_8FunctionIFvvEEE(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk13(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10JSDocumentC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk14(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10JSDocumentC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk15(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10JSDocumentD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk16(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10JSDocumentD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk17(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10ScrollView17setUseFixedLayoutEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk18(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore10ScrollView18setFixedLayoutSizeERKNS_7IntSizeE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk19(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore10ScrollView21setDelegatesScrollingEb(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk20(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11CachedFrame26setCachedFramePlatformDataESt10unique_ptrINS_23CachedFramePlatformDataESt14default_deleteIS2_EE(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk21(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer10setPreloadENS_16MediaPlayerEnums7PreloadE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk22(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11MediaPlayer11muteChangedEb(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk23(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    return _ZN7WebCore11MediaPlayer13volumeChangedEd(a0, a1, a2, a3, a4, a5, x0);
}

std::uint64_t APS5_VABI Thunk24(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer17setPreservesPitchEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk25(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer19prepareForRenderingEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk26(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11MediaPlayer21endSimulatedHDCPErrorEv(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk27(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11MediaPlayer21setShouldDisableSleepEb(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk28(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11MediaPlayer22setPrivateBrowsingModeEb(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk29(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore11MediaPlayer23beginSimulatedHDCPErrorEv(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk30(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer28setPreferredDynamicRangeModeENS_16DynamicRangeModeE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk31(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer7setSizeERKNS_7IntSizeE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk32(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore11MediaPlayer8setMutedEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk33(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    return _ZN7WebCore11MediaPlayer9setVolumeEd(a0, a1, a2, a3, a4, a5, x0);
}

std::uint64_t APS5_VABI Thunk34(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore12JSAudioTrackD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk35(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore12JSAudioTrackD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk36(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore13JSHTMLElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk37(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore13JSHTMLElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk38(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore13JSHTMLElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk39(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore13JSHTMLElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk40(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14JSWebAnimationC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk41(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14JSWebAnimationC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk42(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14JSWebAnimationD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk43(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14JSWebAnimationD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk44(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14ResourceHandle6cancelEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk45(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore14ScrollableArea21scrollbarStyleChangedENS_14ScrollbarStyleEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk46(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk47(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSFetchResponseD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk48(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSFetchResponseD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk49(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSSVGSVGElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk50(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSSVGSVGElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk51(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSSVGSVGElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk52(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore15JSSVGSVGElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk53(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore15XPathExpressionD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk54(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore15XPathExpressionD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk55(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore16HTMLMediaElement30setWebkitClosedCaptionsVisibleEb(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk56(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore16JSXMLHttpRequestD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk57(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore16JSXMLHttpRequestD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk58(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore17JSHTMLLinkElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk59(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore17JSHTMLLinkElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk60(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore17JSHTMLLinkElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk61(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore17JSHTMLLinkElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk62(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18ImageBufferBackend13drawConsumingERNS_15GraphicsContextERKNS_9FloatRectES5_RKNS_20ImagePaintingOptionsE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk63(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLImageElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk64(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLImageElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk65(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLImageElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk66(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLImageElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk67(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLInputElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk68(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLInputElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk69(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLMediaElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk70(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLMediaElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk71(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLMediaElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk72(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLMediaElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk73(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLVideoElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk74(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLVideoElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk75(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLVideoElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk76(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore18JSHTMLVideoElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk77(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSAnimationTimelineD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk78(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSAnimationTimelineD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk79(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSHTMLSelectElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk80(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSHTMLSelectElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk81(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSHTMLSelectElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk82(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore19JSHTMLSelectElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk83(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSCSSStyleDeclarationC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk84(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSCSSStyleDeclarationC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk85(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSCSSStyleDeclarationD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk86(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSCSSStyleDeclarationD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk87(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSTextTrackCueGenericD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk88(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore21JSTextTrackCueGenericD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk89(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore28InspectorFrontendClientLocal15changeSheetRectERKNS_9FloatRectE(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk90(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore4Page12whenUnnestedEON3WTF8FunctionIFvvEEE(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk91(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore4Page29applicationDidEnterBackgroundEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk92(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore4Page30applicationWillEnterForegroundEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk93(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSFileD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk94(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSFileD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk95(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSNodeC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk96(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSNodeC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk97(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSNodeD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk98(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6JSNodeD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk99(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore6Widget16removeFromParentEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk100(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore7JSRangeD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk101(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore7JSRangeD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk102(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore9JSElementC1ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk103(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore9JSElementC2ERKS0_(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk104(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore9JSElementD1Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk105(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN7WebCore9JSElementD2Ev(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk106(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN7WebCore9Scrollbar13setProportionEii(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk107(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    _ZN9Inspector15InspectorTarget17setResumeCallbackEON3WTF8FunctionIFvvEEE(a0, a1, a2, a3, a4, a5);
    return 0;
}

std::uint64_t APS5_VABI Thunk108(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZN9Inspector21InjectedScriptManager10disconnectEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk109(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK7WebCore15ActiveDOMObject18hasPendingActivityEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk110(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK7WebCore16HTMLMediaElement23webkitHasClosedCaptionsEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk111(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK7WebCore6Editor7Command11isSupportedEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk112(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK9Inspector18InjectedScriptBase31hasAccessToInspectedScriptStateEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk113(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK9Inspector21InspectorConsoleAgent22developerExtrasEnabledEv(a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI Thunk114(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    (void)x0;
    return _ZNK9Inspector22RemoteInspectionTarget20remoteControlAllowedEv(a0, a1, a2, a3, a4, a5);
}

}  // namespace

#ifdef LIFTED_RECORDED_RUN

namespace LiftedTrial {

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

inline std::uint64_t Pointer(Random& random, std::uint64_t base) {
    return base + 0x1000 + (random.Next() % (ArenaSize - 0x2000)) / 8 * 8;
}

struct Trial {
    std::uint64_t args[6];
    std::uint64_t mockSeed;
};

constexpr std::uint8_t ByteHints[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0x17, 0x18, 0x20, 0x28, 0x30, 0x40, 0x50, 0xb0, 0xf8, 0xfe, 0xff};
constexpr std::uint64_t WordHints[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0x18, 0x20, 0x28, 0x30, 0x40, 0x1000, 0x10000, 0x40000000ull,
                                       0xfffffffeull, 0xffffffffull, 0xffffffff00000000ull, 0x7fffffffffffffffull,
                                       0x800000001ull, 0x1000000001ull, 0x1800000001ull, 0x800000002ull, 0x1000000002ull, 0x1800000002ull};

inline Trial Prepare(std::uint64_t seed, unsigned char* arena) {
    Random random{seed};
    const std::uint64_t base = reinterpret_cast<std::uint64_t>(arena);
    const int mode = static_cast<int>(seed % 3);
    std::uint64_t pool[6];
    for (auto& p : pool) p = Pointer(random, base);
    const int share = 45 + 15 * static_cast<int>(random.Next() % 4);
    for (std::size_t i = 0; i < ArenaSize; i += 8) {
        const std::uint64_t pick = random.Next() % 100;
        std::uint64_t value;
        if (mode == 0) {
            if (pick < 45) value = Pointer(random, base);
            else if (pick < 60) value = 0;
            else if (pick < 75) value = random.Next() % 17;
            else value = random.Next();
        } else {
            const std::uint64_t h = WordHints[random.Next() % (sizeof WordHints / sizeof *WordHints)];
            const std::uint8_t hb = ByteHints[random.Next() % sizeof ByteHints];
            if (pick < static_cast<std::uint64_t>(share)) value = (mode == 2 && random.Next() % 2) ? pool[random.Next() % 6] : Pointer(random, base);
            else if (pick < static_cast<std::uint64_t>(share) + 4) value = 0;
            else if (pick < static_cast<std::uint64_t>(share) + 12) value = h;
            else if (random.Next() % 2) {
                std::uint64_t off = Pointer(random, base) - base;
                const int shift = 8 * static_cast<int>(random.Next() % 2);
                off = (off & ~(0xffull << shift)) | (static_cast<std::uint64_t>(hb) << shift);
                value = base + off;
            } else {
                value = random.Next() % 4 ? (random.Next() % 4 ? 0 : h) : Pointer(random, base) - base;
                const int shift = 8 * static_cast<int>(random.Next() % 8);
                value = (value & ~(0xffull << shift)) | (static_cast<std::uint64_t>(hb) << shift);
            }
        }
        std::memcpy(arena + i, &value, 8);
    }
    Trial trial{};
    for (auto& arg : trial.args) {
        const std::uint64_t pick = random.Next() % 100;
        if (mode == 0) arg = pick < 85 ? Pointer(random, base) : pick < 92 ? 0 : random.Next() % 4096;
        else arg = pick < 70 ? pool[random.Next() % 6] : pick < 92 ? Pointer(random, base) : pick < 96 ? 0 : WordHints[random.Next() % (sizeof WordHints / sizeof *WordHints)];
    }
    if (mode == 2) {
        for (int j = 1; j < 6; ++j) {
            if (random.Next() % 100 >= 25) continue;
            const int from = static_cast<int>(random.Next() % j);
            const std::uint64_t delta = 8 * (random.Next() % 9);
            if (trial.args[from] - base < ArenaSize - 0x2000) trial.args[j] = trial.args[from] + delta;
        }
    }
    trial.mockSeed = random.Next();
    return trial;
}

inline std::uint64_t MockReturn(Random& random, std::uint64_t base) {
    const std::uint64_t pick = random.Next() % 100;
    return pick < 50 ? Pointer(random, base) : pick < 75 ? 0 : random.Next();
}

struct Digest {
    std::uint64_t value = 0xcbf29ce484222325ull;
    std::uint64_t base;
    void Byte(unsigned char byte) {
        value = (value ^ byte) * 0x100000001b3ull;
    }
    void Word(std::uint64_t word) {
        if (word - base < ArenaSize) word = 0xa7e0000000000000ull | (word - base);
        for (int i = 0; i < 8; ++i) Byte(static_cast<unsigned char>(word >> (8 * i)));
    }
};

}

namespace {

constexpr int SlotCount = 1024;
constexpr std::uint64_t VtableAddress = 0x5b0000000000ull;
constexpr std::uint64_t StubBase = 0x5c0000000000ull;
constexpr std::uint64_t StubStride = 32;
constexpr std::uint64_t ControlAddress = StubBase + SlotCount * StubStride;
constexpr std::uint64_t IdAddress = ControlAddress + 8;
constexpr std::size_t MaxCalls = 64;

std::uint64_t StubAddress(int slot) {
    return StubBase + StubStride * static_cast<std::uint64_t>(slot);
}

[[noreturn]] void MappingFailed(const char* what, std::uint64_t address, unsigned long error) {
    std::fprintf(stderr, "LiftedVirtualThunks: cannot map %s at fixed address 0x%llx (error %lu)\n", what, static_cast<unsigned long long>(address), error);
    std::abort();
}

unsigned char* MapFixed(const char* what, std::uint64_t address, std::size_t size) {
#ifdef _WIN32
    void* mapped = VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mapped != reinterpret_cast<void*>(address)) MappingFailed(what, address, GetLastError());
#else
    void* mapped = mmap(reinterpret_cast<void*>(address), size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (mapped != reinterpret_cast<void*>(address)) MappingFailed(what, address, mapped == MAP_FAILED ? static_cast<unsigned long>(errno) : 0ul);
#endif
    return static_cast<unsigned char*>(mapped);
}

void Protect(const char* what, void* address, std::size_t size, bool executable) {
#ifdef _WIN32
    DWORD old;
    if (!VirtualProtect(address, size, executable ? PAGE_EXECUTE_READ : PAGE_READONLY, &old)) MappingFailed(what, reinterpret_cast<std::uint64_t>(address), GetLastError());
    if (executable) FlushInstructionCache(GetCurrentProcess(), address, size);
#else
    if (mprotect(address, size, executable ? PROT_READ | PROT_EXEC : PROT_READ) != 0) MappingFailed(what, reinterpret_cast<std::uint64_t>(address), static_cast<unsigned long>(errno));
#endif
}

struct CallRecord {
    std::uint32_t id;
    std::uint64_t args[6];
    std::uint64_t x[2];
};

std::vector<CallRecord> calls;
std::mt19937_64 mockRandom;
std::uint64_t arenaBase = 0;

std::uint64_t Bits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double FromBits(std::uint64_t bits) {
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI RecordingStub(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0, double x1) {
    CallRecord record{};
    std::memcpy(&record.id, reinterpret_cast<const void*>(IdAddress), sizeof(record.id));
    record.args[0] = a0;
    record.args[1] = a1;
    record.args[2] = a2;
    record.args[3] = a3;
    record.args[4] = a4;
    record.args[5] = a5;
    record.x[0] = Bits(x0);
    record.x[1] = Bits(x1);
    calls.push_back(record);
    const std::uint64_t pick = mockRandom() % 100;
    std::uint64_t value;
    if (pick < 50) value = arenaBase + 0x1000 + (mockRandom() % (LiftedTrial::ArenaSize - 0x2000)) / 8 * 8;
    else if (pick < 75) value = 0;
    else value = mockRandom();
    mockRandom();
    return value;
}

void InstallStubs() {
    unsigned char* region = MapFixed("recording stubs", StubBase, SlotCount * StubStride + 0x1000);
    const std::uint64_t handler = reinterpret_cast<std::uint64_t>(&RecordingStub);
    std::memcpy(region + SlotCount * StubStride, &handler, sizeof(handler));
    for (int slot = 0; slot < SlotCount; ++slot) {
        unsigned char* code = region + StubStride * static_cast<std::uint64_t>(slot);
        std::memset(code, 0xcc, StubStride);
        const std::uint32_t id = static_cast<std::uint32_t>(slot);
        const std::uint64_t idAddress = IdAddress;
        const std::int32_t relative = static_cast<std::int32_t>(ControlAddress - (StubAddress(slot) + 20));
        code[0] = 0xb8;
        std::memcpy(code + 1, &id, 4);
        code[5] = 0xa3;
        std::memcpy(code + 6, &idAddress, 8);
        code[14] = 0xff;
        code[15] = 0x25;
        std::memcpy(code + 16, &relative, 4);
    }
    Protect("recording stubs", region, SlotCount * StubStride, true);
    unsigned char* vtable = MapFixed("fake vtable", VtableAddress, SlotCount * 8);
    for (int slot = 0; slot < SlotCount; ++slot) {
        const std::uint64_t address = StubAddress(slot);
        std::memcpy(vtable + 8 * slot, &address, 8);
    }
    Protect("fake vtable", vtable, SlotCount * 8, false);
}

LiftedTrial::Trial PrepareVirtual(std::uint64_t seed, unsigned char* arena) {
    auto trial = LiftedTrial::Prepare(seed, arena);
    LiftedTrial::Random r{seed ^ 0x76a1b2c3d4e5f607ull};
    const std::uint64_t base = arenaBase;
    std::uint64_t objs[16];
    for (int k = 0; k < 16; ++k) objs[k] = base + 0x2000 + 0x400 * k * 2;
    for (int k = 0; k < 16; ++k) {
        const std::uint64_t o = objs[k] - base;
        std::memcpy(arena + o, &VtableAddress, 8);
        for (int off = 8; off <= 0x40; off += 8) {
            const std::uint64_t pickv[10] = {1, VtableAddress, 0, objs[r.Next() % 16], objs[r.Next() % 16], objs[r.Next() % 16], objs[r.Next() % 16] + 0x10, r.Next() & 0x0101010101010101ull, 2, objs[r.Next() % 16]};
            const std::uint64_t w = pickv[r.Next() % 10];
            if (r.Next() % 6) std::memcpy(arena + o + off, &w, 8);
        }
    }
    for (int i = 0; i < 6; ++i) {
        const auto p = r.Next() % 100;
        if (i == 0 ? p < 85 : p < 30) trial.args[i] = objs[r.Next() % 16] + (r.Next() % 4 == 0 ? 0x10 : 0);
        else if (i && p < 40 && trial.args[0] - base < LiftedTrial::ArenaSize - 0x400) std::memcpy(&trial.args[i], arena + (trial.args[0] - base) + 8 * (r.Next() % 8), 8);
    }
    for (int i = 1; i < 6; ++i)
        if (r.Next() % 100 < 15) trial.args[i] = StubAddress(static_cast<int>(r.Next() % SlotCount));
    return trial;
}

struct Seed {
    std::uint32_t seed;
    std::uint64_t digest;
};

struct Case {
    const char* name;
    Thunk thunk;
    int xmm;
    int ret;
    std::vector<Seed> seeds;
};

const std::vector<Case> Cases = {
    {"WKBundlePageOverlaySetAccessibilityClient", &Thunk0, 0, 4, {{1u, 0x2a65343594610cdcull}, {3u, 0x12a7e0c4ab078bf8ull}, {5u, 0x1ef15d0e5c0b8b86ull}, {13u, 0x80f4c8039f24c849ull}, {17u, 0xd62b0d9a8161aef9ull}, {21u, 0x7264ad181acc5b93ull}, {25u, 0x35c92806bb48dc5eull}, {27u, 0x695dc799c93f5001ull}}},
    {"WKPageRunBeforeUnloadConfirmPanelResultListenerCall", &Thunk1, 0, 4, {{1u, 0x26aea5963364a24cull}, {3u, 0x471aabbd0330d888ull}, {14u, 0xdc2732b7eddad828ull}, {16u, 0x79b4e109f4a19267ull}, {19u, 0x5220521da578c8c1ull}, {23u, 0x48bdfda457201dd7ull}, {26u, 0x9f124a310f68b5bdull}, {28u, 0x51cd74d60d23d51aull}}},
    {"WKPageRunJavaScriptAlertResultListenerCall", &Thunk2, 0, 4, {{1u, 0x26aea5963364a24cull}, {3u, 0x471aabbd0330d888ull}, {14u, 0xdc2732b7eddad828ull}, {16u, 0x79b4e109f4a19267ull}, {19u, 0x5220521da578c8c1ull}, {23u, 0x48bdfda457201dd7ull}, {26u, 0x9f124a310f68b5bdull}, {28u, 0x51cd74d60d23d51aull}}},
    {"WKPageRunJavaScriptConfirmResultListenerCall", &Thunk3, 0, 4, {{1u, 0x26aea5963364a24cull}, {3u, 0x471aabbd0330d888ull}, {14u, 0xdc2732b7eddad828ull}, {16u, 0x79b4e109f4a19267ull}, {19u, 0x5220521da578c8c1ull}, {23u, 0x48bdfda457201dd7ull}, {26u, 0x9f124a310f68b5bdull}, {28u, 0x51cd74d60d23d51aull}}},
    {"WKUserContentExtensionStoreRemove", &Thunk4, 0, 4, {{9u, 0x5a798095c7c249f7ull}, {25u, 0x4661cf56198e2bf0ull}, {36u, 0x9c82a329c3bf94abull}, {44u, 0xc0f09b16e2c49bf1ull}, {45u, 0xfad30e3077ae8478ull}, {53u, 0x4cd02a1fda88132cull}, {64u, 0x153ec4f35f6272fbull}, {84u, 0xe8bdc237b41ede71ull}}},
    {"WKWebsiteDataStoreClearAppBoundSession", &Thunk5, 0, 4, {{1u, 0xf82b0f5add50af5full}, {3u, 0xac967b76b40901bdull}, {12u, 0xed0c6c5c6e4e00d4ull}, {25u, 0x89ab30741fbc5374ull}, {31u, 0xf1e3c9cc14a685a5ull}, {33u, 0x5535072a62724fedull}, {35u, 0xc66d7dc03217c0c6ull}, {42u, 0xd63f32d496dff8f1ull}}},
    {"WKWebsiteDataStoreHasAppBoundSession", &Thunk6, 0, 4, {{1u, 0x54b644e6bef8fd54ull}, {3u, 0x287c6a8806a373a3ull}, {12u, 0x99a8862ea4b0d46eull}, {25u, 0xee23f625558aca46ull}, {31u, 0xe6fde4eea3e84e43ull}, {33u, 0x03210d6fb0e2d540ull}, {35u, 0x0bc2afcac2c54980ull}, {42u, 0x3821c68b928f4841ull}}},
    {"WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations", &Thunk7, 0, 4, {{1u, 0xf82b0f5add50af5full}, {3u, 0xac967b76b40901bdull}, {12u, 0xed0c6c5c6e4e00d4ull}, {25u, 0x89ab30741fbc5374ull}, {31u, 0xf1e3c9cc14a685a5ull}, {33u, 0x5535072a62724fedull}, {35u, 0xc66d7dc03217c0c6ull}, {42u, 0xd63f32d496dff8f1ull}}},
    {"_ZN3JSC8Bindings13RuntimeObjectD1Ev", &Thunk8, 0, 3, {{74u, 0x1a7737c73418e1f8ull}, {77u, 0x147d187b615f54fdull}, {105u, 0x28e4d3d0d156a628ull}, {147u, 0x670ee5719dea9c1dull}, {170u, 0x75a882d3fe81368eull}, {190u, 0x2a7ee790c07a559cull}, {244u, 0xceb7e054db18d2cbull}, {274u, 0xf195e6b3d0848031ull}}},
    {"_ZN3JSC8Bindings13RuntimeObjectD2Ev", &Thunk9, 0, 3, {{74u, 0x1a7737c73418e1f8ull}, {77u, 0x147d187b615f54fdull}, {105u, 0x28e4d3d0d156a628ull}, {147u, 0x670ee5719dea9c1dull}, {170u, 0x75a882d3fe81368eull}, {190u, 0x2a7ee790c07a559cull}, {244u, 0xceb7e054db18d2cbull}, {274u, 0xf195e6b3d0848031ull}}},
    {"_ZN3JSC8Debugger23setBreakpointsActivatedEb", &Thunk10, 0, 3, {{87u, 0xf93c52bd5fc4ba54ull}, {231u, 0x9d2908187e631b60ull}, {233u, 0x9b481218685ab129ull}, {251u, 0x4ffcc23da88eb23bull}, {269u, 0x8085de4db7594b28ull}, {306u, 0xa2a082b1f67d9f1aull}, {345u, 0x58b5b384257e4d1eull}, {360u, 0x4bcd8324f798d2adull}}},
    {"_ZN3WTF21MemoryPressureHandler26endSimulatedMemoryPressureEv", &Thunk11, 0, 0, {{273u, 0xea94cec992fe6da6ull}, {459u, 0x03f683d18726281full}, {608u, 0xaf729b724df60ec5ull}, {747u, 0x0ad7a2ac90928c29ull}, {780u, 0x57d003f7f8c65b46ull}, {1126u, 0xc0ceb0d65382bd73ull}, {1185u, 0xd9226f13e214ffc3ull}, {1260u, 0x56a7ac17a6e968a5ull}}},
    {"_ZN3WTF22CrossThreadTaskHandler21setCompletionCallbackEONS_8FunctionIFvvEEE", &Thunk12, 0, 0, {{853u, 0x74ba40cba4e13d19ull}, {4097u, 0x7d57552f22397b3eull}, {4205u, 0xa1db491639a00407ull}, {4908u, 0xbfcc1fb82d9005c1ull}, {5358u, 0x8ddb39e5f5fd25c2ull}, {7526u, 0x35e543295cfa6501ull}, {7542u, 0xb2c1fe7981c1aa97ull}, {8031u, 0x37aa83c8b1551557ull}}},
    {"_ZN7WebCore10JSDocumentC1ERKS0_", &Thunk13, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore10JSDocumentC2ERKS0_", &Thunk14, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore10JSDocumentD1Ev", &Thunk15, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore10JSDocumentD2Ev", &Thunk16, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore10ScrollView17setUseFixedLayoutEb", &Thunk17, 0, 1, {{1u, 0xe0dc0f606d27c158ull}, {7u, 0x7689c29c0d5de7f0ull}, {9u, 0x81dea599f3a39981ull}, {14u, 0x848c79aaf0974b10ull}, {15u, 0xff02a4c246eeeecfull}, {18u, 0x5b31ebdb64e054ffull}, {22u, 0xe05b6a47803b06a4ull}, {23u, 0x542591bf809d51beull}}},
    {"_ZN7WebCore10ScrollView18setFixedLayoutSizeERKNS_7IntSizeE", &Thunk18, 0, 4, {{1u, 0xde385c875a78adbbull}, {3u, 0xac62868f75c126f5ull}, {6u, 0xbd381570c807a168ull}, {7u, 0x3901df3b77822116ull}, {11u, 0x9e8bcaaebd5110ecull}, {14u, 0x7f458052359f2fdfull}, {15u, 0xef79d8a99f27c3a5ull}, {16u, 0xbcbb4804b13758cfull}}},
    {"_ZN7WebCore10ScrollView21setDelegatesScrollingEb", &Thunk19, 0, 0, {{1u, 0xa96e2a13cc9bc71dull}, {3u, 0x9810554fd56e2333ull}, {6u, 0x7ee539e8b417ed6cull}, {7u, 0x8607808e0f3ddf0full}, {9u, 0x523a767afac52465ull}, {11u, 0x6e8dacf71b7f97ddull}, {12u, 0x699f649d553adc7cull}, {14u, 0x667369ff38133b31ull}}},
    {"_ZN7WebCore11CachedFrame26setCachedFramePlatformDataESt10unique_ptrINS_23CachedFramePlatformDataESt14default_deleteIS2_EE", &Thunk20, 0, 0, {{1540u, 0x5b84fc2340307b35ull}, {2378u, 0x053bf1a2a3ae8645ull}, {4205u, 0x014d4006d1d990c7ull}, {9634u, 0x3dab259ad7352472ull}, {9775u, 0xb38030af6156267bull}, {9802u, 0x1802a95b71fdc1d0ull}, {15451u, 0x4af6838526d88fe6ull}, {15685u, 0xf98665c8d311a965ull}}},
    {"_ZN7WebCore11MediaPlayer10setPreloadENS_16MediaPlayerEnums7PreloadE", &Thunk21, 0, 4, {{379u, 0x2207779c997885c3ull}, {2078u, 0xd42a9ce2f58d65c7ull}, {2826u, 0x37349e11978f1db8ull}, {4205u, 0x2da058db018c8f2eull}, {5240u, 0x77adae69fa6b2d87ull}, {5521u, 0x67e76974f6c46edfull}, {6108u, 0xd79e35f4026d4b7cull}, {8031u, 0x19e0ef7156c44b9cull}}},
    {"_ZN7WebCore11MediaPlayer11muteChangedEb", &Thunk22, 0, 0, {{2u, 0x69ebf9849750afdeull}, {3u, 0xab5d8195ae180ad8ull}, {4u, 0x13990a93df5e62c6ull}, {12u, 0x0604c88bd1c2ab15ull}, {14u, 0xb40ed6e82ceb29d9ull}, {16u, 0x799fa167d3d002e5ull}, {21u, 0xbe33c912e24c743full}, {24u, 0xa459a02adc2a3068ull}}},
    {"_ZN7WebCore11MediaPlayer13volumeChangedEd", &Thunk23, 1, 4, {{2u, 0x0b077414294e4fb2ull}, {3u, 0x4e77bee5970e7c8eull}, {4u, 0x349dc66885f2b69dull}, {5u, 0xf71e4bbd4e856cf7ull}, {12u, 0x23f0e705056a0e5dull}, {14u, 0x9a8c51ac36eab5d5ull}, {16u, 0x795d60963edb4ff5ull}, {18u, 0x9c99b4ec91b9b066ull}}},
    {"_ZN7WebCore11MediaPlayer17setPreservesPitchEb", &Thunk24, 0, 4, {{379u, 0x500c94bfc233d64full}, {2078u, 0x020117e0d8975d09ull}, {2826u, 0x2867792ea5c8851aull}, {4205u, 0xd832e74c52bcbbd0ull}, {5240u, 0x46897f72ef582d59ull}, {5521u, 0x6ccd900aaccd21eeull}, {6108u, 0x86ce514eb295ce90ull}, {8031u, 0xa4445f9ec1073a95ull}}},
    {"_ZN7WebCore11MediaPlayer19prepareForRenderingEv", &Thunk25, 0, 4, {{379u, 0xd4042cfdd6ca4390ull}, {2078u, 0x045a008eefa9fa42ull}, {2826u, 0x19e45e505ad2a698ull}, {4205u, 0xf434d06b2fe8e2f7ull}, {5240u, 0x5949534f4f3098a7ull}, {5521u, 0x1d8ec1b8e8589d9eull}, {6108u, 0x0c99e6265bcf4598ull}, {8031u, 0x8781f1b837f0b305ull}}},
    {"_ZN7WebCore11MediaPlayer21endSimulatedHDCPErrorEv", &Thunk26, 0, 0, {{379u, 0x01e613a83f370cc4ull}, {2078u, 0x374069cf814f9cd7ull}, {2826u, 0xd3dc8576c9729d16ull}, {4205u, 0x0d797c56e1bf3ba2ull}, {5240u, 0xc536b9dd5c1af234ull}, {5521u, 0xd379eaeb6cec123dull}, {6108u, 0x96d96ec974543d7full}, {8031u, 0x185e3e353f8638e8ull}}},
    {"_ZN7WebCore11MediaPlayer21setShouldDisableSleepEb", &Thunk27, 0, 0, {{379u, 0x754795d8031cf549ull}, {2078u, 0xbae36f18f69cfc4full}, {2826u, 0xd574c0ac6f6c5f4bull}, {4205u, 0x50cfaf93ce7b9143ull}, {5240u, 0x66950df7775a71dfull}, {5521u, 0x01c458d3a837fa68ull}, {6108u, 0xdf09285359696073ull}, {8031u, 0xd0559bff9e5bd6c1ull}}},
    {"_ZN7WebCore11MediaPlayer22setPrivateBrowsingModeEb", &Thunk28, 0, 0, {{379u, 0xff19534d3bd1f490ull}, {2078u, 0x09d73449a516eca2ull}, {2826u, 0x32887606d5a3e61aull}, {4205u, 0x981c5379b8a01716ull}, {5240u, 0x4d5fd6d3907ced2eull}, {5521u, 0x4a93753ef53ed39aull}, {6108u, 0xca11f49b84ccdae9ull}, {8031u, 0xeda8cec1e4288fbbull}}},
    {"_ZN7WebCore11MediaPlayer23beginSimulatedHDCPErrorEv", &Thunk29, 0, 0, {{379u, 0xc9f15d5d03fc9a23ull}, {2078u, 0xb78ba069be9fc3acull}, {2826u, 0x5e8d2777c8410f45ull}, {4205u, 0x8ea55a87f7d4f72dull}, {5240u, 0xc6ff386a100c2fcbull}, {5521u, 0xafd5f9833ac51cd2ull}, {6108u, 0x504845f0b3b3e12cull}, {8031u, 0x32e0503b9ce9f09bull}}},
    {"_ZN7WebCore11MediaPlayer28setPreferredDynamicRangeModeENS_16DynamicRangeModeE", &Thunk30, 0, 4, {{379u, 0x12f2b2a1915a0f89ull}, {2078u, 0x177acd862e88058bull}, {2826u, 0x841bf384feed5f0cull}, {4205u, 0x0c2aa217b020a8c2ull}, {5240u, 0xa02252f01c7a116bull}, {5521u, 0x0f4c183e224fed4bull}, {6108u, 0x19f82517f68e0429ull}, {8031u, 0x3b64e4ebbab4dc62ull}}},
    {"_ZN7WebCore11MediaPlayer7setSizeERKNS_7IntSizeE", &Thunk31, 0, 4, {{379u, 0xdb05dc3e4fbe43a3ull}, {2826u, 0x9e34a0fea3b541fbull}, {4205u, 0x35e7961650a22fecull}, {5240u, 0x84dfcf957fddb14bull}, {5521u, 0x2d319b75644da837ull}, {6108u, 0xf28c0f72c71bfc76ull}, {8031u, 0x1d691d421b83ff73ull}, {11273u, 0xc15800dae9c87c24ull}}},
    {"_ZN7WebCore11MediaPlayer8setMutedEb", &Thunk32, 0, 4, {{379u, 0xfe3ff05f415fba27ull}, {2078u, 0x9327578dcd783a07ull}, {2826u, 0x14b469d39c17b785ull}, {4205u, 0xfaca14415c5cb1f2ull}, {5240u, 0xdc8b54540e9c1276ull}, {5521u, 0xd4adcac1e99255aeull}, {6108u, 0x881997301ed59c85ull}, {8031u, 0xab05f2972eebaae5ull}}},
    {"_ZN7WebCore11MediaPlayer9setVolumeEd", &Thunk33, 1, 4, {{379u, 0x483d7696c9baf6a3ull}, {2078u, 0x4ec7ecdba8f3b4d5ull}, {2826u, 0x5be918e72d9182d2ull}, {4205u, 0xfc1df79a894f35f9ull}, {5240u, 0x2cff0f2773df89acull}, {5521u, 0xbd7ce8b82eefcc51ull}, {6108u, 0xac78fb3ccfb32499ull}, {8031u, 0x437d04e820ff18c8ull}}},
    {"_ZN7WebCore12JSAudioTrackD1Ev", &Thunk34, 0, 3, {{12u, 0x44fd246449b67f04ull}, {45u, 0x831a4b6a8f40beaeull}, {98u, 0x30889f83453bfe46ull}, {127u, 0x99ab5df4f8125277ull}, {136u, 0xff828a90166c4179ull}, {209u, 0x9ff05184e349f383ull}, {216u, 0xf80dd12ac1e14978ull}, {218u, 0xdbe507361fab7698ull}}},
    {"_ZN7WebCore12JSAudioTrackD2Ev", &Thunk35, 0, 3, {{12u, 0x44fd246449b67f04ull}, {45u, 0x831a4b6a8f40beaeull}, {98u, 0x30889f83453bfe46ull}, {127u, 0x99ab5df4f8125277ull}, {136u, 0xff828a90166c4179ull}, {209u, 0x9ff05184e349f383ull}, {216u, 0xf80dd12ac1e14978ull}, {218u, 0xdbe507361fab7698ull}}},
    {"_ZN7WebCore13JSHTMLElementC1ERKS0_", &Thunk36, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore13JSHTMLElementC2ERKS0_", &Thunk37, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore13JSHTMLElementD1Ev", &Thunk38, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore13JSHTMLElementD2Ev", &Thunk39, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore14JSWebAnimationC1ERKS0_", &Thunk40, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore14JSWebAnimationC2ERKS0_", &Thunk41, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore14JSWebAnimationD1Ev", &Thunk42, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore14JSWebAnimationD2Ev", &Thunk43, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore14ResourceHandle6cancelEv", &Thunk44, 0, 4, {{940u, 0x2690dce295c911fdull}, {2187u, 0x750d9deab1c23fbcull}, {2523u, 0xaba58736f0b06fb3ull}, {2666u, 0x4a3c16f12733f90cull}, {3440u, 0x78bb8a54583b677eull}, {4727u, 0xb8c0aad55810c9e2ull}, {6706u, 0xfdc98fcaa08cafc3ull}, {6715u, 0xd5040b2c80d25578ull}}},
    {"_ZN7WebCore14ScrollableArea21scrollbarStyleChangedENS_14ScrollbarStyleEb", &Thunk45, 0, 4, {{1u, 0xc65e24fb96675b83ull}, {3u, 0x8721e9634c887006ull}, {4u, 0x014bc5e91c9d4d64ull}, {6u, 0x90e245702b59a081ull}, {7u, 0xcdf124c6140a470cull}, {9u, 0x7335c55023a77564ull}, {11u, 0x8cf16928e0fcb850ull}, {12u, 0xc3103bab5a31b99full}}},
    {"_ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE", &Thunk46, 0, 4, {{1u, 0x98f1592d0f168b7eull}, {3u, 0x66019c924801633aull}, {4u, 0x5973499fca2238d2ull}, {6u, 0xfc814fcbf6f2982dull}, {7u, 0x4bf57fa9e3be765eull}, {9u, 0x13b6ed992bc31da0ull}, {11u, 0x651e3ad481b431c9ull}, {12u, 0x59ae481413872451ull}}},
    {"_ZN7WebCore15JSFetchResponseD1Ev", &Thunk47, 0, 3, {{16u, 0xb06e79bf94a815e7ull}, {92u, 0xa1f496dd57b87367ull}, {96u, 0x29eaf98582de306dull}, {134u, 0x4e33c31c3e43a38eull}, {140u, 0xaab4dcef456dfe3bull}, {148u, 0xb11f64d8ff2fa8acull}, {186u, 0x1ae733383c813579ull}, {226u, 0xcec9320c0d70aedeull}}},
    {"_ZN7WebCore15JSFetchResponseD2Ev", &Thunk48, 0, 3, {{16u, 0xb06e79bf94a815e7ull}, {92u, 0xa1f496dd57b87367ull}, {96u, 0x29eaf98582de306dull}, {134u, 0x4e33c31c3e43a38eull}, {140u, 0xaab4dcef456dfe3bull}, {148u, 0xb11f64d8ff2fa8acull}, {186u, 0x1ae733383c813579ull}, {226u, 0xcec9320c0d70aedeull}}},
    {"_ZN7WebCore15JSSVGSVGElementC1ERKS0_", &Thunk49, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore15JSSVGSVGElementC2ERKS0_", &Thunk50, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore15JSSVGSVGElementD1Ev", &Thunk51, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore15JSSVGSVGElementD2Ev", &Thunk52, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore15XPathExpressionD1Ev", &Thunk53, 0, 0, {{6u, 0xfe87b91026995f8aull}, {8u, 0x5040d6dea1bd3278ull}, {11u, 0x189751987e26351eull}, {13u, 0xf68ae7fb27cfe664ull}, {18u, 0x21f5dc51af288db0ull}, {22u, 0x429e44bbf435c543ull}, {24u, 0x0e1103b0e065dbbeull}, {28u, 0xdd9817a6d6e29b1bull}}},
    {"_ZN7WebCore15XPathExpressionD2Ev", &Thunk54, 0, 0, {{6u, 0xfe87b91026995f8aull}, {8u, 0x5040d6dea1bd3278ull}, {11u, 0x189751987e26351eull}, {13u, 0xf68ae7fb27cfe664ull}, {18u, 0x21f5dc51af288db0ull}, {22u, 0x429e44bbf435c543ull}, {24u, 0x0e1103b0e065dbbeull}, {28u, 0xdd9817a6d6e29b1bull}}},
    {"_ZN7WebCore16HTMLMediaElement30setWebkitClosedCaptionsVisibleEb", &Thunk55, 0, 4, {{1u, 0xd41244135a54f9f5ull}, {3u, 0x7f802b7b5e40d2b6ull}, {4u, 0xb3a0a6907be3e02dull}, {6u, 0x20d3df5c38e2975aull}, {7u, 0x1d3513c271951b19ull}, {9u, 0x015224a90b43116dull}, {11u, 0xc4fe33be7f5bb65eull}, {12u, 0x6a4a2b8b29ad0d5eull}}},
    {"_ZN7WebCore16JSXMLHttpRequestD1Ev", &Thunk56, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore16JSXMLHttpRequestD2Ev", &Thunk57, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore17JSHTMLLinkElementC1ERKS0_", &Thunk58, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore17JSHTMLLinkElementC2ERKS0_", &Thunk59, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore17JSHTMLLinkElementD1Ev", &Thunk60, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore17JSHTMLLinkElementD2Ev", &Thunk61, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18ImageBufferBackend13drawConsumingERNS_15GraphicsContextERKNS_9FloatRectES5_RKNS_20ImagePaintingOptionsE", &Thunk62, 0, 4, {{1u, 0x6871985e452953c1ull}, {3u, 0x075844525416bf1dull}, {4u, 0xf99068db6853d214ull}, {6u, 0xb2462c83bf3966a9ull}, {7u, 0x93c58f31f00dced2ull}, {9u, 0x57d1677f704f3e7eull}, {11u, 0x23cd5e1f75f21cd8ull}, {12u, 0xe2480a54541b0104ull}}},
    {"_ZN7WebCore18JSHTMLImageElementC1ERKS0_", &Thunk63, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLImageElementC2ERKS0_", &Thunk64, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLImageElementD1Ev", &Thunk65, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLImageElementD2Ev", &Thunk66, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLInputElementD1Ev", &Thunk67, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLInputElementD2Ev", &Thunk68, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLMediaElementC1ERKS0_", &Thunk69, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLMediaElementC2ERKS0_", &Thunk70, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLMediaElementD1Ev", &Thunk71, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLMediaElementD2Ev", &Thunk72, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLVideoElementC1ERKS0_", &Thunk73, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLVideoElementC2ERKS0_", &Thunk74, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore18JSHTMLVideoElementD1Ev", &Thunk75, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore18JSHTMLVideoElementD2Ev", &Thunk76, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore19JSAnimationTimelineD1Ev", &Thunk77, 0, 3, {{31u, 0x34182712067d64d4ull}, {96u, 0x45064a755b87923eull}, {98u, 0x0589906788a595fdull}, {110u, 0x482690d6ad3cb35bull}, {281u, 0xf061d59a3dd0d586ull}, {312u, 0x9fdea73e42ce922eull}, {319u, 0x38562ed61c53834full}, {345u, 0x0590972a46be56d2ull}}},
    {"_ZN7WebCore19JSAnimationTimelineD2Ev", &Thunk78, 0, 3, {{31u, 0x34182712067d64d4ull}, {96u, 0x45064a755b87923eull}, {98u, 0x0589906788a595fdull}, {110u, 0x482690d6ad3cb35bull}, {281u, 0xf061d59a3dd0d586ull}, {312u, 0x9fdea73e42ce922eull}, {319u, 0x38562ed61c53834full}, {345u, 0x0590972a46be56d2ull}}},
    {"_ZN7WebCore19JSHTMLSelectElementC1ERKS0_", &Thunk79, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore19JSHTMLSelectElementC2ERKS0_", &Thunk80, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore19JSHTMLSelectElementD1Ev", &Thunk81, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore19JSHTMLSelectElementD2Ev", &Thunk82, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore21JSCSSStyleDeclarationC1ERKS0_", &Thunk83, 0, 4, {{6u, 0x4f5e784d3e50dec1ull}, {14u, 0x68e31b7f93a09b90ull}, {24u, 0xe171d3a9fe8f324dull}, {33u, 0x310a9803dee1d2dbull}, {34u, 0xb42f489e5b527bc4ull}, {72u, 0xdf695b56f7917b15ull}, {82u, 0xbfa86e1520997cecull}, {90u, 0x2e96e0f9ef42f2c0ull}}},
    {"_ZN7WebCore21JSCSSStyleDeclarationC2ERKS0_", &Thunk84, 0, 4, {{6u, 0x4f5e784d3e50dec1ull}, {14u, 0x68e31b7f93a09b90ull}, {24u, 0xe171d3a9fe8f324dull}, {33u, 0x310a9803dee1d2dbull}, {34u, 0xb42f489e5b527bc4ull}, {72u, 0xdf695b56f7917b15ull}, {82u, 0xbfa86e1520997cecull}, {90u, 0x2e96e0f9ef42f2c0ull}}},
    {"_ZN7WebCore21JSCSSStyleDeclarationD1Ev", &Thunk85, 0, 4, {{2u, 0x3bf9bb18a9ea6b6full}, {3u, 0xe44c7c5d103df358ull}, {4u, 0xb6ac0c1f69722d3dull}, {5u, 0xee638106056c664eull}, {12u, 0x67590d800d9c5fe5ull}, {14u, 0xc652a54f136ad9b8ull}, {16u, 0xac6041cea97548c3ull}, {18u, 0xf218ba73e722f567ull}}},
    {"_ZN7WebCore21JSCSSStyleDeclarationD2Ev", &Thunk86, 0, 4, {{2u, 0x3bf9bb18a9ea6b6full}, {3u, 0xe44c7c5d103df358ull}, {4u, 0xb6ac0c1f69722d3dull}, {5u, 0xee638106056c664eull}, {12u, 0x67590d800d9c5fe5ull}, {14u, 0xc652a54f136ad9b8ull}, {16u, 0xac6041cea97548c3ull}, {18u, 0xf218ba73e722f567ull}}},
    {"_ZN7WebCore21JSTextTrackCueGenericD1Ev", &Thunk87, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore21JSTextTrackCueGenericD2Ev", &Thunk88, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore28InspectorFrontendClientLocal15changeSheetRectERKNS_9FloatRectE", &Thunk89, 0, 4, {{1u, 0xe07af4c2271c4d84ull}, {3u, 0x120f203a3e622990ull}, {4u, 0x4956470152dde489ull}, {6u, 0x2fa106b26cf6fe74ull}, {7u, 0x99d3470e1a39a3dfull}, {9u, 0xf9eff827e81b55d3ull}, {11u, 0x3500563bb0a1ca59ull}, {12u, 0x2ddb790f3ad0c5d1ull}}},
    {"_ZN7WebCore4Page12whenUnnestedEON3WTF8FunctionIFvvEEE", &Thunk90, 0, 0, {{4912u, 0x5ac30836b51790b8ull}, {5314u, 0x500ce8d91f8c91ceull}, {6392u, 0xdd9f79676d5446c9ull}, {12928u, 0xd3e912003f52eaecull}, {13487u, 0x20b178479f003472ull}, {13525u, 0xd9c951c40437e879ull}, {18824u, 0x1cd7768722d45a2bull}, {19874u, 0x7cab01ddb3cadd4eull}}},
    {"_ZN7WebCore4Page29applicationDidEnterBackgroundEv", &Thunk91, 0, 4, {{1691u, 0xa48977d88cd1b7f7ull}, {1757u, 0xd6876d49290b6bcdull}, {3729u, 0x66f156cee27f7042ull}, {5668u, 0x1061ab1053e0b57dull}, {6057u, 0x8951d48ffc10552cull}, {7821u, 0xfa847edd7e72f9a7ull}, {8186u, 0x0b8b28e5f2c2c568ull}, {8748u, 0x3f08c0780dbef127ull}}},
    {"_ZN7WebCore4Page30applicationWillEnterForegroundEv", &Thunk92, 0, 4, {{1691u, 0xac23792a35209aa6ull}, {1757u, 0x711aa4c95aa4364cull}, {3729u, 0x2a9f7bb48dd84363ull}, {5668u, 0x5bdf968203a3abf8ull}, {6057u, 0x1ff4bb541db2fe89ull}, {7821u, 0x85a26a04f326f076ull}, {8186u, 0x604f6b677792eb75ull}, {8748u, 0x69c53ba16df7100aull}}},
    {"_ZN7WebCore6JSFileD1Ev", &Thunk93, 0, 3, {{12u, 0xf40edb156e66cc8full}, {45u, 0xb6bce4c1cb9470fdull}, {98u, 0x0589906788a595fdull}, {127u, 0xaabed30bba15d418ull}, {136u, 0x64d2eb3eae75b82eull}, {209u, 0x1b352852df7cb0c0ull}, {216u, 0x7c9ad63babf2a9afull}, {218u, 0xed6decfbdbf2e2a3ull}}},
    {"_ZN7WebCore6JSFileD2Ev", &Thunk94, 0, 3, {{12u, 0xf40edb156e66cc8full}, {45u, 0xb6bce4c1cb9470fdull}, {98u, 0x0589906788a595fdull}, {127u, 0xaabed30bba15d418ull}, {136u, 0x64d2eb3eae75b82eull}, {209u, 0x1b352852df7cb0c0ull}, {216u, 0x7c9ad63babf2a9afull}, {218u, 0xed6decfbdbf2e2a3ull}}},
    {"_ZN7WebCore6JSNodeC1ERKS0_", &Thunk95, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore6JSNodeC2ERKS0_", &Thunk96, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore6JSNodeD1Ev", &Thunk97, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore6JSNodeD2Ev", &Thunk98, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore6Widget16removeFromParentEv", &Thunk99, 0, 4, {{3u, 0x8a3e65224472cac5ull}, {5u, 0x7baa98f27fe785b6ull}, {13u, 0xacb0a8c52dfe2ba0ull}, {25u, 0x8133c84f9c581c97ull}, {28u, 0x0e98c5bc5084ef86ull}, {29u, 0x2950400ea195403aull}, {40u, 0x9b1c87cd5f4b826cull}, {49u, 0xfba8fae65588fc69ull}}},
    {"_ZN7WebCore7JSRangeD1Ev", &Thunk100, 0, 3, {{31u, 0x34182712067d64d4ull}, {96u, 0x45064a755b87923eull}, {98u, 0x0589906788a595fdull}, {110u, 0x482690d6ad3cb35bull}, {281u, 0xf061d59a3dd0d586ull}, {312u, 0x9fdea73e42ce922eull}, {319u, 0x38562ed61c53834full}, {345u, 0x0590972a46be56d2ull}}},
    {"_ZN7WebCore7JSRangeD2Ev", &Thunk101, 0, 3, {{31u, 0x34182712067d64d4ull}, {96u, 0x45064a755b87923eull}, {98u, 0x0589906788a595fdull}, {110u, 0x482690d6ad3cb35bull}, {281u, 0xf061d59a3dd0d586ull}, {312u, 0x9fdea73e42ce922eull}, {319u, 0x38562ed61c53834full}, {345u, 0x0590972a46be56d2ull}}},
    {"_ZN7WebCore9JSElementC1ERKS0_", &Thunk102, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore9JSElementC2ERKS0_", &Thunk103, 0, 4, {{6u, 0xc6032fe8067880ecull}, {14u, 0xcfef3fe637b602c5ull}, {24u, 0x8f6b8f9f9357e9e8ull}, {33u, 0x00b75697f3972e2eull}, {34u, 0x83bc832c1aa1faddull}, {72u, 0x305299f85904de10ull}, {82u, 0x7599ca4a83d2fe01ull}, {90u, 0xed2a893b75a7ea1dull}}},
    {"_ZN7WebCore9JSElementD1Ev", &Thunk104, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore9JSElementD2Ev", &Thunk105, 0, 4, {{2u, 0x8ef53d1728a02a64ull}, {3u, 0xeeae548f0e7d0e0full}, {4u, 0xe4c9817480f1682aull}, {5u, 0xf9835eaae49d45a9ull}, {12u, 0xa3cd4d0221114e7eull}, {14u, 0xc08cb0db002b61f7ull}, {16u, 0xb8794397375a6a88ull}, {18u, 0x8788e2d348259a60ull}}},
    {"_ZN7WebCore9Scrollbar13setProportionEii", &Thunk106, 0, 0, {{1u, 0x20141a73b0ce8c9eull}, {3u, 0x90dcbeda3d265c3cull}, {4u, 0xc8d7441df8b203cbull}, {6u, 0x9ebd66728338d616ull}, {7u, 0x5d8ca8dcc15e835full}, {9u, 0x95a7aa0ce027bce0ull}, {11u, 0x55280cb2728ac1feull}, {12u, 0x3fab20ef6f9bb5f1ull}}},
    {"_ZN9Inspector15InspectorTarget17setResumeCallbackEON3WTF8FunctionIFvvEEE", &Thunk107, 0, 0, {{6u, 0x1bc10114153c3f17ull}, {8u, 0xa33cc0c9bc594ca3ull}, {13u, 0x642c271f809134f1ull}, {18u, 0xd34765ca1694d06dull}, {24u, 0x7c3962adfd84510full}, {29u, 0xf258697e73aed04bull}, {33u, 0xa81362e26a69af92ull}, {34u, 0xb55bc815725d25e7ull}}},
    {"_ZN9Inspector21InjectedScriptManager10disconnectEv", &Thunk108, 0, 4, {{1u, 0xdc20abb969ffe1f6ull}, {3u, 0xd0136c2088c8a736ull}, {4u, 0x61be13c2b0473607ull}, {6u, 0x11018eb16e6229e2ull}, {7u, 0xbdd7883393f83aedull}, {9u, 0x73874f56ad4e030dull}, {11u, 0xa6381a51fd81b21full}, {12u, 0x1faba44dc6df90abull}}},
    {"_ZNK7WebCore15ActiveDOMObject18hasPendingActivityEv", &Thunk109, 0, 1, {{22u, 0x4c94db2542ad0b15ull}, {39u, 0x2f19616d001b2286ull}, {87u, 0x04850125d1c71cdeull}, {104u, 0x28b6add37fd7749aull}, {107u, 0x3613fc0b3830c3e5ull}, {123u, 0x7fbe3d9aa41bd4eaull}, {144u, 0xc11fe0da0a6a708aull}, {159u, 0x453bb87cfd8d44d8ull}}},
    {"_ZNK7WebCore16HTMLMediaElement23webkitHasClosedCaptionsEv", &Thunk110, 0, 4, {{1u, 0xf3e2efb9dccc4538ull}, {3u, 0xcb717266fa306654ull}, {4u, 0x5a5911ecfc59a51dull}, {6u, 0x1c3c50d0c1d92690ull}, {7u, 0x12b98ef8bfe5af53ull}, {9u, 0xa8a489dfc770bfdfull}, {11u, 0xb95e6023c8db54edull}, {12u, 0x9544d304c0b7feb5ull}}},
    {"_ZNK7WebCore6Editor7Command11isSupportedEv", &Thunk111, 0, 3, {{1u, 0xadfb9cb09408e682ull}, {3u, 0x319df1a00772a942ull}, {14u, 0x90142eed75798966ull}, {16u, 0x4a3099f36a81c3f0ull}, {31u, 0x9c5e745bb7efd1bbull}, {46u, 0x5ff68354d819471dull}, {51u, 0x4fb16ca7f1396d8dull}, {54u, 0x9360f2f9327b4803ull}}},
    {"_ZNK9Inspector18InjectedScriptBase31hasAccessToInspectedScriptStateEv", &Thunk112, 0, 3, {{1u, 0xe8841a4ef525a03eull}, {3u, 0xb067dde9dd8e1cb9ull}, {5u, 0xf743adf314c00761ull}, {13u, 0xda119d7214c05332ull}, {17u, 0x8f10358c0ce6dec9ull}, {21u, 0x05954298cb8e4737ull}, {25u, 0x58ac57b1d164af6aull}, {27u, 0x7d20a84cb2ab7613ull}}},
    {"_ZNK9Inspector21InspectorConsoleAgent22developerExtrasEnabledEv", &Thunk113, 0, 4, {{2u, 0x4d8e838f5d17a9f3ull}, {4u, 0xeb4915f709bc0ac9ull}, {5u, 0x43dda52a2c7aecbeull}, {27u, 0xc8a88d1b16f28e11ull}, {28u, 0xb922b3eda67db10aull}, {39u, 0x3cadb72891276002ull}, {42u, 0x82df243e78fe1624ull}, {45u, 0xa0203b03feb0aa52ull}}},
    {"_ZNK9Inspector22RemoteInspectionTarget20remoteControlAllowedEv", &Thunk114, 0, 1, {{1u, 0x419e3c89e2a126dcull}, {3u, 0x991c6bc36d5d363bull}, {6u, 0x16df88ebd160a41aull}, {7u, 0x57ee08e63d211ec8ull}, {9u, 0x4d2f2a51c0d65609ull}, {11u, 0x31b93d6040c6d206ull}, {12u, 0x9193910cded139a6ull}, {14u, 0x93c1fb591a312f1eull}}},
};

std::uint64_t RunCase(const Case& c, std::uint32_t seed, unsigned char* arena) {
    const auto trial = PrepareVirtual(seed, arena);
    mockRandom.seed(trial.mockSeed);
    calls.clear();
    const std::uint64_t result = c.thunk(trial.args[0], trial.args[1], trial.args[2], trial.args[3], trial.args[4], trial.args[5], FromBits(trial.mockSeed));
    Require(!calls.empty() && calls.size() <= MaxCalls);
    LiftedTrial::Digest digest{};
    digest.base = arenaBase;
    digest.Word(calls.size());
    for (const auto& call : calls) {
        digest.Word(call.id);
        for (const auto arg : call.args) digest.Word(arg);
        for (int k = 0; k < c.xmm; ++k) digest.Word(call.x[k]);
    }
    for (std::size_t i = 0; i < LiftedTrial::ArenaSize; i += 8) {
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
    unsigned char* arena = MapFixed("trial arena", LiftedTrial::ArenaAddress, LiftedTrial::ArenaSize);
    arenaBase = reinterpret_cast<std::uint64_t>(arena);
    InstallStubs();
    for (const Case& c : Cases) {
        for (const Seed& s : c.seeds) {
            if (RunCase(c, s.seed, arena) != s.digest) {
                std::fprintf(stderr, "%s seed %u digest mismatch\n", c.name, s.seed);
                Require(false);
            }
        }
    }
}
#endif

static void TestNullObjects() {
    for (const Thunk thunk : {&Thunk0, &Thunk1, &Thunk2, &Thunk3, &Thunk8, &Thunk9, &Thunk15, &Thunk16, &Thunk17, &Thunk18, &Thunk19, &Thunk22, &Thunk26, &Thunk27, &Thunk29, &Thunk34, &Thunk35, &Thunk38, &Thunk39, &Thunk42, &Thunk43, &Thunk44, &Thunk47, &Thunk48, &Thunk51, &Thunk52, &Thunk53, &Thunk54, &Thunk56, &Thunk57, &Thunk60, &Thunk61, &Thunk62, &Thunk65, &Thunk66, &Thunk67, &Thunk68, &Thunk71, &Thunk72, &Thunk75, &Thunk76, &Thunk77, &Thunk78, &Thunk81, &Thunk82, &Thunk85, &Thunk86, &Thunk87, &Thunk88, &Thunk89, &Thunk93, &Thunk94, &Thunk97, &Thunk98, &Thunk100, &Thunk101, &Thunk104, &Thunk105, &Thunk106, &Thunk108, &Thunk109, &Thunk110, &Thunk113, &Thunk114}) {
        bool threw = false;
        try {
            thunk(0, 0, 0, 0, 0, 0, 0.0);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
}

int main() {
#ifdef LIFTED_RECORDED_RUN
    TestRecordedBehaviour();
#endif
    TestNullObjects();
    return 0;
}
