#include "prx/libSceNKWebKit/include/LiftedBranchyWrappersExports.hpp"

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
        std::fprintf(stderr, "LiftedBranchyWrappers check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Wrapper = std::uint64_t (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

static std::uint64_t APS5_VABI Thunk0(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKAccessibilityRootObject(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk1(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC12StackVisitor5Frame8sourceIDEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC17DebuggerCallFrame20sourceIDForCallFrameEPNS_9CallFrameE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk3(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC9Structure16isValidPrototypeENS_7JSValueE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk4(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN3WTF10StringView16GraphemeClusters8IteratorD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk5(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN3WTF10StringView16GraphemeClusters8IteratorD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk6(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore11MemoryCache14evictResourcesEv(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk7(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore12EventHandler30setCapturingMouseEventsElementEPNS_7ElementE(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk8(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore12JSTimeRangesD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk9(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore12JSTimeRangesD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk10(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore12JSTimeRangesaSERKS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk11(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore13JSDOMRectListD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk12(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore13JSDOMRectListD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk13(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore13JSDOMRectListaSERKS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk14(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore13displayStringERKN3WTF6StringEPKNS_4NodeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk15(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore20HTMLFrameElementBase5widthEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk16(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore20HTMLFrameElementBase6heightEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk17(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore22makeRangeSelectingNodeERNS_4NodeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk18(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23ApplicationCacheStorageC1ERKN3WTF6StringES4_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk19(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23ApplicationCacheStorageC2ERKN3WTF6StringES4_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk20(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23MediaQueryParserContextC1ERKNS_8DocumentE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk21(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore23MediaQueryParserContextC2ERKNS_8DocumentE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk22(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore26makeBoundaryPointAfterNodeERNS_4NodeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk23(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore27makeBoundaryPointBeforeNodeERNS_4NodeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk24(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore30makeRangeSelectingNodeContentsERNS_4NodeE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk25(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4Node11appendChildERS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk26(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4Node11removeChildERS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk27(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4Node6removeEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk28(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_9DOMWindowE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk29(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore8JSPath2DD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk30(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN7WebCore8JSPath2DD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk31(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8JSPath2DaSERKS0_(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk32(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7bmalloc13IsoSharedPage9tryCreateEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk33(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN9Inspector22InspectorDebuggerAgent19clearExceptionValueEv(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk34(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk35(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD1Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk36(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    _ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD2Ev(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk37(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore12NamedNodeMap6lengthEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk38(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore13HitTestResult11textContentEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk39(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore13HitTestResult15absoluteLinkURLEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk40(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore14DocumentLoader21archiveResourceForURLERKN3WTF3URLE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk41(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore14FrameSelection18isFocusedAndActiveEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk42(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore14FrameSelection36rootEditableElementOrDocumentElementEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk43(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore16DOMGuardedObject11isSuspendedEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk44(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore20ScrollingCoordinator32coordinatesScrollingForFrameViewERKNS_9FrameViewE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk45(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore27ScriptedAnimationController17throttlingReasonsEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk46(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore4Page27pageExtendedBackgroundColorEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk47(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore5Frame31displayStringModifiedByEncodingERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk48(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore6Editor6clientEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk49(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore6Editor9canDeleteEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk50(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore6Quirks70shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk51(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore8Document6domainEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk52(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(a0, a1, a2, a3, a4, a5);
}

#if defined(__linux__)
#include <dlfcn.h>
#include <sys/mman.h>


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

}  // namespace LiftedTrial


namespace {

using LiftedTrial::ArenaSize;
using LiftedTrial::ArenaAddress;

struct CallRecord {
    std::uint32_t id;
    std::uint64_t args[6];
};

bool armed = false;
LiftedTrial::Random mockRandom{0};
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
    const std::uint64_t value = LiftedTrial::MockReturn(mockRandom, arenaBase);
    mockRandom.Next();
    return value;
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC14SourceProvider5getIDEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(0, "_ZN3JSC14SourceProvider5getIDEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3JSC6JSCell9getObjectEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(1, "_ZN3JSC6JSCell9getObjectEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF31NonSharedCharacterBreakIteratorD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(2, "_ZN3WTF31NonSharedCharacterBreakIteratorD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF3URL10invalidateEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(3, "_ZN3WTF3URL10invalidateEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF5equalEPKNS_10StringImplES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(4, "_ZN3WTF5equalEPKNS_10StringImplES2_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF8fastFreeEPv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(5, "_ZN3WTF8fastFreeEPv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore10RenderView10compositorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(6, "_ZN7WebCore10RenderView10compositorEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11DOMRectListD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(7, "_ZN7WebCore11DOMRectListD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11MemoryCache11setDisabledEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(8, "_ZN7WebCore11MemoryCache11setDisabledEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore13AXObjectCache10rootObjectEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(9, "_ZN7WebCore13AXObjectCache10rootObjectEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore13AXObjectCache19enableAccessibilityEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(10, "_ZN7WebCore13AXObjectCache19enableAccessibilityEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore13ContainerNode11appendChildERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(11, "_ZN7WebCore13ContainerNode11appendChildERNS_4NodeE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore13ContainerNode11removeChildERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(12, "_ZN7WebCore13ContainerNode11removeChildERNS_4NodeE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore14SQLiteDatabaseC1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(13, "_ZN7WebCore14SQLiteDatabaseC1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore25ArchiveResourceCollection21archiveResourceForURLERKN3WTF3URLE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(14, "_ZN7WebCore25ArchiveResourceCollection21archiveResourceForURLERKN3WTF3URLE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore4Node14removedLastRefEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(15, "_ZN7WebCore4Node14removedLastRefEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_11WindowProxyE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(16, "_ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_11WindowProxyE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore5ColorC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(17, "_ZN7WebCore5ColorC1ERKS0_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore6Path2DD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(18, "_ZN7WebCore6Path2DD1Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(19, "_ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7bmalloc11IsoPageBase18allocatePageMemoryEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(20, "_ZN7bmalloc11IsoPageBase18allocatePageMemoryEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector17BackendDispatcherD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(21, "_ZN9Inspector17BackendDispatcherD2Ev", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector21InjectedScriptManager19clearExceptionValueEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(22, "_ZN9Inspector21InjectedScriptManager19clearExceptionValueEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK3WTF3URL4hostEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(23, "_ZNK3WTF3URL4hostEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore10RenderView15usesCompositingEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(24, "_ZNK7WebCore10RenderView15usesCompositingEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore13ContainerNode15countChildNodesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(25, "_ZNK7WebCore13ContainerNode15countChildNodesEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore15SecurityContext14securityOriginEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(26, "_ZNK7WebCore15SecurityContext14securityOriginEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore16VisibleSelection19rootEditableElementEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(27, "_ZNK7WebCore16VisibleSelection19rootEditableElementEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore17ActiveDOMCallback17canInvokeCallbackEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(28, "_ZNK7WebCore17ActiveDOMCallback17canInvokeCallbackEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore4Node11textContentEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(29, "_ZNK7WebCore4Node11textContentEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore4Node16computeNodeIndexEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(30, "_ZNK7WebCore4Node16computeNodeIndexEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore4Node9renderBoxEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(31, "_ZNK7WebCore4Node9renderBoxEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore5Frame15contentRendererEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(32, "_ZNK7WebCore5Frame15contentRendererEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore7Element13hasAttributesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(33, "_ZNK7WebCore7Element13hasAttributesEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore7Element15absoluteLinkURLEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(34, "_ZNK7WebCore7Element15absoluteLinkURLEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore8Document11topDocumentEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(35, "_ZNK7WebCore8Document11topDocumentEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore8Document13axObjectCacheEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(36, "_ZNK7WebCore8Document13axObjectCacheEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore8Document31displayStringModifiedByEncodingERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(37, "_ZNK7WebCore8Document31displayStringModifiedByEncodingERKN3WTF6StringE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore8Document4pageEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(38, "_ZNK7WebCore8Document4pageEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore8Settings23needsSiteSpecificQuirksEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(39, "_ZNK7WebCore8Settings23needsSiteSpecificQuirksEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore9DOMWindow5frameEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(40, "_ZNK7WebCore9DOMWindow5frameEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore9FrameView10renderViewEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(41, "_ZNK7WebCore9FrameView10renderViewEv", a0, a1, a2, a3, a4, a5);
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
    {"WKAccessibilityRootObject", &Thunk0, 3, {{0u, 0x088520008fbbc16eull}, {7u, 0x00254f3f565d0c41ull}, {12u, 0x2072a7d027de6df2ull}, {14u, 0xa61d25810d458bb2ull}, {20u, 0x039be6d4c46fbd87ull}, {2u, 0x12189fd8ee645e78ull}, {8u, 0x8934f6a7d07d0f3eull}, {18u, 0xa7df720d2a18c4a8ull}}},
    {"_ZN3JSC12StackVisitor5Frame8sourceIDEv", &Thunk1, 3, {{2u, 0x232d1b04fc4fd047ull}, {0u, 0xdd6a17a5b222ac0eull}, {4u, 0xb06ccacd369bcb7cull}, {50u, 0x42c64283b2250e53ull}, {8u, 0x8ac0ae44fc5bfdc3ull}, {11u, 0x90d51689468d8402ull}, {16u, 0xce60b6b39def2b8eull}, {22u, 0xbb614254850c90edull}}},
    {"_ZN3JSC17DebuggerCallFrame20sourceIDForCallFrameEPNS_9CallFrameE", &Thunk2, 3, {{6u, 0xf76a474ac91f6c74ull}, {9u, 0xfe3f698cb2b89130ull}, {20u, 0x039be6d4c46fbd87ull}, {39u, 0xe38dd69bed1dc838ull}, {62u, 0xd02d523934adb83aull}, {8u, 0x0b209705157653cdull}, {10u, 0x00c22ed06414e0cdull}, {13u, 0x89200c1b24cb432aull}}},
    {"_ZN3JSC9Structure16isValidPrototypeENS_7JSValueE", &Thunk3, 1, {{0u, 0x27e234f22d051121ull}, {2u, 0x3380aa744d5b2e37ull}, {120u, 0x0d2d8ef0b99e2a4eull}, {1063u, 0x05c30b4412f43921ull}, {1u, 0xba110d984526629bull}, {3u, 0x75e6d806121fb0f5ull}, {4u, 0x570978382b1282b6ull}, {6u, 0x89d0ce53cf40e094ull}}},
    {"_ZN3WTF10StringView16GraphemeClusters8IteratorD1Ev", &Thunk4, 0, {{1u, 0xb7b8bd5164ede2feull}, {3u, 0x8f1a3222f1446391ull}, {9u, 0xfe3f698cb2b89130ull}, {4u, 0x6c5d26247bb6d013ull}, {5u, 0x4c866b095751062bull}, {7u, 0x71f3e5ee275501e5ull}, {8u, 0x690f74291c67e983ull}, {10u, 0x2a2760a66f405e42ull}}},
    {"_ZN3WTF10StringView16GraphemeClusters8IteratorD2Ev", &Thunk5, 0, {{1u, 0xb7b8bd5164ede2feull}, {3u, 0x8f1a3222f1446391ull}, {9u, 0xfe3f698cb2b89130ull}, {4u, 0x6c5d26247bb6d013ull}, {5u, 0x4c866b095751062bull}, {7u, 0x71f3e5ee275501e5ull}, {8u, 0x690f74291c67e983ull}, {10u, 0x2a2760a66f405e42ull}}},
    {"_ZN7WebCore11MemoryCache14evictResourcesEv", &Thunk6, 0, {{0u, 0xfc64deaebd11f62full}, {9u, 0xf97985c22db22447ull}, {1u, 0xd4cd2bbca2444744ull}, {2u, 0x3380aa744d5b2e37ull}, {3u, 0x42a5807cdc6022b7ull}, {4u, 0xb06ccacd369bcb7cull}, {5u, 0xc2030ad29a390fadull}, {6u, 0x89d0ce53cf40e094ull}}},
    {"_ZN7WebCore12EventHandler30setCapturingMouseEventsElementEPNS_7ElementE", &Thunk7, 0, {{0u, 0x897e0b5490a3b3e4ull}, {5u, 0xd3cac0ed47b93549ull}, {1u, 0x62f0cdb1b1bdaab9ull}, {8u, 0x8bee9c4f927c55ebull}, {168u, 0xe5d2da5d5ad49106ull}, {6370u, 0x536698ae627f5dbdull}, {3u, 0x6086cbd0067469f2ull}, {6u, 0x0c07c7e7d485a49full}}},
    {"_ZN7WebCore12JSTimeRangesD1Ev", &Thunk8, 0, {{221u, 0x5e7e1715c0e92f7aull}, {0u, 0xfc64deaebd11f62full}, {1u, 0xc8ac59062caec7cfull}, {646u, 0xd2ba6e649048d9a5ull}, {2u, 0x85dc7bc35b9474bdull}, {4u, 0xbe5b631a9caa0e4cull}, {5u, 0xb9e0d69cc60a7cbbull}, {6u, 0xbc83874aab9b5206ull}}},
    {"_ZN7WebCore12JSTimeRangesD2Ev", &Thunk9, 0, {{221u, 0x5e7e1715c0e92f7aull}, {0u, 0xfc64deaebd11f62full}, {1u, 0xc8ac59062caec7cfull}, {646u, 0xd2ba6e649048d9a5ull}, {2u, 0x85dc7bc35b9474bdull}, {4u, 0xbe5b631a9caa0e4cull}, {5u, 0xb9e0d69cc60a7cbbull}, {6u, 0xbc83874aab9b5206ull}}},
    {"_ZN7WebCore12JSTimeRangesaSERKS0_", &Thunk10, 4, {{238u, 0x0021310128a2da87ull}, {0u, 0x0af19622181f5986ull}, {1u, 0x837411a5831fc67eull}, {646u, 0xb0edacd541aa0cbdull}, {2u, 0x9948684268498c8dull}, {4u, 0xf0ea3d23f14e23ccull}, {6u, 0x5d11413d53028bc0ull}, {7u, 0x796343e4313b6eccull}}},
    {"_ZN7WebCore13JSDOMRectListD1Ev", &Thunk11, 0, {{1u, 0xc8ac59062caec7cfull}, {0u, 0xfc64deaebd11f62full}, {221u, 0xfd198a34e9ad76adull}, {2u, 0x85dc7bc35b9474bdull}, {4u, 0xbe5b631a9caa0e4cull}, {5u, 0xb9e0d69cc60a7cbbull}, {6u, 0xbc83874aab9b5206ull}, {7u, 0x00254f3f565d0c41ull}}},
    {"_ZN7WebCore13JSDOMRectListD2Ev", &Thunk12, 0, {{1u, 0xc8ac59062caec7cfull}, {0u, 0xfc64deaebd11f62full}, {221u, 0xfd198a34e9ad76adull}, {2u, 0x85dc7bc35b9474bdull}, {4u, 0xbe5b631a9caa0e4cull}, {5u, 0xb9e0d69cc60a7cbbull}, {6u, 0xbc83874aab9b5206ull}, {7u, 0x00254f3f565d0c41ull}}},
    {"_ZN7WebCore13JSDOMRectListaSERKS0_", &Thunk13, 4, {{1u, 0x837411a5831fc67eull}, {0u, 0x0af19622181f5986ull}, {238u, 0x9128d2ba3fdc30a9ull}, {2u, 0x9948684268498c8dull}, {4u, 0xf0ea3d23f14e23ccull}, {6u, 0x5d11413d53028bc0ull}, {7u, 0x796343e4313b6eccull}, {8u, 0xfeaa39c831f284acull}}},
    {"_ZN7WebCore13displayStringERKN3WTF6StringEPKNS_4NodeE", &Thunk14, 4, {{36u, 0x7fa5b61056dce15aull}, {2u, 0xef81425c80d774a6ull}, {150u, 0x64527ea64fdcfceeull}, {3u, 0xee6b5dce3dbba86eull}, {4u, 0xe736bac81edae5edull}, {5u, 0x962614d3559e6367ull}, {6u, 0xa62bba0d4da08093ull}, {7u, 0x90e413ecca26f426ull}}},
    {"_ZN7WebCore20HTMLFrameElementBase5widthEv", &Thunk15, 3, {{0u, 0x7c02eba2c5987d94ull}, {2u, 0xe870b55f1a345566ull}, {9u, 0x54e9b1d2c60e37bcull}, {13u, 0xcf3780ad2563d251ull}, {16u, 0x32ade450bee1f652ull}, {18u, 0x002e4f26daeec444ull}, {21u, 0x9aabf2365f7cf82aull}, {23u, 0x6f5b28397fd8b59full}}},
    {"_ZN7WebCore20HTMLFrameElementBase6heightEv", &Thunk16, 3, {{0u, 0x73d12ecb3f6a7c1cull}, {2u, 0xe870b55f1a345566ull}, {9u, 0x7f374f2f9424f7d5ull}, {13u, 0xcf3780ad2563d251ull}, {16u, 0x47a099a0364eaa99ull}, {18u, 0x9e264f3436a3ffa3ull}, {21u, 0x9aabf2365f7cf82aull}, {23u, 0xfbaa79dbc33db874ull}}},
    {"_ZN7WebCore22makeRangeSelectingNodeERNS_4NodeE", &Thunk17, 4, {{0u, 0x0813cc38adbf487full}, {19u, 0xee34a866cfc869dcull}, {4u, 0x1dee3822bc429a03ull}, {6u, 0xdfd94740524f5426ull}, {7u, 0x773a2b372a4737feull}, {8u, 0x3266d35e00eba939ull}, {10u, 0x29be1054b89e5f97ull}, {12u, 0xf2d616afdc31e612ull}}},
    {"_ZN7WebCore23ApplicationCacheStorageC1ERKN3WTF6StringES4_", &Thunk18, 4, {{0u, 0xde0556f818fae734ull}, {6u, 0xe406db9500261242ull}, {1u, 0x7ef0abcdc4924d8aull}, {66u, 0x402f95f87dc61422ull}, {2u, 0xdafe7b0624c352daull}, {4u, 0x6f28e91040a9c268ull}, {7u, 0x0707975113cab578ull}, {10u, 0x95448ea7642bfabcull}}},
    {"_ZN7WebCore23ApplicationCacheStorageC2ERKN3WTF6StringES4_", &Thunk19, 4, {{0u, 0xde0556f818fae734ull}, {6u, 0xe406db9500261242ull}, {1u, 0x7ef0abcdc4924d8aull}, {66u, 0x402f95f87dc61422ull}, {2u, 0xdafe7b0624c352daull}, {4u, 0x6f28e91040a9c268ull}, {7u, 0x0707975113cab578ull}, {10u, 0x95448ea7642bfabcull}}},
    {"_ZN7WebCore23MediaQueryParserContextC1ERKNS_8DocumentE", &Thunk20, 3, {{0u, 0x9efdff29e374752bull}, {5u, 0x7fa90dba01f403aeull}, {3u, 0x7744ce6412c7bcd6ull}, {11u, 0x8acf643dd4de0112ull}, {12u, 0x6a322adaeea376f7ull}, {13u, 0x0005ad682f8bb666ull}, {14u, 0xf5617e3479a1a888ull}, {15u, 0x0f2ae955633c45a0ull}}},
    {"_ZN7WebCore23MediaQueryParserContextC2ERKNS_8DocumentE", &Thunk21, 3, {{0u, 0x9efdff29e374752bull}, {5u, 0x7fa90dba01f403aeull}, {3u, 0x7744ce6412c7bcd6ull}, {11u, 0x8acf643dd4de0112ull}, {12u, 0x6a322adaeea376f7ull}, {13u, 0x0005ad682f8bb666ull}, {14u, 0xf5617e3479a1a888ull}, {15u, 0x0f2ae955633c45a0ull}}},
    {"_ZN7WebCore26makeBoundaryPointAfterNodeERNS_4NodeE", &Thunk22, 4, {{0u, 0xb4cb835edf08d5f3ull}, {19u, 0xb8150d93e9231f06ull}, {4u, 0xf2e9ca581ef368b6ull}, {6u, 0x86ea33067f067754ull}, {7u, 0xc670d5a52eab0b62ull}, {8u, 0xb1546b7bed999afaull}, {10u, 0xbd717d28401ed16bull}, {12u, 0xb013bd7a7aa66529ull}}},
    {"_ZN7WebCore27makeBoundaryPointBeforeNodeERNS_4NodeE", &Thunk23, 4, {{0u, 0x11ba6b289a794586ull}, {19u, 0xb8150d93e9231f06ull}, {4u, 0x98b037afbdb7c87full}, {6u, 0x6c3691d30a0ab505ull}, {7u, 0x60e691ac15310c3bull}, {8u, 0x78d5a907bf60acd3ull}, {10u, 0x3f7f7a3d259763aaull}, {12u, 0x603c478e14340d30ull}}},
    {"_ZN7WebCore30makeRangeSelectingNodeContentsERNS_4NodeE", &Thunk24, 4, {{0u, 0xe09745dc95573506ull}, {9u, 0x56574af316dfcb0dull}, {48u, 0xff64488281b4f175ull}, {243u, 0x93806d50ab642c93ull}, {1u, 0xa8da74c8121cb119ull}, {2u, 0xf2198612790baad7ull}, {3u, 0xdb3c9785d633602dull}, {4u, 0xefaca432d43d0a2cull}}},
    {"_ZN7WebCore4Node11appendChildERS0_", &Thunk25, 4, {{0u, 0xebd790272e5473d7ull}, {9u, 0x799299e3cb8d9029ull}, {1u, 0x6149d306aab88bd2ull}, {2u, 0xdf0f956ffc273d9dull}, {3u, 0xc1b0d8bf5d350808ull}, {4u, 0x5bf844eaac76cfd5ull}, {6u, 0x10d101ed32ab01cfull}, {7u, 0x6f01e300e9b3b87dull}}},
    {"_ZN7WebCore4Node11removeChildERS0_", &Thunk26, 4, {{0u, 0xf8deab3a013aeac3ull}, {9u, 0x77f6a87ff889042aull}, {1u, 0xa19ec6eae75c85c6ull}, {2u, 0x35e25a33ac0894d9ull}, {3u, 0x18b7bdfdda5b2ca4ull}, {4u, 0xa672fe6a55e39439ull}, {6u, 0x6261456fb12a6f1bull}, {7u, 0x1c0d771c223c3621ull}}},
    {"_ZN7WebCore4Node6removeEv", &Thunk27, 4, {{0u, 0x9fd7ca5619a661e9ull}, {19u, 0xe9455ec4a934b534ull}, {1u, 0x778b9c14995db547ull}, {2u, 0xda7a5fb2021bfa8cull}, {3u, 0xdde81bcb5ad9218aull}, {4u, 0xa253bdc403eef0ebull}, {6u, 0xd0879ad9971292a8ull}, {7u, 0xb40d064482d9ada6ull}}},
    {"_ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_9DOMWindowE", &Thunk28, 3, {{0u, 0x80419d0456973dbcull}, {5u, 0x89de5a10632ecac3ull}, {1u, 0xdeed8b7011de73d7ull}, {2u, 0xc600fdd4f4a20cccull}, {3u, 0x4d95c075d995e493ull}, {4u, 0xd92b99fddfc41514ull}, {7u, 0x436315beb615842bull}, {8u, 0xd598da6b0e06ed14ull}}},
    {"_ZN7WebCore8JSPath2DD1Ev", &Thunk29, 0, {{1u, 0x619793365e9fdaa7ull}, {0u, 0xfc64deaebd11f62full}, {104u, 0xfd2770dbaabaad16ull}, {2u, 0xee10c59f84212ef5ull}, {4u, 0xbb8b5e3b131d6fc4ull}, {5u, 0x83246fc0b9f6ad54ull}, {6u, 0xd9206ffa28535194ull}, {7u, 0x00254f3f565d0c41ull}}},
    {"_ZN7WebCore8JSPath2DD2Ev", &Thunk30, 0, {{1u, 0x619793365e9fdaa7ull}, {0u, 0xfc64deaebd11f62full}, {104u, 0xfd2770dbaabaad16ull}, {2u, 0xee10c59f84212ef5ull}, {4u, 0xbb8b5e3b131d6fc4ull}, {5u, 0x83246fc0b9f6ad54ull}, {6u, 0xd9206ffa28535194ull}, {7u, 0x00254f3f565d0c41ull}}},
    {"_ZN7WebCore8JSPath2DaSERKS0_", &Thunk31, 4, {{1u, 0x8acd9665d82b2486ull}, {0u, 0xc21f21db66c0c1aeull}, {83u, 0xca1716a7fe0878cfull}, {2u, 0x5d845ddcbf46ef99ull}, {4u, 0xb769989b6f062078ull}, {6u, 0xda92a0178f40852aull}, {7u, 0x45d4f6355daf8f3cull}, {8u, 0x726b8891656e809cull}}},
    {"_ZN7bmalloc13IsoSharedPage9tryCreateEv", &Thunk32, 3, {{0u, 0x9979b01e12b8c8e0ull}, {5u, 0x5ac129b1f11ba19dull}, {1u, 0x6f2a3620edc5358bull}, {2u, 0xb2ff0ad1206342f2ull}, {3u, 0x71381b0a95491b53ull}, {4u, 0x788d15ac0cda05ffull}, {7u, 0x0691409efde64179ull}, {8u, 0x36e9b77189282d9cull}}},
    {"_ZN9Inspector22InspectorDebuggerAgent19clearExceptionValueEv", &Thunk33, 0, {{0u, 0x56417b39173346ddull}, {3u, 0x42a5807cdc6022b7ull}, {1u, 0x96c717986b3e4243ull}, {2u, 0xd2e0459c80d137c0ull}, {4u, 0xb06ccacd369bcb7cull}, {5u, 0xa2de2e4c0a6bf491ull}, {6u, 0x28813bf127c21a6full}, {7u, 0xec6b1dcae8279f99ull}}},
    {"_ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackD1Ev", &Thunk34, 0, {{2u, 0xa4438c5707deefb2ull}, {3u, 0x42a5807cdc6022b7ull}, {134u, 0xbf08f681a2d1ea43ull}, {4u, 0x4c02bfc33550a585ull}, {5u, 0x5182fe160aba5274ull}, {6u, 0x89d0ce53cf40e094ull}, {7u, 0x00254f3f565d0c41ull}, {8u, 0xac88dfac7d955e75ull}}},
    {"_ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD1Ev", &Thunk35, 0, {{2u, 0xa4438c5707deefb2ull}, {3u, 0x42a5807cdc6022b7ull}, {134u, 0xbf08f681a2d1ea43ull}, {4u, 0x4c02bfc33550a585ull}, {5u, 0x5182fe160aba5274ull}, {6u, 0x89d0ce53cf40e094ull}, {7u, 0x00254f3f565d0c41ull}, {8u, 0xac88dfac7d955e75ull}}},
    {"_ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD2Ev", &Thunk36, 0, {{2u, 0xa4438c5707deefb2ull}, {3u, 0x42a5807cdc6022b7ull}, {134u, 0xbf08f681a2d1ea43ull}, {4u, 0x4c02bfc33550a585ull}, {5u, 0x5182fe160aba5274ull}, {6u, 0x89d0ce53cf40e094ull}, {7u, 0x00254f3f565d0c41ull}, {8u, 0xac88dfac7d955e75ull}}},
    {"_ZNK7WebCore12NamedNodeMap6lengthEv", &Thunk37, 3, {{2u, 0x164d90cfe6240d55ull}, {5u, 0x73fe5fe76f943170ull}, {84u, 0xdf14d430e026a284ull}, {4u, 0xeb7abae59fe3d5bdull}, {8u, 0x4a6b220c8253e10aull}, {13u, 0x69e957ae37d559e1ull}, {14u, 0xd168e66bfd7595a8ull}, {16u, 0xe00e501cfca01ed6ull}}},
    {"_ZNK7WebCore13HitTestResult11textContentEv", &Thunk38, 4, {{0u, 0x79e29809019df61cull}, {42u, 0xab30713d61141828ull}, {1u, 0x436da5d483a4dd69ull}, {2u, 0x9cc97327bb8d58b8ull}, {3u, 0x2f95d30f956a4614ull}, {4u, 0xfb40bf89ec4d6065ull}, {6u, 0xce09d173fd7a3717ull}, {7u, 0x40810abca9e3e21aull}}},
    {"_ZNK7WebCore13HitTestResult15absoluteLinkURLEv", &Thunk39, 4, {{0u, 0x40e573a297e9f66full}, {42u, 0x93381af5496c20c7ull}, {1u, 0xcd10832794b6b9feull}, {2u, 0xcdf4d69bcf8d8246ull}, {3u, 0x00f767c69e80ca81ull}, {4u, 0x7ef90445a34ad731ull}, {6u, 0x7aa2a32969b113a9ull}, {7u, 0x2f145e904ac0d784ull}}},
    {"_ZNK7WebCore14DocumentLoader21archiveResourceForURLERKN3WTF3URLE", &Thunk40, 3, {{0u, 0x005ed585f2ceaeb4ull}, {5u, 0xebfabf13cfa84caaull}, {9u, 0xfe3f698cb2b89130ull}, {1u, 0x437d85dbb63a0855ull}, {2u, 0x7128e23919085144ull}, {3u, 0xd66b9e6bddee365aull}, {4u, 0x78a23d1cf1a65576ull}, {7u, 0x4699f61bab235895ull}}},
    {"_ZNK7WebCore14FrameSelection18isFocusedAndActiveEv", &Thunk41, 3, {{5u, 0xaf94d85fde866867ull}, {0u, 0xfc64deaebd11f62full}, {93u, 0xea436a3aed2db7caull}, {1u, 0xd4cd2bbca2444744ull}, {2u, 0x3380aa744d5b2e37ull}, {3u, 0x42a5807cdc6022b7ull}, {4u, 0xb06ccacd369bcb7cull}, {6u, 0x89d0ce53cf40e094ull}}},
    {"_ZNK7WebCore14FrameSelection36rootEditableElementOrDocumentElementEv", &Thunk42, 4, {{0u, 0xc090ee8705369cadull}, {5u, 0xaac6ff6ccca632f5ull}, {1u, 0x53b7738150551b98ull}, {2u, 0x2e4663917c27fd07ull}, {3u, 0xf2051a96bba4928eull}, {4u, 0xb03145d16f9ea0f6ull}, {6u, 0x255b403cd380d65cull}, {7u, 0xb322b4ff2dcda098ull}}},
    {"_ZNK7WebCore16DOMGuardedObject11isSuspendedEv", &Thunk43, 1, {{1u, 0xebbb6e90e0436db8ull}, {0u, 0xdd6a17a5b222ac0eull}, {5u, 0xa30843c98f49c58cull}, {6u, 0xa8cb955cda302ab5ull}, {2u, 0x254c2629b838c680ull}, {4u, 0x7dcd9f89d6e8fbacull}, {7u, 0xe12a88364b6dc220ull}, {8u, 0x3ccf1b8722a11e6aull}}},
    {"_ZNK7WebCore20ScrollingCoordinator32coordinatesScrollingForFrameViewERKNS_9FrameViewE", &Thunk44, 3, {{2u, 0x400db5dc841e3853ull}, {179u, 0xf979283bb701d37cull}, {0u, 0xfc64deaebd11f62full}, {13u, 0xd9f696d441ecd0a5ull}, {896u, 0xead85bba30b74876ull}, {8u, 0x83f81543ceaea696ull}, {10u, 0xf7408f05f1946b3aull}, {14u, 0x8e05bc25a85bcce7ull}}},
    {"_ZNK7WebCore27ScriptedAnimationController17throttlingReasonsEv", &Thunk45, 1, {{2u, 0x5fd12f7f24f81069ull}, {0u, 0x2535c54507ae6107ull}, {1u, 0xabfc452657a7dc6cull}, {5u, 0x7e9a2a11eb644e97ull}, {4u, 0x8a752a06f083e73dull}, {7u, 0x00254f3f565d0c41ull}, {8u, 0x7d988db9c51cdad9ull}, {13u, 0x0dd09aec05e893b6ull}}},
    {"_ZNK7WebCore4Page27pageExtendedBackgroundColorEv", &Thunk46, 4, {{0u, 0x0217aec50c0e62a0ull}, {13u, 0x6fbd8596217b656eull}, {17u, 0x1d0596661eb0bd1cull}, {1u, 0xa73ddb2b71fe76caull}, {2u, 0xe704dd20a0834bc6ull}, {3u, 0xc151484381d5b807ull}, {7u, 0x0d8ae7442788fe99ull}, {8u, 0xb0bf30df91e7eb2full}}},
    {"_ZNK7WebCore5Frame31displayStringModifiedByEncodingERKN3WTF6StringE", &Thunk47, 4, {{7u, 0x17e536c9bdcc0e56ull}, {0u, 0x9e7939a1251c8adcull}, {9u, 0x80bb1798cb4b061full}, {1u, 0xc41ffee9f3c46fe5ull}, {2u, 0xa1ea6ef850245c88ull}, {3u, 0x30f69c2d0db08243ull}, {4u, 0xa62ddcfcd03841ffull}, {6u, 0x1da3343228f88c6full}}},
    {"_ZNK7WebCore6Editor6clientEv", &Thunk48, 3, {{0u, 0xe34f9ed996629c36ull}, {5u, 0xaf94d85fde866867ull}, {1u, 0x7589b47029f8120aull}, {2u, 0x65404e161aeb96e3ull}, {3u, 0xf0d3a049d1663be6ull}, {4u, 0xda5371daecbb9835ull}, {7u, 0x7aba4c4420e1c541ull}, {8u, 0xe69b9121530d5b11ull}}},
    {"_ZNK7WebCore6Editor9canDeleteEv", &Thunk49, 3, {{4u, 0xb06ccacd369bcb7cull}, {339u, 0x6c9662a8a19c1e76ull}, {7u, 0x00254f3f565d0c41ull}, {8u, 0x8ac0ae44fc5bfdc3ull}, {13u, 0x2830b6447105db27ull}, {14u, 0x8762fc073e182856ull}, {15u, 0x6b6676d1f2641aa8ull}, {16u, 0x45d6fce8556edfbeull}}},
    {"_ZNK7WebCore6Quirks70shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasEv", &Thunk50, 3, {{1u, 0x099ff951fa6646b2ull}, {8u, 0x8ac0ae44fc5bfdc3ull}, {9u, 0xfe3f698cb2b89130ull}, {13u, 0x8ac09d21d52b727eull}, {4u, 0x4d90ca61446d7b2cull}, {10u, 0xbe76872850205953ull}, {11u, 0x4b864c46fdf4be90ull}, {12u, 0x2072a7d027de6df2ull}}},
    {"_ZNK7WebCore8Document6domainEv", &Thunk51, 4, {{0u, 0xa33335c20545a4baull}, {12u, 0x1885ab554213dd47ull}, {1u, 0xd322aebcf0ede39cull}, {8u, 0x45526fb50f9913cbull}, {11u, 0x5e010ddd5f7b7ac0ull}, {15u, 0x30fbf12f5fa33168ull}, {16u, 0xdf40d0e813d9fff7ull}, {22u, 0x819d67609a544095ull}}},
    {"_ZNK9Inspector15ScriptCallFrame7isEqualERKS0_", &Thunk52, 3, {{0u, 0x4262ead8cefd4f12ull}, {1u, 0xae329518a1af2626ull}, {3u, 0x94157507fda8d70aull}, {13u, 0x55ba86421610c0e9ull}, {2u, 0x137794db8bdcadb5ull}, {4u, 0x6cb85100a413e6d6ull}, {6u, 0x7d691648ce835e66ull}, {7u, 0xa61dee08dd95efb5ull}}},
};

std::uint64_t RunCase(const Case& c, std::uint32_t seed, unsigned char* arena) {
    arenaBase = reinterpret_cast<std::uint64_t>(arena);
    const LiftedTrial::Trial trial = LiftedTrial::Prepare(seed, arena);
    mockRandom = LiftedTrial::Random{trial.mockSeed};
    calls.clear();
    armed = true;
    const std::uint64_t result = c.wrapper(trial.args[0], trial.args[1], trial.args[2], trial.args[3], trial.args[4], trial.args[5]);
    armed = false;
    LiftedTrial::Digest digest{};
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
    Require(mapped == reinterpret_cast<void*>(ArenaAddress));
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
    for (const Wrapper wrapper : {&Thunk1, &Thunk4, &Thunk5, &Thunk8, &Thunk9, &Thunk11, &Thunk12, &Thunk29, &Thunk30, &Thunk33, &Thunk34, &Thunk35, &Thunk36, &Thunk37, &Thunk40, &Thunk41, &Thunk45, &Thunk48, &Thunk49, &Thunk50, &Thunk52}) {
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
