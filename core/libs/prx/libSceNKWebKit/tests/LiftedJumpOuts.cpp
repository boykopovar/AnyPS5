#include "prx/libSceNKWebKit/include/LiftedJumpOutsExports.hpp"

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
        std::fprintf(stderr, "LiftedJumpOuts check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Wrapper = std::uint64_t (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

static std::uint64_t APS5_VABI Thunk0(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKAXObjectPage(a0);
}

static std::uint64_t APS5_VABI Thunk1(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleFrameGetJavaScriptContext(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk2(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleFrameGetPendingUnloadCount(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk3(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleFrameStopLoading(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk4(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleGarbageCollectJavaScriptObjects(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk5(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk6(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageClearMainFrameName(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk7(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageIsTrackingRepaints(a0);
}

static std::uint64_t APS5_VABI Thunk8(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    WKBundlePageListenForLayoutMilestones(a0, a1, a2, a3, a4, a5);
    return 0;
}

static std::uint64_t APS5_VABI Thunk9(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageOverlayClear(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk10(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageResetTrackedRepaints(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk11(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageSetAppCacheMaximumSize(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk12(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    WKBundlePageSetPolicyClient(a0, a1);
    return 0;
}

static std::uint64_t APS5_VABI Thunk13(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundlePageSetTracksRepaints(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk14(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKBundleSetTabKeyCyclesThroughElements(a0, a1, a2);
}

static std::uint64_t APS5_VABI Thunk15(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    WKContextSetConnectionClient(a0, a1);
    return 0;
}

static std::uint64_t APS5_VABI Thunk16(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKPageGetContext(a0);
}

static std::uint64_t APS5_VABI Thunk17(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    WKViewSetViewAccessibilityClient(a0, a1);
    return 0;
}

static std::uint64_t APS5_VABI Thunk18(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(a0);
}

static std::uint64_t APS5_VABI Thunk19(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3JSC21MarkedJSValueRefArrayD1Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk20(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3WTF24AutomaticThreadConditionD1Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk21(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN3WTF24AutomaticThreadConditionD2Ev(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk22(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore13GraphicsLayer18setBackgroundColorERKNS_5ColorE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk23(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore18TextureMapperLayer13setSolidColorERKNS_5ColorE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk24(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore4Page31setRemoteInspectionNameOverrideERKN3WTF6StringE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk25(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Document56hasRequestedPageSpecificStorageAccessWithUserInteractionERKNS_17RegistrableDomainE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk26(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings23setUsesBackForwardCacheEb(a0, a1);
}

static std::uint64_t APS5_VABI Thunk27(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings24setVisualViewportEnabledEb(a0, a1);
}

static std::uint64_t APS5_VABI Thunk28(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings27setLoadsImagesAutomaticallyEb(a0, a1, a2, a3);
}

static std::uint64_t APS5_VABI Thunk29(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings32setAcceleratedCompositingEnabledEb(a0, a1, a2, a3);
}

static std::uint64_t APS5_VABI Thunk30(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings32setShouldAllowUserInstalledFontsEb(a0, a1, a2, a3);
}

static std::uint64_t APS5_VABI Thunk31(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings33setFontFallbackPrefersPictographsEb(a0, a1, a2, a3);
}

static std::uint64_t APS5_VABI Thunk32(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCore8Settings41setAcceleratedCompositedAnimationsEnabledEb(a0, a1, a2, a3);
}

static std::uint64_t APS5_VABI Thunk33(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk34(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore13HitTestResult14innerNodeFrameEv(a0);
}

static std::uint64_t APS5_VABI Thunk35(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore18DOMWindowExtension5frameEv(a0);
}

static std::uint64_t APS5_VABI Thunk36(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore20ScrollingCoordinator36coordinatesScrollingForOverflowLayerERKNS_11RenderLayerE(a0, a1);
}

static std::uint64_t APS5_VABI Thunk37(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore4Node7baseURIEv(a0, a1, a2, a3, a4, a5);
}

static std::uint64_t APS5_VABI Thunk38(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore8Document4pageEv(a0);
}

static std::uint64_t APS5_VABI Thunk39(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return _ZNK7WebCore9DOMWindow5frameEv(a0);
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

extern "C" std::uint64_t APS5_VABI _ZN3WTF10StringImpl7destroyEPS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(0, "_ZN3WTF10StringImpl7destroyEPS0_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF10TextStreamlsEPKv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(1, "_ZN3WTF10TextStreamlsEPKv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF13aboutBlankURLEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(2, "_ZN3WTF13aboutBlankURLEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF5equalEPKNS_10StringImplES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(3, "_ZN3WTF5equalEPKNS_10StringImplES2_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN3WTF8fastFreeEPv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(4, "_ZN3WTF8fastFreeEPv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11FrameLoader17stopForUserCancelEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(5, "_ZN7WebCore11FrameLoader17stopForUserCancelEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore11PageOverlay5clearEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(6, "_ZN7WebCore11PageOverlay5clearEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore12GCController17garbageCollectNowEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(7, "_ZN7WebCore12GCController17garbageCollectNowEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore12GCController43garbageCollectOnAlternateThreadForDebuggingEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(8, "_ZN7WebCore12GCController43garbageCollectOnAlternateThreadForDebuggingEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore12GCController9singletonEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(9, "_ZN7WebCore12GCController9singletonEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16BackForwardCache14pruneToSizeNowEjNS_13PruningReasonE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(10, "_ZN7WebCore16BackForwardCache14pruneToSizeNowEjNS_13PruningReasonE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16BackForwardCache9singletonEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(11, "_ZN7WebCore16BackForwardCache9singletonEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore16ScriptController13jsWindowProxyERNS_15DOMWrapperWorldE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(12, "_ZN7WebCore16ScriptController13jsWindowProxyERNS_15DOMWrapperWorldE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore21mainThreadNormalWorldEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(13, "_ZN7WebCore21mainThreadNormalWorldEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore23ApplicationCacheStorage14setMaximumSizeEl(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(14, "_ZN7WebCore23ApplicationCacheStorage14setMaximumSizeEl", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(15, "_ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(16, "_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore5ColoraSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(17, "_ZN7WebCore5ColoraSERKS0_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9FrameTree9clearNameEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(18, "_ZN7WebCore9FrameTree9clearNameEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9FrameView17setTracksRepaintsEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(19, "_ZN7WebCore9FrameView17setTracksRepaintsEb", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9FrameView20resetTrackedRepaintsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(20, "_ZN7WebCore9FrameView20resetTrackedRepaintsEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore9TimerBase5startEN3WTF7SecondsES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(21, "_ZN7WebCore9TimerBase5startEN3WTF7SecondsES2_", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN8Gigacage4freeENS_4KindEPv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(22, "_ZN8Gigacage4freeENS_4KindEPv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZN9Inspector24RemoteControllableTarget6updateEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(23, "_ZN9Inspector24RemoteControllableTarget6updateEv", a0, a1, a2, a3, a4, a5);
}

extern "C" std::uint64_t APS5_VABI _ZNK7WebCore9DOMWindow27pendingUnloadEventListenersEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Mock(24, "_ZNK7WebCore9DOMWindow27pendingUnloadEventListenersEv", a0, a1, a2, a3, a4, a5);
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
    {"WKAXObjectPage", &Thunk0, 3, {{4u, 0x21e1aa45f42c305dull}, {7u, 0x0b325765155c8b65ull}, {8u, 0xcc71563292c53c6bull}, {10u, 0xa4d5f04f713e3bc9ull}, {11u, 0x203dce6cf41cf783ull}, {13u, 0x8495ddaee7bc7fc4ull}, {15u, 0x409ee6c1617795e6ull}, {22u, 0xe51fa826780de0f9ull}}},
    {"WKBundleFrameGetJavaScriptContext", &Thunk1, 3, {{2u, 0xe956727e1c1f980full}, {7u, 0xa3a873f9f8936669ull}, {12u, 0x00d6871f8138c792ull}, {15u, 0x76cc0c715ec61500ull}, {25u, 0x7e2407216533c597ull}, {27u, 0xc37e6073ff19537bull}, {29u, 0x296d4f413d7e9b3cull}, {30u, 0x83a3f4486ee8e129ull}}},
    {"WKBundleFrameGetPendingUnloadCount", &Thunk2, 3, {{2u, 0xe956727e1c1f980full}, {15u, 0x76cc0c715ec61500ull}, {16u, 0xfde9c6e66015185aull}, {21u, 0xcd476af1c0b43e76ull}, {22u, 0x6e919272c3f8f94full}, {25u, 0x7e2407216533c597ull}, {27u, 0xfa182f97721aec61ull}, {29u, 0x296d4f413d7e9b3cull}}},
    {"WKBundleFrameStopLoading", &Thunk3, 4, {{2u, 0xe956727e1c1f980full}, {7u, 0xbcfbcecc2f68717eull}, {12u, 0xc896d40befca4c1full}, {15u, 0x76cc0c715ec61500ull}, {16u, 0x423f59ceb460e43aull}, {21u, 0xe1b80ca90eb7972dull}, {22u, 0xe912c4bf91fbc908ull}, {25u, 0x7e2407216533c597ull}}},
    {"WKBundleGarbageCollectJavaScriptObjects", &Thunk4, 4, {{1u, 0x2053da0b2518bc19ull}, {2u, 0xe22728ba78298665ull}, {3u, 0x02cef873d60901e5ull}, {4u, 0x0881d320b853e07bull}, {5u, 0xe9d70ecaa658e59eull}, {6u, 0xf09c094ea96d6ec6ull}, {7u, 0xafff3abedd9d8b02ull}, {8u, 0x316989850b50b5bdull}}},
    {"WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging", &Thunk5, 4, {{1u, 0x019bddd07b554c3dull}, {2u, 0x63a4126f3df978faull}, {3u, 0x6d6e6e0c80ebafd2ull}, {4u, 0x669fb795f74549ddull}, {5u, 0x7647ba11277b9864ull}, {6u, 0xa90f6cc0d5cbc656ull}, {7u, 0xb908a649729bc80dull}, {8u, 0x6d3a8d192c191163ull}}},
    {"WKBundlePageClearMainFrameName", &Thunk6, 4, {{2u, 0xe956727e1c1f980full}, {4u, 0x27272c428210ede1ull}, {7u, 0x7ba7c0a3a0933edaull}, {8u, 0x83913c02130c8849ull}, {9u, 0xe3da91c096df2b5full}, {10u, 0x380cea3c16c1aa2bull}, {11u, 0x4266945a1a5e10f1ull}, {15u, 0xde000f6a009d799dull}}},
    {"WKBundlePageIsTrackingRepaints", &Thunk7, 3, {{2u, 0xe956727e1c1f980full}, {4u, 0xc0bc00d96de0e7eeull}, {7u, 0x0b325765155c8b65ull}, {8u, 0xcc71563292c53c6bull}, {9u, 0x6f8c0493fec8f91dull}, {10u, 0x380cea3c16c1aa2bull}, {11u, 0x2179eb608740c133ull}, {19u, 0x914f252178240604ull}}},
    {"WKBundlePageListenForLayoutMilestones", &Thunk8, 0, {{1u, 0xd9e63d852c980269ull}, {2u, 0xe956727e1c1f980full}, {3u, 0x55c25574072bcfe9ull}, {4u, 0xd469068b4713cc1full}, {5u, 0x8d9373c2bdb13019ull}, {6u, 0xfc15ab20a1c99330ull}, {7u, 0xbf281f4c6bc93954ull}, {8u, 0x719afecc13f1d03cull}}},
    {"WKBundlePageOverlayClear", &Thunk9, 4, {{1u, 0xa797b2db6f9b137aull}, {2u, 0xc2be3ba584942559ull}, {3u, 0x3ef97975843795f7ull}, {4u, 0x57fd6bae7398fe85ull}, {5u, 0x57e8d9f7a717db34ull}, {6u, 0x2c05bf202621d182ull}, {7u, 0xa1bcb0df60780d4cull}, {8u, 0x99db0fe7ea51131eull}}},
    {"WKBundlePageResetTrackedRepaints", &Thunk10, 4, {{2u, 0xe956727e1c1f980full}, {4u, 0xc0bc00d96de0e7eeull}, {7u, 0x0b325765155c8b65ull}, {8u, 0xcc71563292c53c6bull}, {9u, 0x51782d527eeb286eull}, {10u, 0x380cea3c16c1aa2bull}, {11u, 0x61b52ed725c891e0ull}, {19u, 0x9340a1ef67d0a4b8ull}}},
    {"WKBundlePageSetAppCacheMaximumSize", &Thunk11, 4, {{4u, 0x967c57410173a8f2ull}, {7u, 0x0f62800f3a1eb9d6ull}, {8u, 0x43482004faa959e2ull}, {9u, 0x0e27a6fadce86ea4ull}, {11u, 0x0f2caa92b0ea26fbull}, {15u, 0x994f4ebb41ab4b40ull}, {19u, 0xd66ff176df7846e9ull}, {20u, 0xb1cb342b02f4e7d6ull}}},
    {"WKBundlePageSetPolicyClient", &Thunk12, 0, {{1u, 0xe39ba759887c3589ull}, {2u, 0x7131c03e7b3c4a61ull}, {3u, 0xcc6b6b16ca4afbf4ull}, {4u, 0x58458792f715b6fcull}, {5u, 0x555afa2739bd2f29ull}, {7u, 0x3b9416d25077e6dcull}, {9u, 0x47cdb04aa2d82ea9ull}, {10u, 0x2f22e0a7374b896eull}}},
    {"WKBundlePageSetTracksRepaints", &Thunk13, 4, {{2u, 0xe956727e1c1f980full}, {4u, 0xc0bc00d96de0e7eeull}, {7u, 0x0b325765155c8b65ull}, {8u, 0xcc71563292c53c6bull}, {9u, 0x51782d527eeb286eull}, {10u, 0x380cea3c16c1aa2bull}, {11u, 0x63d303d1b25614dbull}, {19u, 0x523c35c98d5ccaf5ull}}},
    {"WKBundleSetTabKeyCyclesThroughElements", &Thunk14, 4, {{2u, 0x3430542757a105abull}, {4u, 0x615e2f4ef8bb7e61ull}, {7u, 0xc5dc95253e1d28cdull}, {9u, 0xc828fc0e26b4102bull}, {10u, 0x991097c39caddf23ull}, {12u, 0xcfa69c540c3c9b25ull}, {21u, 0xa23bc70fad509159ull}, {25u, 0x1b69f455f19b5f19ull}}},
    {"WKContextSetConnectionClient", &Thunk15, 0, {{1u, 0x701c08da8e3674fdull}, {2u, 0xf00bfe2cc3946598ull}, {3u, 0x68dc2d062e63b25eull}, {4u, 0x392850d1e0af5180ull}, {5u, 0x16ec1204812e4378ull}, {7u, 0x514cd145b40f8b68ull}, {9u, 0xd8a551d8433ef451ull}, {10u, 0x032e6d7f0cc6261eull}}},
    {"WKPageGetContext", &Thunk16, 3, {{9u, 0xb2c4493d55942d01ull}, {15u, 0x2b712b78017a6379ull}, {20u, 0x181ca1425c20611bull}, {22u, 0x5b24d34009e7aaf5ull}, {26u, 0x90d9428d658b020aull}, {32u, 0x0a5e199a11656367ull}, {41u, 0xed627da19564e8eaull}, {43u, 0x6d61c49fb41767e7ull}}},
    {"WKViewSetViewAccessibilityClient", &Thunk17, 0, {{1u, 0x34257533915e4f89ull}, {2u, 0x70cf56aa8377edbbull}, {3u, 0x165b549abd6f24d0ull}, {4u, 0xc1bbf6fab3bac43cull}, {5u, 0x0670573ef918b92cull}, {7u, 0x25006276a3403cadull}, {9u, 0x1b8c98c861dc5cd4ull}, {10u, 0x145b6c2b7146bea3ull}}},
    {"WKWebsiteDataStoreGetResourceLoadStatisticsEnabled", &Thunk18, 1, {{1u, 0x47771a045ca20fa8ull}, {2u, 0xe956727e1c1f980full}, {3u, 0x658582b8d8030119ull}, {4u, 0xeb054fd104653ac2ull}, {5u, 0xc7f20d2c69a64b5dull}, {6u, 0x5e4985d0b7a29746ull}, {7u, 0x4456cd6ab104542dull}, {8u, 0x741e29afa0ee9fbfull}}},
    {"_ZN3JSC21MarkedJSValueRefArrayD1Ev", &Thunk19, 4, {{1u, 0xd38bd13e1bbc7ceeull}, {6u, 0x5d47d9cb3c3479d2ull}, {7u, 0x597ce943c81939b7ull}, {9u, 0x2892ce7d67c83f01ull}, {16u, 0x171819594a9ab3afull}, {18u, 0x4505c9c9f0d807f0ull}, {22u, 0x2aaf09d5a85f88bdull}, {25u, 0xec2020a8f235420cull}}},
    {"_ZN3WTF24AutomaticThreadConditionD1Ev", &Thunk20, 4, {{1u, 0x43e98df2da618e24ull}, {2u, 0xd1f3acd831321778ull}, {3u, 0x8ce36391661e14baull}, {4u, 0x0de89efbb09e0544ull}, {5u, 0x583b0f66b4aaa13eull}, {6u, 0x788d0a89fb2c8095ull}, {7u, 0x3a2026499005b472ull}, {8u, 0x3197eaa27016ff8full}}},
    {"_ZN3WTF24AutomaticThreadConditionD2Ev", &Thunk21, 4, {{1u, 0x43e98df2da618e24ull}, {2u, 0xd1f3acd831321778ull}, {3u, 0x8ce36391661e14baull}, {4u, 0x0de89efbb09e0544ull}, {5u, 0x583b0f66b4aaa13eull}, {6u, 0x788d0a89fb2c8095ull}, {7u, 0x3a2026499005b472ull}, {8u, 0x3197eaa27016ff8full}}},
    {"_ZN7WebCore13GraphicsLayer18setBackgroundColorERKNS_5ColorE", &Thunk22, 4, {{1u, 0xb44877c68e25b5e6ull}, {2u, 0x270324af52e39ee2ull}, {3u, 0x0706a498f7a8c404ull}, {4u, 0x7ebded9e88e37850ull}, {5u, 0xcf2724843d092670ull}, {6u, 0x1881aa1a14980093ull}, {7u, 0x3a4bee0844be5a6eull}, {8u, 0x586478df9c91dea7ull}}},
    {"_ZN7WebCore18TextureMapperLayer13setSolidColorERKNS_5ColorE", &Thunk23, 4, {{1u, 0xd9c8b932cd5d6684ull}, {2u, 0x3bcd5b128c7e17d8ull}, {3u, 0xb6fd3977ae31a6baull}, {4u, 0x6e0237575106016eull}, {5u, 0x42a228aa09bcc846ull}, {6u, 0x283cc29faa628211ull}, {7u, 0x2c8dc67980939918ull}, {8u, 0x3acb3d2ce051797dull}}},
    {"_ZN7WebCore4Page31setRemoteInspectionNameOverrideERKN3WTF6StringE", &Thunk24, 4, {{1u, 0x0eba5feb87f8adfaull}, {5u, 0xd1f7208cb445737bull}, {16u, 0x178d40b9e9342991ull}, {38u, 0xed744312be9f9f44ull}, {42u, 0x295e71760dde5105ull}, {49u, 0x0e0a6fb3de47d531ull}, {53u, 0xbf0e31c9c7e72748ull}, {54u, 0x469aa11bee6b8a76ull}}},
    {"_ZN7WebCore8Document56hasRequestedPageSpecificStorageAccessWithUserInteractionERKNS_17RegistrableDomainE", &Thunk25, 4, {{1u, 0x64eb38cdd2dff57cull}, {2u, 0xe731ef862b72991cull}, {4u, 0xa4755b655266759bull}, {5u, 0x13a627236fd9a27eull}, {7u, 0x0b636ffe10d3adb8ull}, {9u, 0x47f45122bfdc343dull}, {10u, 0x34860f03c3f6cb0eull}, {11u, 0xb2d1f150a2cd9834ull}}},
    {"_ZN7WebCore8Settings23setUsesBackForwardCacheEb", &Thunk26, 3, {{1u, 0x712229914d97f766ull}, {2u, 0xaf112928c749849eull}, {4u, 0xf1928080b318bd4bull}, {5u, 0x888416b907dfcf1full}, {6u, 0xa4a09e4683e4f031ull}, {7u, 0xa6183a924c08fe0aull}, {8u, 0x11120e5bad227770ull}, {9u, 0xfe7c6b2ece4a1aeaull}}},
    {"_ZN7WebCore8Settings24setVisualViewportEnabledEb", &Thunk27, 4, {{1u, 0x712229914d97f766ull}, {2u, 0xaf112928c749849eull}, {3u, 0x55eeb8ce7a7fa3dcull}, {4u, 0xf1928080b318bd4bull}, {5u, 0x888416b907dfcf1full}, {6u, 0xe5eaf441a2e14513ull}, {7u, 0xa6183a924c08fe0aull}, {8u, 0x11120e5bad227770ull}}},
    {"_ZN7WebCore8Settings27setLoadsImagesAutomaticallyEb", &Thunk28, 4, {{1u, 0x2d815a7c17291ac4ull}, {2u, 0x669d07c1eb12e25eull}, {3u, 0x28ac9f287b21c186ull}, {4u, 0x59ba3be3ec1d3c70ull}, {5u, 0x888416b907dfcf1full}, {6u, 0x80690d10a5a13b22ull}, {7u, 0x2b427e842d4110ccull}, {8u, 0xdbe955bd82d03a7cull}}},
    {"_ZN7WebCore8Settings32setAcceleratedCompositingEnabledEb", &Thunk29, 4, {{1u, 0x35a88265a7093e1dull}, {2u, 0x379f8fff83098a41ull}, {3u, 0x6481c88a2ac0e619ull}, {4u, 0x690fe2f9ef5fbbcbull}, {5u, 0x888416b907dfcf1full}, {6u, 0x8558519bd72f07f4ull}, {7u, 0xff5e00008aa4e7baull}, {8u, 0x8f46b2dcb0203ba7ull}}},
    {"_ZN7WebCore8Settings32setShouldAllowUserInstalledFontsEb", &Thunk30, 4, {{1u, 0x9a6a8bc4dbeb3d63ull}, {2u, 0xe8485b9d0a1567c7ull}, {3u, 0x373d9bd99024ea78ull}, {4u, 0xc8e5c891166696e6ull}, {5u, 0x409e8c4e7197479aull}, {6u, 0xf42a617d28d8b4aeull}, {7u, 0xc513019b56f8482bull}, {8u, 0xe7f27259ceb0b31cull}}},
    {"_ZN7WebCore8Settings33setFontFallbackPrefersPictographsEb", &Thunk31, 4, {{1u, 0x5eaaaa5fd8e23f4eull}, {2u, 0x48c384ddbbabc039ull}, {3u, 0xc3665da05de558c8ull}, {4u, 0x39d9eb417e553280ull}, {5u, 0x888416b907dfcf1full}, {6u, 0xcd6b0080d9374c4dull}, {7u, 0xd404699ff1ae9de4ull}, {8u, 0xb75ee6a0b793b60cull}}},
    {"_ZN7WebCore8Settings41setAcceleratedCompositedAnimationsEnabledEb", &Thunk32, 4, {{1u, 0x35a88265a7093e1dull}, {2u, 0x379f8fff83098a41ull}, {3u, 0x6481c88a2ac0e619ull}, {4u, 0x690fe2f9ef5fbbcbull}, {5u, 0x888416b907dfcf1full}, {6u, 0x8558519bd72f07f4ull}, {7u, 0xff5e00008aa4e7baull}, {8u, 0x8f46b2dcb0203ba7ull}}},
    {"_ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE", &Thunk33, 4, {{1u, 0xf7c31be72844fa30ull}, {2u, 0x92c6a22cbbbcfe8bull}, {4u, 0x9255fd77ac05062full}, {5u, 0xcbe9e4a645f8c4e0ull}, {7u, 0x17d4b416c2143674ull}, {9u, 0xaacb5cd8b9ca8db4ull}, {10u, 0x0d129769a8a0a361ull}, {11u, 0xbeeca1d4c4dc25a7ull}}},
    {"_ZNK7WebCore13HitTestResult14innerNodeFrameEv", &Thunk34, 3, {{19u, 0x28afdb4f824ce583ull}, {30u, 0x83a3f4486ee8e129ull}, {35u, 0xc54b3d73ba9857ccull}, {36u, 0x28fcb1c77b7d0731ull}, {42u, 0xbebb4f676cf4aeaeull}, {54u, 0x813254aa347d6652ull}, {55u, 0x7789cfa565e36746ull}, {65u, 0x383c3c29ceb17fd2ull}}},
    {"_ZNK7WebCore18DOMWindowExtension5frameEv", &Thunk35, 3, {{7u, 0xa07fa1ddc1627c38ull}, {9u, 0x6f8c0493fec8f91dull}, {13u, 0x0f8ca61122e02078ull}, {16u, 0x171819594a9ab3afull}, {20u, 0x181ca1425c20611bull}, {22u, 0x5b24d34009e7aaf5ull}, {25u, 0x13dbf2124608fda9ull}, {28u, 0x1435a82ae012217eull}}},
    {"_ZNK7WebCore20ScrollingCoordinator36coordinatesScrollingForOverflowLayerERKNS_11RenderLayerE", &Thunk36, 3, {{1u, 0xfde7dd098e5c4628ull}, {2u, 0x6fda4f067fde3751ull}, {4u, 0xb8b7684137d9a7f9ull}, {7u, 0x0b325765155c8b65ull}, {9u, 0x6f8c0493fec8f91dull}, {10u, 0x778b3c6a90ba494dull}, {11u, 0x4dac11af58b13f9dull}, {12u, 0x2c3d5c8a7006dd26ull}}},
    {"_ZNK7WebCore4Node7baseURIEv", &Thunk37, 4, {{5u, 0xf904e12ab0c888ffull}, {6u, 0x9ffb67113945b7c7ull}, {8u, 0x022b5cf5880a28d0ull}, {10u, 0xbea4b5a3e9feabe8ull}, {11u, 0xb1a793a094c72159ull}, {12u, 0xc4393644183b34cbull}, {21u, 0x6c72ccf756b5e87full}, {23u, 0x9634e9593ad092edull}}},
    {"_ZNK7WebCore8Document4pageEv", &Thunk38, 3, {{3u, 0x48313d29466764e6ull}, {5u, 0x0069e6a2851c8fd8ull}, {6u, 0xf42a617d28d8b4aeull}, {7u, 0x0b325765155c8b65ull}, {9u, 0x6f8c0493fec8f91dull}, {10u, 0x1b0722a9ad38b42cull}, {11u, 0x4dac11af58b13f9dull}, {13u, 0x3eef1ec09ae07922ull}}},
    {"_ZNK7WebCore9DOMWindow5frameEv", &Thunk39, 3, {{2u, 0xe956727e1c1f980full}, {4u, 0x93496f5fe52dd649ull}, {5u, 0x888416b907dfcf1full}, {8u, 0xcc71563292c53c6bull}, {9u, 0x6f8c0493fec8f91dull}, {15u, 0x76cc0c715ec61500ull}, {16u, 0x3bf22867cae882f2ull}, {20u, 0x181ca1425c20611bull}}},
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
    for (const Wrapper wrapper : {&Thunk0, &Thunk1, &Thunk2, &Thunk6, &Thunk7, &Thunk8, &Thunk9, &Thunk10, &Thunk11, &Thunk13, &Thunk16, &Thunk18, &Thunk20, &Thunk21, &Thunk24, &Thunk25, &Thunk26, &Thunk27, &Thunk34, &Thunk35, &Thunk37, &Thunk38, &Thunk39}) {
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
