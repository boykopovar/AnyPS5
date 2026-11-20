#include "prx/libSceNKWebKit/include/LiftedBranchingFunctionsExports.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

constexpr const char* TestName = "LiftedBranchingFunctions";

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "%s check failed at line %d\n", TestName, line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct SplitMix {
    std::uint64_t state;
    std::uint64_t Next() {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
};

constexpr std::size_t ArenaWords = 32768;
constexpr std::size_t ArenaBytes = ArenaWords * 8;
constexpr std::uint64_t ArenaAddress = 0x5a0000000000ull;

static std::uint64_t* MapArena() {
    void* const wanted = reinterpret_cast<void*>(ArenaAddress);
#if defined(_WIN32)
    void* const mapped = VirtualAlloc(wanted, ArenaBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mapped != wanted) {
        std::fprintf(stderr, "%s could not map the arena at %#llx: error %lu\n", TestName, static_cast<unsigned long long>(ArenaAddress),
                     static_cast<unsigned long>(GetLastError()));
        std::abort();
    }
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_FIXED_NOREPLACE)
    flags |= MAP_FIXED_NOREPLACE;
#endif
    void* const mapped = mmap(wanted, ArenaBytes, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (mapped != wanted) {
        const int error = errno;
        std::fprintf(stderr, "%s could not map the arena at %#llx (got %p): %s\n", TestName, static_cast<unsigned long long>(ArenaAddress), mapped,
                     std::strerror(error));
        std::abort();
    }
#endif
    return static_cast<std::uint64_t*>(mapped);
}

static void UnmapArena(std::uint64_t* arena) {
#if defined(_WIN32)
    VirtualFree(arena, 0, MEM_RELEASE);
#else
    munmap(arena, ArenaBytes);
#endif
}

struct Inputs {
    std::uint64_t gpr[6];
    std::uint64_t xmm[8];
};

static Inputs Generate(std::uint64_t seed, std::uint64_t* arena, const std::uint64_t* hints, std::size_t hintCount) {
    SplitMix random{seed * 0x100000001b3ull + 0x51ed27};
    const std::uint64_t base = reinterpret_cast<std::uint64_t>(arena);
    auto pointer = [&] { return base + 0x10000 + (random.Next() % 0x10000) / 8 * 8; };
    auto hint = [&] { return hintCount ? hints[random.Next() % hintCount] : 0; };
    const int mode = static_cast<int>(seed % 3);
    std::uint64_t pool[12];
    for (int k = 0; k < 4; ++k) pool[k] = pointer();
    for (int k = 4; k < 8; ++k) pool[k] = std::uint64_t{1} << (random.Next() % 64);
    for (int k = 8; k < 12; ++k) pool[k] = random.Next() % 3 ? hint() : random.Next() % 8;
    for (std::size_t i = 0; i < ArenaWords; ++i) {
        const std::uint64_t pick = random.Next() % 100;
        std::uint64_t value;
        if (mode == 2) {
            const std::uint64_t full = hint();
            const std::uint64_t h = full & 0xffffffffull;
            value = pick < 45 ? pool[random.Next() % 4] : pick < 55 ? 0 : pick < 72 ? (h | (h << 32)) : pick < 86 ? full : full << (8 * (random.Next() % 8));
        } else if (mode == 1) {
            value = pick < 30 ? 0 : pool[random.Next() % 12];
            if (pick >= 85) value = hint() << (8 * (random.Next() % 8));
        } else if (pick < 40) {
            value = pointer();
        } else if (pick < 55) {
            value = 0;
        } else if (pick < 70) {
            value = random.Next() % 17;
        } else {
            value = random.Next();
        }
        arena[i] = value;
    }
    Inputs inputs{};
    for (auto& g : inputs.gpr) {
        const std::uint64_t pick = random.Next() % 100;
        g = pick < 85 ? (mode != 0 && random.Next() % 2 ? pool[random.Next() % 4] : pointer()) : pick < 92 ? 0 : random.Next() % 4096;
    }
    for (auto& x : inputs.xmm) {
        const std::uint64_t r = random.Next();
        const std::int64_t whole = static_cast<std::int64_t>(r % 2000) - 1000;
        const double fraction = static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);
        const double d = random.Next() % 4 == 0 ? static_cast<double>(whole) : fraction * 2e6 - 1e6;
        if (random.Next() % 3 == 0) {
            const float f = static_cast<float>(d);
            std::uint32_t bits;
            std::memcpy(&bits, &f, 4);
            x = bits;
        } else {
            std::memcpy(&x, &d, 8);
        }
    }
    return inputs;
}

static std::uint64_t Normalize(std::uint64_t value, std::uint64_t base) {
    return value - base < ArenaBytes ? ((value - base) | 0xa5a5000000000000ull) : value;
}

static std::uint64_t Digest(const std::uint64_t* arena, std::uint64_t result, int kind) {
    const std::uint64_t base = reinterpret_cast<std::uint64_t>(arena);
    std::uint64_t hash = 0xcbf29ce484222325ull;
    auto mix = [&](std::uint64_t word) {
        hash ^= word;
        hash *= 0x100000001b3ull;
        hash ^= hash >> 29;
    };
    for (std::size_t i = 0; i < ArenaWords; ++i) mix(Normalize(arena[i], base));
    static const std::uint64_t masks[] = {0, 0xff, 0xffff, 0xffffffffull, ~0ull, 0xffffffffull, ~0ull};
    std::uint64_t r = result & masks[kind];
    if (kind == 4) r = Normalize(r, base);
    mix(r);
    return hash;
}

template<typename T> static T Scalar(std::uint64_t bits) {
    T value;
    std::memcpy(&value, &bits, sizeof(T));
    return value;
}

template<typename T> static std::uint64_t BitsOfScalar(T value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(T));
    return bits;
}

static std::uint64_t Call0(const Inputs& in) {
    return _ZN3JSC17DebuggerCallFrame11callerFrameEv(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call1(const Inputs& in) {
    return _ZN3JSC4Heap32setGarbageCollectionTimerEnabledEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call2(const Inputs& in) {
    return _ZN3JSC4Yarr17RegularExpressionC1ERKS1_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call3(const Inputs& in) {
    return _ZN3JSC4Yarr17RegularExpressionC2ERKS1_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call4(const Inputs& in) {
    return _ZN3JSC8Bindings13RuntimeObjectC1ERKS1_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call5(const Inputs& in) {
    return _ZN3JSC8Bindings13RuntimeObjectC2ERKS1_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call6(const Inputs& in) {
    return _ZN3WTF15AutomaticThread7tryStopERKNS_14AbstractLockerE(in.gpr[0]);
}

static std::uint64_t Call7(const Inputs& in) {
    _ZN3WTF21MemoryPressureHandler15setProcessStateENS_18WebsamProcessStateE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call8(const Inputs& in) {
    return _ZN7WebCore10ScrollView21setExposedContentRectERKNS_9FloatRectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call9(const Inputs& in) {
    _ZN7WebCore11FrameLoader21clearTestingOverridesEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call10(const Inputs& in) {
    return _ZN7WebCore11FrameLoader6openerEv(in.gpr[0]);
}

static std::uint64_t Call11(const Inputs& in) {
    return _ZN7WebCore14PluginDocument12pluginWidgetEv(in.gpr[0]);
}

static std::uint64_t Call12(const Inputs& in) {
    return _ZN7WebCore15DatabaseManager16hasOpenDatabasesERNS_8DocumentE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call13(const Inputs& in) {
    _ZN7WebCore17HTMLCanvasElement25setUsesDisplayListDrawingEb(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call14(const Inputs& in) {
    _ZN7WebCore21NetworkStorageSession38setCacheMaxAgeCapForPrevalentResourcesEN3WTF7SecondsE(in.gpr[0], Scalar<double>(in.xmm[0]));
    return 0;
}

static std::uint64_t Call15(const Inputs& in) {
    _ZN7WebCore21NetworkStorageSession40resetCacheMaxAgeCapForPrevalentResourcesEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call16(const Inputs& in) {
    return _ZN7WebCore24CoordinatedGraphicsLayer20didUpdateTileBuffersEv(in.gpr[0]);
}

static std::uint64_t Call17(const Inputs& in) {
    _ZN7WebCore27PlatformMediaSessionManager32setIsPlayingToAutomotiveHeadUnitEb(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call18(const Inputs& in) {
    _ZN7WebCore4Page27setVerticalScrollElasticityENS_16ScrollElasticityE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call19(const Inputs& in) {
    _ZN7WebCore4Page29setHorizontalScrollElasticityENS_16ScrollElasticityE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call20(const Inputs& in) {
    return _ZN7WebCore4PathC1EOS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call21(const Inputs& in) {
    return _ZN7WebCore4PathC2EOS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call22(const Inputs& in) {
    return _ZN7WebCore4toJSERNS_12VoidCallbackE(in.gpr[0]);
}

static std::uint64_t Call23(const Inputs& in) {
    return _ZN7WebCore4toJSERNS_14StringCallbackE(in.gpr[0]);
}

static std::uint64_t Call24(const Inputs& in) {
    return _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(in.gpr[0], in.gpr[1], in.gpr[2], in.gpr[3]);
}

static std::uint64_t Call25(const Inputs& in) {
    return _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(in.gpr[0], in.gpr[1], in.gpr[2], in.gpr[3]);
}

static std::uint64_t Call26(const Inputs& in) {
    return _ZN7WebCore9CSSParserC1ERKNS_16CSSParserContextE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call27(const Inputs& in) {
    return _ZN7WebCore9CSSParserC2ERKNS_16CSSParserContextE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call28(const Inputs& in) {
    _ZN7WebCore9DOMWindow29setCanShowModalDialogOverrideEb(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call29(const Inputs& in) {
    return _ZN7WebCore9FontCache9fontCountEv(in.gpr[0]);
}

static std::uint64_t Call30(const Inputs& in) {
    _ZN9Inspector21InspectorConsoleAgent29willDestroyFrontendAndBackendENS_16DisconnectReasonE(in.gpr[0]);
    return 0;
}

static std::uint64_t Call31(const Inputs& in) {
    return _ZNK3JSC2VM29deprecatedVMEntryGlobalObjectEPNS_14JSGlobalObjectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call32(const Inputs& in) {
    return _ZNK3JSC7ArgList8getSliceEiRS0_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call33(const Inputs& in) {
    return _ZNK3JSC8Bindings10RootObject12globalObjectEv(in.gpr[0]);
}

static std::uint64_t Call34(const Inputs& in) {
    return _ZNK7WebCore10RenderText8textNodeEv(in.gpr[0]);
}

static std::uint64_t Call35(const Inputs& in) {
    return _ZNK7WebCore10RenderView15usesCompositingEv(in.gpr[0]);
}

static std::uint64_t Call36(const Inputs& in) {
    return _ZNK7WebCore10ScrollView14scrollbarModesERNS_13ScrollbarModeES2_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call37(const Inputs& in) {
    return _ZNK7WebCore10StorageMap6lengthEv(in.gpr[0]);
}

static std::uint64_t Call38(const Inputs& in) {
    return _ZNK7WebCore11PageOverlay10controllerEv(in.gpr[0]);
}

static std::uint64_t Call39(const Inputs& in) {
    return _ZNK7WebCore11XPathResult20invalidIteratorStateEv(in.gpr[0]);
}

static std::uint64_t Call40(const Inputs& in) {
    return _ZNK7WebCore13CSSImportRule4hrefEv(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call41(const Inputs& in) {
    return _ZNK7WebCore13HitTestResult21innerNonSharedElementEv(in.gpr[0]);
}

static std::uint64_t Call42(const Inputs& in) {
    return _ZNK7WebCore13MIMETypeCache7isEmptyEv(in.gpr[0]);
}

static std::uint64_t Call43(const Inputs& in) {
    return _ZNK7WebCore14DocumentLoader11frameLoaderEv(in.gpr[0]);
}

static std::uint64_t Call44(const Inputs& in) {
    return _ZNK7WebCore14DocumentLoader18mainResourceLoaderEv(in.gpr[0]);
}

static std::uint64_t Call45(const Inputs& in) {
    return _ZNK7WebCore14ResourceLoader11frameLoaderEv(in.gpr[0]);
}

static std::uint64_t Call46(const Inputs& in) {
    return _ZNK7WebCore14StaticNodeList4itemEj(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call47(const Inputs& in) {
    return _ZNK7WebCore15ActiveDOMObject16isContextStoppedEv(in.gpr[0]);
}

static std::uint64_t Call48(const Inputs& in) {
    return _ZNK7WebCore15ActiveDOMObject20isAllowedToRunScriptEv(in.gpr[0]);
}

static std::uint64_t Call49(const Inputs& in) {
    return _ZNK7WebCore15FocusController18focusedOrMainFrameEv(in.gpr[0]);
}

static std::uint64_t Call50(const Inputs& in) {
    return _ZNK7WebCore15SecurityContext14securityOriginEv(in.gpr[0]);
}

static std::uint64_t Call51(const Inputs& in) {
    return _ZNK7WebCore16DOMGuardedObject12globalObjectEv(in.gpr[0]);
}

static std::uint64_t Call52(const Inputs& in) {
    return _ZNK7WebCore16DOMGuardedObject13guardedObjectEv(in.gpr[0]);
}

static std::uint64_t Call53(const Inputs& in) {
    return _ZNK7WebCore16DOMGuardedObject7isEmptyEv(in.gpr[0]);
}

static std::uint64_t Call54(const Inputs& in) {
    return _ZNK7WebCore16HTMLMediaElement12isFullscreenEv(in.gpr[0]);
}

static std::uint64_t Call55(const Inputs& in) {
    return _ZNK7WebCore16HTMLMediaElement33lastMediaResourceLoaderForTestingEv(in.gpr[0]);
}

static std::uint64_t Call56(const Inputs& in) {
    return _ZNK7WebCore16HTMLStyleElement8disabledEv(in.gpr[0]);
}

static std::uint64_t Call57(const Inputs& in) {
    return _ZNK7WebCore17ActiveDOMCallback17canInvokeCallbackEv(in.gpr[0]);
}

static std::uint64_t Call58(const Inputs& in) {
    return _ZNK7WebCore17ActiveDOMCallback25activeDOMObjectAreStoppedEv(in.gpr[0]);
}

static std::uint64_t Call59(const Inputs& in) {
    return _ZNK7WebCore17ActiveDOMCallback28activeDOMObjectsAreSuspendedEv(in.gpr[0]);
}

static std::uint64_t Call60(const Inputs& in) {
    return _ZNK7WebCore21FormAssociatedElement4formEv(in.gpr[0]);
}

static std::uint64_t Call61(const Inputs& in) {
    return _ZNK7WebCore21HTMLFrameOwnerElement13contentWindowEv(in.gpr[0]);
}

static std::uint64_t Call62(const Inputs& in) {
    return _ZNK7WebCore21HTMLFrameOwnerElement15contentDocumentEv(in.gpr[0]);
}

static std::uint64_t Call63(const Inputs& in) {
    return _ZNK7WebCore26NetscapePlugInStreamLoader6isDoneEv(in.gpr[0]);
}

static std::uint64_t Call64(const Inputs& in) {
    return _ZNK7WebCore28InspectorFrontendClientLocal13inspectedPageEv(in.gpr[0]);
}

static std::uint64_t Call65(const Inputs& in) {
    return _ZNK7WebCore4Node10shadowHostEv(in.gpr[0]);
}

static std::uint64_t Call66(const Inputs& in) {
    return _ZNK7WebCore4Node9renderBoxEv(in.gpr[0]);
}

static std::uint64_t Call67(const Inputs& in) {
    return _ZNK7WebCore4Page16hasSeenAnyPluginEv(in.gpr[0]);
}

static std::uint64_t Call68(const Inputs& in) {
    return _ZNK7WebCore4Page19disabledAdaptationsEv(in.gpr[0]);
}

static std::uint64_t Call69(const Inputs& in) {
    return _ZNK7WebCore4Page28remoteInspectionNameOverrideEv(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call70(const Inputs& in) {
    return _ZNK7WebCore5Frame15contentRendererEv(in.gpr[0]);
}

static std::uint64_t Call71(const Inputs& in) {
    return _ZNK7WebCore5Frame6windowEv(in.gpr[0]);
}

static std::uint64_t Call72(const Inputs& in) {
    return _ZNK7WebCore6Editor7Command15isTextInsertionEv(in.gpr[0]);
}

static std::uint64_t Call73(const Inputs& in) {
    return _ZNK7WebCore6Widget6parentEv(in.gpr[0]);
}

static std::uint64_t Call74(const Inputs& in) {
    return _ZNK7WebCore7Element10shadowRootEv(in.gpr[0]);
}

static std::uint64_t Call75(const Inputs& in) {
    return _ZNK7WebCore7Element18afterPseudoElementEv(in.gpr[0]);
}

static std::uint64_t Call76(const Inputs& in) {
    return _ZNK7WebCore7Element19beforePseudoElementEv(in.gpr[0]);
}

static std::uint64_t Call77(const Inputs& in) {
    return _ZNK7WebCore8Document11windowProxyEv(in.gpr[0]);
}

static std::uint64_t Call78(const Inputs& in) {
    return _ZNK7WebCore8Document21haveStylesheetsLoadedEv(in.gpr[0]);
}

static std::uint64_t Call79(const Inputs& in) {
    return _ZNK7WebCore8Document4viewEv(in.gpr[0]);
}

static std::uint64_t Call80(const Inputs& in) {
    return _ZNK7WebCore8FileList4itemEj(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call81(const Inputs& in) {
    return _ZNK7WebCore9FrameTree6parentEv(in.gpr[0]);
}

static std::uint64_t Call82(const Inputs& in) {
    return _ZNK7WebCore9FrameView10renderViewEv(in.gpr[0]);
}

static std::uint64_t Call83(const Inputs& in) {
    return _ZNK9Inspector14ConsoleMessage13argumentCountEv(in.gpr[0]);
}

static std::uint64_t Call84(const Inputs& in) {
    return _ZNK9Inspector15ScriptArguments12globalObjectEv(in.gpr[0]);
}

static std::uint64_t Call85(const Inputs& in) {
    return _ZNK9Inspector18InjectedScriptBase10hasNoValueEv(in.gpr[0]);
}

static std::uint64_t Call86(const Inputs& in) {
    return _ZNK9Inspector19InspectorAuditAgent14hasActiveAuditEv(in.gpr[0]);
}

struct Seed {
    std::uint64_t seed;
    std::uint64_t digest;
};

struct Case {
    std::uint64_t (*call)(const Inputs&);
    int kind;
    std::vector<std::uint64_t> hints;
    std::vector<Seed> seeds;
};

static const Case Cases[] = {
    {&Call0, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x1b9350bac85a7352ull}, {3, 0x309ab410000514ebull}, {5, 0xb7aa0c52af89b835ull}, {7, 0xfcc820f22e70544dull}, {8, 0x58c9afe96d96a67aull}, {10, 0x21eeba9bda505aa5ull}, {11, 0x37b5c3d241843856ull}, {12, 0x7eed6018083740adull}, {13, 0x268db42cd22d48a3ull}, {14, 0x29f501a0bbec3530ull}, {15, 0x4d92ba8fb8d315c8ull}, {17, 0x404c883a5a1cf730ull}}},
    {&Call1, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x6543d7c2288584b5ull}, {5, 0x982a880d9ce014d7ull}, {8, 0x8d2c91a29a8eee58ull}, {10, 0xed0448b2b6abd99cull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x716f2ba1a54e6c61ull}, {13, 0xbc5f8f51b407ee4full}, {15, 0xd9dd60440fc13bc0ull}, {16, 0x4e164eb0c7117bcfull}, {17, 0x80a799c8ecbea0e1ull}}},
    {&Call2, 4, {}, {{1, 0x6b3d82ff5a211c1full}, {2, 0xabb7a721f4d963aeull}, {3, 0x97d49b249efb9543ull}, {4, 0x070a1c34a9a936c1ull}, {5, 0x1b54cf7f587577e2ull}, {6, 0xb5cb0228a8103e85ull}, {7, 0x486357f2f0c77e26ull}, {8, 0xe678e1d11b12de48ull}, {9, 0x10b68ba894c94c28ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {16, 0xc28df685c14e6ca6ull}}},
    {&Call3, 4, {}, {{1, 0x6b3d82ff5a211c1full}, {2, 0xabb7a721f4d963aeull}, {3, 0x97d49b249efb9543ull}, {4, 0x070a1c34a9a936c1ull}, {5, 0x1b54cf7f587577e2ull}, {6, 0xb5cb0228a8103e85ull}, {7, 0x486357f2f0c77e26ull}, {8, 0xe678e1d11b12de48ull}, {9, 0x10b68ba894c94c28ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {16, 0xc28df685c14e6ca6ull}}},
    {&Call4, 4, {}, {{2, 0x74f430384742f2f5ull}, {3, 0x16ae5460c78980e5ull}, {4, 0x58f3cf4670301d19ull}, {5, 0xe96e18ef1eae643full}, {6, 0xd52e44024ddf7f80ull}, {7, 0x04c13133167f59d6ull}, {8, 0x34e5bd2e8d624875ull}, {10, 0x5d181fd63970984bull}, {13, 0x0da4bb5a14af9a9full}, {16, 0xc28df685c14e6ca6ull}, {17, 0x59228f991377c095ull}, {18, 0x0e58c6f13695ba11ull}}},
    {&Call5, 4, {}, {{2, 0x74f430384742f2f5ull}, {3, 0x16ae5460c78980e5ull}, {4, 0x58f3cf4670301d19ull}, {5, 0xe96e18ef1eae643full}, {6, 0xd52e44024ddf7f80ull}, {7, 0x04c13133167f59d6ull}, {8, 0x34e5bd2e8d624875ull}, {10, 0x5d181fd63970984bull}, {13, 0x0da4bb5a14af9a9full}, {16, 0xc28df685c14e6ca6ull}, {17, 0x59228f991377c095ull}, {18, 0x0e58c6f13695ba11ull}}},
    {&Call6, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x51771b5bfb5c0b10ull}, {2, 0xaf517ae4a5766b22ull}, {3, 0x23ec20c0eb6e57dbull}, {5, 0x32a98ccd552ce48aull}, {6, 0x69c1cbf411496279ull}, {7, 0x16efb3685afe3180ull}, {8, 0x6dc0e243c21cdc12ull}, {10, 0x07c11544255fda99ull}, {11, 0xcb5ccce18adf8376ull}, {12, 0x344be786e5dd011aull}, {13, 0x50f5a72326f5538bull}}},
    {&Call7, 0, {}, {{1, 0xf99d95413ab08d57ull}, {2, 0x02913ed5576e9cd2ull}, {3, 0x182fe4a922bed86full}, {4, 0x3626dfc0b021225dull}, {5, 0x3b16030951e7e746ull}, {6, 0x196f1b60abeb79bcull}, {7, 0x980f738dcb0119e8ull}, {8, 0x4fa89a10b156a862ull}, {9, 0x9170fbeb17a1cb1aull}, {10, 0x6b60f641dc167798ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x9d741650785f59fdull}}},
    {&Call8, 3, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{1, 0xca07ba50326e1977ull}, {2, 0xeed6501435775b64ull}, {3, 0xfecef5c2035a68a1ull}, {5, 0xdb91e0fb4b3347cfull}, {6, 0x422bdc74c5ce233cull}, {7, 0x803e74d591322096ull}, {8, 0x7284427b0adbd7c3ull}, {10, 0x527bd0f81dc6e143ull}, {11, 0x4b6196fa38d2689aull}, {12, 0x3782c95e227273a2ull}, {13, 0x31eef8132e3df08full}, {14, 0x34f49c8affb8d7daull}}},
    {&Call9, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x9b254db482d1eff2ull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xa3fc76f865ff286dull}, {6, 0xab52f098410047c9ull}, {7, 0xc12a15d6a4bdebf6ull}, {8, 0xf5f6fc5c22ac34e7ull}, {10, 0xb0075978d7d67dffull}, {11, 0x6d37ebb3c1910773ull}, {12, 0x2d4ccd318f8d5cf3ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call10, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134d976e0633bbdull}, {5, 0xfcbc485615d75c66ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d2200bde302cc47ull}, {17, 0xa094126af94ac9e7ull}}},
    {&Call11, 4, {0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0xfffffffffffeull, 0xffffffffffffull, 0x1000000000000ull, 0x3fef000000000001ull, 0x3fff000000000001ull, 0x4007000000000001ull, 0x400f000000000001ull, 0x4013000000000001ull, 0x4017000000000001ull, 0x401b000000000001ull, 0xbfef000000000001ull, 0xbfff000000000001ull, 0xffff000000000000ull, 0xffff000000000001ull}, {{1, 0xcc4553dc5610f0e6ull}, {2, 0xaf48426f400fa6f8ull}, {3, 0xc44831fa3ca72f4bull}, {6, 0x69c1d2f41149de12ull}, {7, 0xb12e072e4919a5e3ull}, {8, 0x44afab89fa3cc112ull}, {10, 0x091f526d6de63a2cull}, {11, 0xcacb8120ff8b12b1ull}, {13, 0x788bbc092431c2a2ull}, {16, 0xc301d0fb7371472bull}, {17, 0x38e05f07711acc8eull}, {19, 0x51ed3a678e406971ull}}},
    {&Call12, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x8829ba324bc2780aull}, {3, 0xc4f630fa07bfcf19ull}, {4, 0xfcb35aa118c6dfc9ull}, {5, 0xb4b0ec52b75ec695ull}, {7, 0x81aa9cc551cf0a3bull}, {8, 0x4f6d0fa6b204841aull}, {9, 0xf495fe5b234bb316ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8fc05b73b9d52a55ull}, {13, 0x688cd42ead663b43ull}, {16, 0xc61e43438ac50ca6ull}, {18, 0x690a968b7c6d6086ull}}},
    {&Call13, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0xaef70503a71c5b64ull}, {1, 0xb478cd5c54c68756ull}, {2, 0xb44a0b0612191f9cull}, {3, 0x8c56f58cde2d55f5ull}, {5, 0xcf3db11efd508870ull}, {6, 0xd049eb90cc488c2bull}, {7, 0x21a43bf0aa1f3293ull}, {8, 0x30a52de7830c716bull}, {10, 0x664cbeb58faa29fbull}, {11, 0x84e802a3bd3b1893ull}, {12, 0xa10e3c5ec33a0d49ull}, {13, 0x4bc9743ef12251e8ull}}},
    {&Call14, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x98ee61360c2b2f96ull}, {1, 0xfc02c6168503bd88ull}, {2, 0x4c97a17ffe6bb122ull}, {3, 0x434442dedbbca91full}, {5, 0xce7f9df0db69b6d7ull}, {6, 0x1c4a041e21d09d41ull}, {7, 0xc9b6bdd771552380ull}, {8, 0x822fed6eaba36b94ull}, {10, 0xe645427f6f089a9aull}, {11, 0xda27fd053c2abcd8ull}, {12, 0x8d974bbf59531ef5ull}, {13, 0x20c18fe1453152f6ull}}},
    {&Call15, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0xa77fc49a67e8f276ull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0x4b20e08c1c12bde9ull}, {5, 0x67f70c46572d586bull}, {6, 0x74e5fb8a4fd23f0eull}, {7, 0x443a887c533cf3e6ull}, {8, 0x9f78d93c89de06cfull}, {10, 0x240da4f6442f2d95ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x1adab58cfa381d4dull}, {13, 0x749b8305b47d7f83ull}}},
    {&Call16, 3, {0x7full, 0x80ull, 0x81ull, 0x1fffull, 0x2000ull, 0x2001ull, 0xffffffffffffdfffull, 0xffffffffffffff7full}, {{0, 0xc9f182515315673cull}, {1, 0xb306c7c3f582b41cull}, {2, 0x0c602441bfdb8799ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x0045b46d547b1534ull}, {6, 0x69c082f411411902ull}, {7, 0x5076ee54e5759093ull}, {8, 0x53274c7ec8545d0bull}, {10, 0x8387527b218a0ac1ull}, {11, 0x69974a99f7873c54ull}, {12, 0x344d2786e5ed60daull}, {13, 0xacd0ef6a1dfb4bedull}}},
    {&Call17, 0, {}, {{1, 0x7b969170868a9312ull}, {2, 0x75097bc8f69b9a73ull}, {3, 0xe5aff77ea84ccc6aull}, {4, 0x3626dfc0b021225dull}, {5, 0xe32911c59ae2006cull}, {6, 0x5dcd2d292ce6de1full}, {7, 0x62216d5edb6c6c02ull}, {8, 0x57fa4832006c7ebfull}, {9, 0x859b61f3a59be975ull}, {10, 0x392e4b001eca13e8ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x11673a0a58824351ull}}},
    {&Call18, 0, {}, {{1, 0xf77f3247a71be692ull}, {2, 0xff5f342bd1a0a0c8ull}, {4, 0x3626dfc0b021225dull}, {8, 0xcbe0b616de519b2eull}, {10, 0x1804e1ea26d1f19dull}, {11, 0xf04d79d063bb7e4aull}, {19, 0x50d3907e9b0ddfbdull}, {20, 0x02cfee15eb0f205dull}, {21, 0x4e91ef0c4c7d5f18ull}, {29, 0x99f57efa5f4f34e6ull}, {30, 0x27d404ce011b2e41ull}, {32, 0xdf5dbee9b7e17a18ull}}},
    {&Call19, 0, {}, {{1, 0xdec6328c6fd6dc7aull}, {2, 0x7a3f79bc7c0281fbull}, {4, 0x3626dfc0b021225dull}, {8, 0x32aa7b210fb6d448ull}, {10, 0x1ea5ecff4624dc63ull}, {11, 0xf04d79d063bb7e4aull}, {19, 0xc14029c21f0c3f1dull}, {20, 0x9be496bc304e1e35ull}, {21, 0x4e91ef0c4c7d5f18ull}, {29, 0x610f5eeb6b05b4b7ull}, {30, 0x27d404ce011b2e41ull}, {32, 0x9e142549923ae463ull}}},
    {&Call20, 4, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{1, 0x1170b1a014cf98baull}, {2, 0x634043f7c0f4b842ull}, {3, 0x16573692c4287f0eull}, {5, 0xd69141295e15337aull}, {6, 0x77a6e383ce412e1aull}, {7, 0xd3845fc27b5621baull}, {8, 0x320cd17df4b4353full}, {10, 0x7f641ae67503da5bull}, {11, 0xab7f362f6938a618ull}, {12, 0x1bac9570fd567997ull}, {13, 0xf257e97346c57e1aull}, {14, 0xd3c069ac487ec448ull}}},
    {&Call21, 4, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{1, 0x1170b1a014cf98baull}, {2, 0x634043f7c0f4b842ull}, {3, 0x16573692c4287f0eull}, {5, 0xd69141295e15337aull}, {6, 0x77a6e383ce412e1aull}, {7, 0xd3845fc27b5621baull}, {8, 0x320cd17df4b4353full}, {10, 0x7f641ae67503da5bull}, {11, 0xab7f362f6938a618ull}, {12, 0x1bac9570fd567997ull}, {13, 0xf257e97346c57e1aull}, {14, 0xd3c069ac487ec448ull}}},
    {&Call22, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{3, 0xc44833fa3ca73bf1ull}, {7, 0x0b5c0fe3462fd553ull}, {8, 0xf663450b3e0bec21ull}, {10, 0x118a8fb64867e736ull}, {13, 0xa54e742163faf664ull}, {14, 0xa82fa9d1cee0ec9full}, {19, 0x4613adf4386e71e7ull}, {20, 0xc73de2afb7e6b623ull}, {22, 0xcca394e5b1de5aa1ull}, {23, 0x113d38f3cc66155full}, {24, 0x90e6aa867f7ec3ddull}, {27, 0xefdc5497a0a980caull}}},
    {&Call23, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{3, 0xc44833fa3ca73bf1ull}, {7, 0x0b5c0fe3462fd553ull}, {8, 0xf663450b3e0bec21ull}, {10, 0x118a8fb64867e736ull}, {13, 0xa54e742163faf664ull}, {14, 0xa82fa9d1cee0ec9full}, {19, 0x4613adf4386e71e7ull}, {20, 0xc73de2afb7e6b623ull}, {22, 0xcca394e5b1de5aa1ull}, {23, 0x113d38f3cc66155full}, {24, 0x90e6aa867f7ec3ddull}, {27, 0xefdc5497a0a980caull}}},
    {&Call24, 1, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x6ull, 0x7ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0xefull, 0xf0ull, 0xf1ull, 0xffffffffffffff0full, 0xfffffffffffffff8ull, 0xfffffffffffffffdull}, {{0, 0xdf2a6fa2cad1003full}, {1, 0xdb96464eb6699ec1ull}, {2, 0xb4ac200d82de4b6cull}, {3, 0x8abd213294c9ff03ull}, {5, 0x0f64230e83a68d86ull}, {6, 0x6b9f02e2d13b88beull}, {7, 0xeb9c7da668994bddull}, {8, 0xc16907b431a4fdd1ull}, {10, 0x183d4b7c13a78ac8ull}, {11, 0x8acaa11ce5cf7c63ull}, {12, 0xb7d1883fba7534e0ull}, {13, 0x5341432b9aa40a56ull}}},
    {&Call25, 1, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x6ull, 0x7ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0xefull, 0xf0ull, 0xf1ull, 0xffffffffffffff0full, 0xfffffffffffffff8ull, 0xfffffffffffffffdull}, {{0, 0xdf2a6fa2cad1003full}, {1, 0xdb96464eb6699ec1ull}, {2, 0xb4ac200d82de4b6cull}, {3, 0x8abd213294c9ff03ull}, {5, 0x0f64230e83a68d86ull}, {6, 0x6b9f02e2d13b88beull}, {7, 0xeb9c7da668994bddull}, {8, 0xc16907b431a4fdd1ull}, {10, 0x183d4b7c13a78ac8ull}, {11, 0x8acaa11ce5cf7c63ull}, {12, 0xb7d1883fba7534e0ull}, {13, 0x5341432b9aa40a56ull}}},
    {&Call26, 3, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{3, 0x0a5f60d3a438d3d2ull}, {13, 0x02cc6aec15eff501ull}, {23, 0x8a733be0836d75bdull}, {24, 0xb23f5c477f61776eull}, {28, 0x32b76be94de708c3ull}, {29, 0x185d8f426c09a67bull}, {31, 0x0a6af74c1942c1a2ull}, {37, 0x9d655673c0fa2221ull}, {46, 0x058e8d153931c7a8ull}, {52, 0xeb7d159ebad4b666ull}, {53, 0xa10af5334ca99d08ull}, {55, 0x2f074f946d4d1bb7ull}}},
    {&Call27, 3, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{3, 0x0a5f60d3a438d3d2ull}, {13, 0x02cc6aec15eff501ull}, {23, 0x8a733be0836d75bdull}, {24, 0xb23f5c477f61776eull}, {28, 0x32b76be94de708c3ull}, {29, 0x185d8f426c09a67bull}, {31, 0x0a6af74c1942c1a2ull}, {37, 0x9d655673c0fa2221ull}, {46, 0x058e8d153931c7a8ull}, {52, 0xeb7d159ebad4b666ull}, {53, 0xa10af5334ca99d08ull}, {55, 0x2f074f946d4d1bb7ull}}},
    {&Call28, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0xaf459799b1ab9710ull}, {1, 0x683526184e1b5121ull}, {2, 0x75f1dbd4e74f52e0ull}, {3, 0x4f12a4e7f1f6fd86ull}, {5, 0xa93f8a81136a61a3ull}, {6, 0xfaf0c5f79de2ee5eull}, {7, 0x0f3bc46608e5d3c8ull}, {8, 0x6439226884d5608bull}, {10, 0xfd9cbd4d71b5b231ull}, {11, 0xe241b4526ad52899ull}, {12, 0x7c9bf6509348f9f8ull}, {13, 0x78a52f10c5cadd94ull}}},
    {&Call29, 3, {}, {{2, 0x3347278c34486f22ull}, {5, 0xb7ac285453649f06ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d3f902168b24344ull}, {20, 0x53ea4fe272646aafull}, {26, 0xe3cde125456cc943ull}, {28, 0xcecf8b45b68b5050ull}, {35, 0xc6efb2b022c4bf14ull}, {44, 0x68c5891a8d7a0b91ull}, {47, 0x6aa7c509dadceca0ull}, {48, 0x426bd33214f177d5ull}, {50, 0x6ad3da525d89931bull}}},
    {&Call30, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x36ab639790dda9cbull}, {1, 0x1b19e3608d09c08dull}, {2, 0x82f8975d24748fa3ull}, {3, 0x3959f4c5ef640b55ull}, {5, 0xc31283701d0c48d0ull}, {6, 0x044b1f864bb5c50aull}, {7, 0x29dbb3f6fd36212dull}, {8, 0x451f534adbdeb9d3ull}, {10, 0xb0075978d7d67dffull}, {11, 0x06fc4cac376d971eull}, {12, 0x9e443be3f697e337ull}, {13, 0x8735ffbcf75a3c74ull}}},
    {&Call31, 4, {}, {{1, 0xeea36a2d0186291aull}, {2, 0xe7204f8a8985592aull}, {3, 0xf134db76e0632817ull}, {4, 0x65dfdfc22d5aa25dull}, {5, 0xfc60a0560c9c62feull}, {6, 0xd8709957fa646074ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x40647e8e52953b98ull}, {9, 0xa16162d831868d06ull}, {10, 0xcdf6aaab7262ebe2ull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x7f82f1d7fbe55b49ull}}},
    {&Call32, 3, {0x0ull, 0x1ull, 0x2ull, 0x3ull, 0x4ull, 0xfffffffffffffffcull, 0xffffffffffffffffull}, {{1, 0xb24f6098eb6d26feull}, {5, 0x36128d50011e0316ull}, {6, 0x22584a0ddbd041ebull}, {7, 0x25ba07f8548a3737ull}, {8, 0x562ae1af70b0f366ull}, {11, 0xe03e665162f90381ull}, {12, 0xa5bb17b98c0b45a8ull}, {13, 0x0cda4f73df539a20ull}, {14, 0x4d5742683cdb0253ull}, {15, 0x908c165e39eb8ef5ull}, {17, 0x94e4e962be72c7a4ull}, {19, 0xda8374367caf24b9ull}}},
    {&Call33, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcbc485615d75c66ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7990216a7dd144ull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05800a6fd7702d7full}, {19, 0x589b081005db3f74ull}}},
    {&Call34, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{0, 0xd6a075249554b590ull}, {1, 0xa06e9f3038e3d84cull}, {2, 0x30ccc584936d9648ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x658363175f1ee8dbull}, {6, 0xae675f252380c39bull}, {7, 0xe304f9e405c505f1ull}, {8, 0x5a168d0e5a64eaf9ull}, {10, 0x118a8e504867ef60ull}, {11, 0xb6ab059d5d9a80deull}, {12, 0x344be786e5dd011aull}, {13, 0xa54e722163fac2c2ull}}},
    {&Call35, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829ba324bc2780aull}, {2, 0x83c9985d2b5f98d4ull}, {3, 0xc53330fa09ec6819ull}, {5, 0xb573ec52bfbb9f95ull}, {7, 0x81809bc550505274ull}, {12, 0x344be786e5dd011aull}, {15, 0xc2b41d324dd8f725ull}, {16, 0xc6d343438df0bfa6ull}, {17, 0x123956b39b04cbb9ull}, {19, 0x2e3729e37d75b196ull}, {20, 0xad33678d910de2bbull}}},
    {&Call36, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x6d0b1078ff5115caull}, {5, 0x2d5c12164df9238cull}, {6, 0x5ac37f33929d2bbaull}, {7, 0x6d464387c2f0447bull}, {8, 0xb634b21da90ade78ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x52b2f036a177514dull}, {13, 0xcea1970d2a2a7319ull}, {14, 0x3efd9ee76bffb576ull}, {15, 0xcf4e8225e043c692ull}, {17, 0x7037b271c97b69e3ull}, {19, 0x2e3729e37d75b196ull}}},
    {&Call37, 3, {}, {{2, 0x3347278c34486f22ull}, {9, 0xa1b75dd836a55b99ull}, {15, 0x7d2200bde302cc47ull}, {25, 0xd0421b7564381daaull}, {26, 0xe3cde125456cc943ull}, {32, 0x4a3fd7801cc0847cull}, {37, 0x6da41912b171099bull}, {38, 0x36de9c8ea7a52b07ull}, {40, 0x05ab94c4c3b96707ull}, {53, 0x786a21c0a8f3d6f4ull}, {62, 0xb5a4519e096e38f2ull}, {71, 0x34b863c38b8715fdull}}},
    {&Call38, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0xe718278a88b87522ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x4081a68e55e56e90ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0xa6169617d89767e4ull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05c7b26fd5d7d307ull}, {19, 0x589b081005db3f74ull}}},
    {&Call39, 3, {0x2ull, 0x3ull, 0x4ull, 0x5ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffffdull, 0xfffffffeull, 0xffffffffull, 0xffffffff00000001ull, 0xfffffffffffffffbull}, {{0, 0x376a90353c024812ull}, {1, 0x764fb9bcfdd7f22eull}, {2, 0xa6c960ba0c319e80ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x6af6f685b02f81e9ull}, {6, 0x69c1cbf411496279ull}, {7, 0xbf9b11a01e3850abull}, {8, 0x06c9dcc9b8436aa9ull}, {10, 0x288901ed337c5cffull}, {11, 0x93fb1b2f942ae45bull}, {12, 0x344be786e5dd011aull}, {13, 0x603a3e87796f35c2ull}}},
    {&Call40, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{2, 0x2e33e97f569b5951ull}, {5, 0x3148d9b74ac3d73eull}, {11, 0x2c6a2441d3805d1full}, {12, 0xd60bd12cbf7e9a30ull}, {17, 0x6c861caa46c9151cull}, {20, 0xcf5fba91bf9ae0e6ull}, {26, 0xb44c2cf69dafea2bull}, {28, 0x644f9bb41e5f2b7dull}, {30, 0x5a65491ae2005e54ull}, {33, 0xecd2808ba13054f2ull}, {35, 0xd222f3b00798f9e7ull}, {47, 0xbd48fb010dff31f7ull}}},
    {&Call41, 4, {0x7ull, 0x8ull, 0x9ull, 0xfffffffffffffff7ull}, {{6, 0x69c1cbf411496279ull}, {7, 0x99b5a2ae61f6fc60ull}, {8, 0x7e21d59560ec4796ull}, {10, 0x361835d0f17e75bcull}, {11, 0x893e1561b39398f0ull}, {13, 0x73acf68f2c1c4081ull}, {19, 0xb9cf40ceff2276cfull}, {20, 0xae7c265f1126245cull}, {21, 0x4a901f0c6c0ecf88ull}, {24, 0xd155d284700107d5ull}, {31, 0x51b29c4df59a52d1ull}, {33, 0xdd01d83644784336ull}}},
    {&Call42, 3, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x363b9035348f6d12ull}, {1, 0x51771a5bfb5c02e3ull}, {3, 0xc44830fa3ca71918ull}, {6, 0x695bcbf41447b479ull}, {7, 0x1792b368520d3880ull}, {8, 0xa5a32fe5057d4b5eull}, {10, 0x6188dbd3c5545091ull}, {12, 0x345be786e542511aull}, {13, 0x50f5a82326f52d34ull}, {14, 0xf389095188e76ee1ull}, {16, 0x65c37e015cd086aaull}, {17, 0x88164a68c20514f0ull}}},
    {&Call43, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b081005db3f74ull}}},
    {&Call44, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d2200bde302cc47ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x0538aa6fd332669full}}},
    {&Call45, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d21f6bde31d0d49ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}}},
    {&Call46, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7992216a7dc2a2ull}}},
    {&Call47, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {3, 0xc44830fa3ca71918ull}, {6, 0x69c1caf411496baaull}, {7, 0x16efb2685afe3a53ull}, {8, 0xa64f2fe51bf1075eull}, {12, 0x344be886e5dd7ae9ull}, {14, 0xf389095188e76ee1ull}, {17, 0x88164a68c20514f0ull}, {19, 0x17cf610697616bdcull}, {22, 0xc083cdbd5d10ea09ull}, {23, 0x2af0c1a81c25ea15ull}, {24, 0xd155d38470010962ull}}},
    {&Call48, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {3, 0xc44831fa3ca72f4bull}, {6, 0x69c1cbf411496279ull}, {7, 0x825f9cc54f339d3bull}, {8, 0x4dde10a6a83678c9ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {14, 0xb55f997be6f10fe3ull}, {19, 0x2e3729e37d75b196ull}, {22, 0xb578cc1429c74a6dull}, {23, 0x7429fb8e3d54eb54ull}, {24, 0xd155d284700107d5ull}}},
    {&Call49, 4, {}, {{1, 0xeeed1a2d02727f0aull}, {3, 0x2cb62970029e8dadull}, {5, 0xfcf6785617243cf6ull}, {6, 0x2b45a9506dae90a4ull}, {8, 0x40647e8e52953b98ull}, {9, 0xa16165d831868fe1ull}, {10, 0x1256a0fa8c6eac22ull}, {11, 0x438921d5fc6b3ad2ull}, {15, 0xd49920b8a8e65fe7ull}, {17, 0x0538aa6fd332669full}, {18, 0xad823d8efa290e6cull}, {19, 0x589b021005db6526ull}}},
    {&Call50, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcbc485615d75c66ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b081005db3f74ull}}},
    {&Call51, 4, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0xfffffffffffffffcull}, {{0, 0x376a90353c024812ull}, {1, 0xed874c2f9b119147ull}, {3, 0x0dd871fc4ebea58aull}, {7, 0xcaeec0020176193full}, {8, 0xc3cda57a8afbc6deull}, {12, 0x344be786e5dd011aull}, {20, 0xe425a4789252cdabull}, {21, 0x4e91ef0c4c7d5f18ull}, {22, 0x1c3cbdaba71b9d28ull}, {24, 0x90d4f2867e8ae835ull}, {26, 0xdd6ea98eb978fa75ull}, {30, 0x31f76320dfb67f51ull}}},
    {&Call52, 4, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0xfffffffffffffffcull}, {{1, 0xed874c2f9b119147ull}, {3, 0xc44831fa3ca72f4bull}, {7, 0xcaeec0020176193full}, {8, 0xdca7a79a8c56f5fdull}, {10, 0x73e94301d5c319c9ull}, {13, 0xb5b5680a7d319512ull}, {14, 0x9b3fa376d0accf39ull}, {17, 0x75074e26d4781092ull}, {19, 0x433bad3d7608b66dull}, {20, 0xe425a4789252cdabull}, {21, 0x4a0eaf0c67c73ed8ull}, {22, 0x1c3cbbaba71bb056ull}}},
    {&Call53, 1, {0x0ull, 0x1ull, 0x2ull, 0x3ull, 0x4ull, 0xfffffffffffffffcull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{1, 0xf2a099009c4114adull}, {3, 0xc44830fa3ca71918ull}, {5, 0x0ed10a10070257a3ull}, {7, 0xc62a38d40b4d269bull}, {8, 0x6e4e2d59d097ab98ull}, {10, 0xccc84e00884e269bull}, {13, 0xa8ef72ef53e5a135ull}, {14, 0x0c9e7bd57fe5af0aull}, {17, 0x48ab94fabafffe01ull}, {19, 0xdb3dc4be0c034897ull}, {20, 0x52241646e665e1d2ull}, {21, 0x4e91ef0c4c7d5f18ull}}},
    {&Call54, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{8, 0xa64f2ee51bf101adull}, {27, 0xefdc5597a0a98979ull}, {39, 0xa8f7437de2d0a7c4ull}, {44, 0xcd40d6cdb9ad58faull}, {92, 0x2f1770d98f78794aull}, {119, 0x5f5dcc19a7e73566ull}, {125, 0x5c28791c9d848543ull}, {140, 0x00c59fffc55e2273ull}, {162, 0x0ff977c9722f0082ull}, {195, 0x5d642b3081945612ull}, {215, 0xf9fa796a7fa07d15ull}, {218, 0x4966f75fc8a4f40eull}}},
    {&Call55, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0xe7204f8a8985592aull}, {4, 0xb626dfc46ba1225bull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x4081a68e55e56e90ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0x42f5e1d5c888f013ull}, {12, 0x1022b02002240da4ull}, {13, 0x0903d1d44f106329ull}, {15, 0xd4a548b8a96e47ffull}}},
    {&Call56, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x36739035362f9512ull}, {1, 0x8829ba324bc2780aull}, {3, 0xc53f30fa0973d419ull}, {8, 0x4dde10a6a83678c9ull}, {11, 0x90315a73491d2086ull}, {13, 0x67d1d42ed94c7843ull}, {17, 0x123d55b39b3fe5f6ull}, {19, 0x2fee29e370c7bc96ull}, {20, 0xad35688d913102e8ull}, {21, 0x4e91ef0c4c7d5f18ull}, {23, 0x7384fb8e07250454ull}, {24, 0xd02ad28479e516d5ull}}},
    {&Call57, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {3, 0xc44831fa3ca72f4bull}, {6, 0x69c1cbf411496279ull}, {7, 0x825f9cc54f339d3bull}, {8, 0x4dde10a6a83678c9ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {14, 0xb55f997be6f10fe3ull}, {19, 0x2e3729e37d75b196ull}, {22, 0xb578cc1429c74a6dull}, {23, 0x7429fb8e3d54eb54ull}, {24, 0xd155d284700107d5ull}}},
    {&Call58, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {3, 0xc44830fa3ca71918ull}, {6, 0x69c1caf411496baaull}, {7, 0x16efb2685afe3a53ull}, {8, 0xa64f2fe51bf1075eull}, {12, 0x344be886e5dd7ae9ull}, {14, 0xf389095188e76ee1ull}, {17, 0x88164a68c20514f0ull}, {19, 0x17cf610697616bdcull}, {22, 0xc083cdbd5d10ea09ull}, {23, 0x2af0c1a81c25ea15ull}, {24, 0xd155d38470010962ull}}},
    {&Call59, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x363b9135348f67cdull}, {3, 0xc44831fa3ca72f4bull}, {6, 0x695bcaf41447bdaaull}, {7, 0x825f9bc54f33a774ull}, {8, 0x4f0a0fa6b094bd1aull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x345be886e5422ae9ull}, {14, 0xb459987be89c53acull}, {19, 0x2e3729e37d75b196ull}, {22, 0xb578cc1429c74a6dull}, {23, 0x7429fb8e3d54eb54ull}, {24, 0xd155d284700107d5ull}}},
    {&Call60, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0x2ce6a97000b7642dull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcbc485615d75c66ull}, {6, 0xd870a257fa65a8c1ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7990216a7dd144ull}, {13, 0x0903d1d44f106329ull}, {15, 0x229afd0a39aa6d5eull}}},
    {&Call61, 4, {}, {{2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0x42f5e1d5c888f013ull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b081005db3f74ull}}},
    {&Call62, 4, {}, {{2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05c7b26fd5d7d307ull}, {19, 0x589b081005db3f74ull}}},
    {&Call63, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{1, 0x51771b5bfb5c0b10ull}, {2, 0xaf517be4a57662d1ull}, {5, 0x32a98ccd552ce48aull}, {6, 0x69c1cbf411496279ull}, {8, 0xa64f2fe51bf1075eull}, {10, 0x6188dad3c5545ae2ull}, {11, 0xcb5ccce18adf8376ull}, {13, 0x50f5a82326f52d34ull}, {14, 0xf389095188e76ee1ull}, {17, 0x88164a68c20514f0ull}, {19, 0x17cf610697616bdcull}, {20, 0x68b8509d614f267bull}}},
    {&Call64, 4, {}, {{1, 0xeec4aa2d02f0a95aull}, {2, 0x3347278c34486f22ull}, {3, 0xe1a7152c333a301eull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0x42f5e1d5c888f013ull}, {12, 0x3d7990216a7dd144ull}, {13, 0x0903d1d44f106329ull}, {15, 0xd41640b8abe2f287ull}}},
    {&Call65, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{3, 0xc44831fa3ca72f4bull}, {8, 0xf663450b3e0bec21ull}, {14, 0x61e8e0d7812e7b17ull}, {17, 0xde0fa59d4091d5adull}, {27, 0x8e0fd9363b4bfa4dull}, {35, 0xa99a8a88ffd90d74ull}, {48, 0x745a0fb750d786fcull}, {52, 0x9cea762fc6aa17ddull}, {71, 0x3268ce0f3ca77bd9ull}, {110, 0x4c38405a50fee59bull}, {119, 0x235f24685ef2e7b2ull}, {134, 0x061a204dcb24b2f6ull}}},
    {&Call66, 4, {0x1ull, 0x2ull, 0x4ull, 0x7ull, 0x8ull, 0x9ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0xfffffffffffeull, 0xffffffffffffull, 0x1000000000000ull, 0x3fef000000000001ull, 0x3fff000000000001ull, 0x4007000000000001ull, 0x400f000000000001ull, 0x4013000000000001ull, 0x4017000000000001ull, 0x401b000000000001ull, 0xbfef000000000001ull, 0xbfff000000000001ull, 0xffff000000000000ull, 0xffff000000000001ull, 0xfffffffffffffff7ull}, {{1, 0x3e2dd618cc5abf18ull}, {8, 0xb8ca5e43060879feull}, {11, 0x431195c060c3809dull}, {13, 0x7a05ab2b4a99dae3ull}, {17, 0xa03baebcb7c5e49bull}, {20, 0x2f3cdd8273a6dde2ull}, {21, 0x4e91ef0c4c7d5f18ull}, {22, 0x7874a1b7229c7123ull}, {23, 0xb409059923d17127ull}, {24, 0x9099ca867306a33dull}, {26, 0x8a646a99b607e567ull}, {27, 0xefdc5497a0a980caull}}},
    {&Call67, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x8829ba324bc2780aull}, {5, 0xb5e7ec52bbd57b95ull}, {7, 0x81809bc550505274ull}, {8, 0x4efe0fa6bf40211aull}, {10, 0xb0075978d7d67dffull}, {11, 0x90315b73491d2f55ull}, {13, 0x67d1d42ed94c7843ull}, {14, 0xb50a997be3299ee3ull}, {15, 0xc2891c324c67ea76ull}, {16, 0xc61e43438ac50ca6ull}, {17, 0x123d56b39b3fffb9ull}, {19, 0x2fee29e370c7bc96ull}}},
    {&Call68, 3, {}, {{2, 0x33a1278c32037d22ull}, {6, 0xd83aa957f8504ea4ull}, {8, 0x14d1b68cf59c6740ull}, {18, 0x3a867c5a326dfd39ull}, {20, 0x53ea4fe272646aafull}, {24, 0x40dde7b4aaf3a2e3ull}, {27, 0xd3680e4106e3fca6ull}, {29, 0x59fd4ceb4e9f9533ull}, {30, 0x27d404ce011b2e41ull}, {32, 0x4a3fd7801cc0847cull}, {39, 0x7813dbafcbbed19full}, {42, 0x02bf700092feb474ull}}},
    {&Call69, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{2, 0x17ab8c0d2303c96dull}, {3, 0xb60144d9018e54a4ull}, {5, 0xe1fa7e9691341a19ull}, {10, 0xa56fde9d1a7202aaull}, {12, 0x7eed6018083740adull}, {14, 0xe7b629d8b252b297ull}, {26, 0x5689f506aee76b1bull}, {27, 0x083cb892c427b9cbull}, {29, 0x051bdea1993cdf21ull}, {33, 0x5c1a9f33a86ffbddull}, {92, 0xdb41d15c87561887ull}, {95, 0x3fd8c3fb0f4295daull}}},
    {&Call70, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfc60a0560c9c62feull}, {6, 0x2c0da95058baf8a4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x55cc4ddf923318a9ull}, {11, 0xf04d79d063bb7e4aull}, {15, 0x7d2200bde302cc47ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05c7b26fd5d7d307ull}}},
    {&Call71, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0xe718278a88b87522ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcf6785617243cf6ull}, {6, 0xf44f3dd36a336c9dull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x6470e0d6b09aa436ull}, {11, 0xf04d79d063bb7e4aull}, {15, 0x7d21ffbde31d35f4ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}}},
    {&Call72, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x3606913534cd56cdull}, {1, 0x8686ba3233f2610aull}, {5, 0xb5e7ec52bbd57b95ull}, {7, 0x81809bc550505274ull}, {8, 0x4f6d0fa6b204841aull}, {10, 0xae55587827a1fd88ull}, {13, 0x688cd42ead663b43ull}, {14, 0xb51a987be34c40acull}, {15, 0xc3331d3240d70225ull}, {16, 0xc6d343438df0bfa6ull}, {17, 0x117156b382f013b9ull}, {20, 0xaca1688d92958ee8ull}}},
    {&Call73, 4, {}, {{1, 0xeeed1a2d02727f0aull}, {2, 0xe7204f8a8985592aull}, {3, 0xf134d976e0633bbdull}, {4, 0xb626dfc46ba1225bull}, {5, 0xb7ac285453649f06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0xd4cf58b8aaf1becfull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05c7b26fd5d7d307ull}}},
    {&Call74, 4, {0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0xfffffffffffeull, 0xffffffffffffull, 0x1000000000000ull, 0x3fef000000000001ull, 0x3fff000000000001ull, 0x4007000000000001ull, 0x400f000000000001ull, 0x4013000000000001ull, 0x4017000000000001ull, 0x401b000000000001ull, 0xbfef000000000001ull, 0xbfff000000000001ull, 0xffff000000000000ull, 0xffff000000000001ull}, {{0, 0x842cd030a42040d2ull}, {1, 0xcc4553dc5610f0e6ull}, {2, 0x1ad4526aee45d488ull}, {3, 0xc4482cfa3ca7ffccull}, {5, 0x8e143232c682ce9dull}, {6, 0x69c1cbf411496279ull}, {7, 0xb12e072e4919a5e3ull}, {8, 0x44afab89fa3cc112ull}, {14, 0x72054044532eb585ull}, {15, 0x760774472405918cull}, {16, 0x4301d0fc5cf14732ull}, {17, 0x5915046e7f2b1464ull}}},
    {&Call75, 4, {0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0xfffffffffffeull, 0xffffffffffffull, 0x1000000000000ull, 0x3fef000000000001ull, 0x3fff000000000001ull, 0x4007000000000001ull, 0x400f000000000001ull, 0x4013000000000001ull, 0x4017000000000001ull, 0x401b000000000001ull, 0xbfef000000000001ull, 0xbfff000000000001ull, 0xffff000000000000ull, 0xffff000000000001ull}, {{0, 0x8457c830a731857aull}, {1, 0x7cfe52d9d3c8fa55ull}, {2, 0x1a99426aec48f8f8ull}, {3, 0x5da52a90c8787fa5ull}, {5, 0x8e2f6232c7584e9dull}, {6, 0x69c1cbf411496279ull}, {7, 0xb12e072e4919a5e3ull}, {8, 0x44afab89fa3cc112ull}, {14, 0x724532ac5158e2c5ull}, {15, 0xa428144f6e01b07eull}, {16, 0xc301d0fb7371472bull}, {17, 0x59a3fc6e7569c50cull}}},
    {&Call76, 4, {0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0xfffffffffffeull, 0xffffffffffffull, 0x1000000000000ull, 0x3fef000000000001ull, 0x3fff000000000001ull, 0x4007000000000001ull, 0x400f000000000001ull, 0x4013000000000001ull, 0x4017000000000001ull, 0x401b000000000001ull, 0xbfef000000000001ull, 0xbfff000000000001ull, 0xffff000000000000ull, 0xffff000000000001ull}, {{0, 0x376a8e353c02b7f4ull}, {1, 0xcc4553dc5610f0e6ull}, {2, 0x1a99426aec48f8f8ull}, {3, 0xb3167b60a49fd338ull}, {5, 0x20eb9a37b3900055ull}, {6, 0x69c1cbf411496279ull}, {7, 0xb12e072e4919a5e3ull}, {8, 0x44afab89fa3cc112ull}, {14, 0xc3c5e2a9da83e8b5ull}, {15, 0xc19d19325a76cb19ull}, {16, 0x584b48ffabb2fb43ull}, {17, 0x5915046e7f2b1464ull}}},
    {&Call77, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0x2d1189700f4741cdull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcddf85616819e76ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d21f3bde31d2e50ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}}},
    {&Call78, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{2, 0xaf517be4a57662d1ull}, {8, 0xa64f2ee51bf101adull}, {15, 0xc19d1d325a76e025ull}, {20, 0x68b8509d614f267bull}, {27, 0xefdc5597a0a98979ull}, {29, 0x9aa29a8dbf36569full}, {31, 0xd633fe06e45afd7aull}, {36, 0xed8b6b7545d9a040ull}, {39, 0xa8f7427de2d0a98bull}, {50, 0x1d3df82cde7e940dull}, {56, 0x2c6e30c52e1a2163ull}, {63, 0xe5991bdd9f5b3af8ull}}},
    {&Call79, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0x8c59650c7c836903ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d21f3bde31d2e50ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}}},
    {&Call80, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b081005db3f74ull}}},
    {&Call81, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcbc485615d75c66ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b081005db3f74ull}}},
    {&Call82, 4, {}, {{2, 0x3347278c34486f22ull}, {3, 0x9978f18d2a48f9c2ull}, {5, 0xb7ac285453649f06ull}, {17, 0xa094126af94ac9e7ull}, {23, 0xad9360f23ee241d4ull}, {29, 0xb2cdacec167bbfd3ull}, {39, 0x7813dfafcbbeeeabull}, {70, 0x18f0e7d262f035aeull}, {75, 0x1d1067e14e55de1aull}, {76, 0xb434c8eb600eb257ull}, {83, 0x30340f42d7f8d0caull}, {86, 0x66fd1eee8f776a0cull}}},
    {&Call83, 3, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf0ead976ee1ef9bdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd83aa957f8504ea4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7990216a7dd144ull}, {13, 0x0903d1d44f106329ull}, {15, 0x7d2200bde302cc47ull}}},
    {&Call84, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {4, 0x3626dfc0b021225dull}, {5, 0xfcbc485615d75c66ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {13, 0x0903d1d44f106329ull}, {16, 0xcd4ab7321cb08d95ull}, {17, 0x05800a6fd7702d7full}, {19, 0x589b081005db3f74ull}}},
    {&Call85, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{2, 0xaf517be4a57662d1ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x32a98dcd552ceaf9ull}, {7, 0x16efb3685afe3180ull}, {8, 0xa64f2fe51bf1075eull}, {13, 0x50f5a72326f5538bull}, {16, 0x65c37d015cd09d15ull}, {21, 0x4e91ee0c4c7d55c7ull}, {23, 0x2af0c1a81c25ea15ull}, {27, 0xefdc5597a0a98979ull}, {29, 0x9aa29a8dbf36569full}, {30, 0x31f76320dfb67f51ull}}},
    {&Call86, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc55d30fa0a54ca19ull}, {5, 0xb4b0ec52b75ec695ull}, {7, 0x81809bc550505274ull}, {8, 0x4ee90fa6bfc4d01aull}, {10, 0xb0075978d7d67dffull}, {11, 0x90315a73491d2086ull}, {12, 0x3501e886e0b8c4e9ull}, {16, 0xc61e43438ac50ca6ull}, {17, 0x10de56b38e7268b9ull}, {19, 0x2e3729e37d75b196ull}}},
};

static const int NullFirst[] = {1, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 22, 23, 24, 25, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86};

int main() {
    std::uint64_t* const arena = MapArena();
    int checked = 0;
    int mismatches = 0;
    for (const Case& c : Cases) {
        for (const Seed& s : c.seeds) {
            const Inputs in = Generate(s.seed, arena, c.hints.data(), c.hints.size());
            const std::uint64_t result = c.call(in);
            const std::uint64_t digest = Digest(arena, result, c.kind);
            if (digest != s.digest) {
                std::fprintf(stderr, "%s case %d seed %llu: digest %016llx, expected %016llx\n", TestName, static_cast<int>(&c - Cases),
                             static_cast<unsigned long long>(s.seed), static_cast<unsigned long long>(digest), static_cast<unsigned long long>(s.digest));
                ++mismatches;
            }
            ++checked;
        }
    }
    UnmapArena(arena);
    Require(mismatches == 0);
    for (const int k : NullFirst) {
        Inputs in{};
        bool threw = false;
        try {
            Cases[k].call(in);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
    Require(checked == 1044);
    return 0;
}
