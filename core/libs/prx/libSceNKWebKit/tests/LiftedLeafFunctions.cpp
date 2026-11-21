#include "prx/libSceNKWebKit/include/LiftedLeafFunctionsExports.hpp"

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

constexpr const char* TestName = "LiftedLeafFunctions";

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
    return _ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(in.gpr[0], in.gpr[1], in.gpr[2], in.gpr[3]);
}

static std::uint64_t Call1(const Inputs& in) {
    return _ZN3JSC11MarkedBlock13areMarksStaleEv(in.gpr[0]);
}

static std::uint64_t Call2(const Inputs& in) {
    return _ZN3JSC11MarkedBlock6Handle13areMarksStaleEv(in.gpr[0]);
}

static std::uint64_t Call3(const Inputs& in) {
    return _ZN3JSC13RuntimeMethod15createPrototypeERNS_2VMERNS_14JSGlobalObjectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call4(const Inputs& in) {
    return _ZN3JSC13RuntimeMethod15subspaceForImplERNS_2VME(in.gpr[0]);
}

static std::uint64_t Call6(const Inputs& in) {
    return _ZN3JSC20failNextNewCodeBlockEPNS_14JSGlobalObjectE(in.gpr[0]);
}

static std::uint64_t Call7(const Inputs& in) {
    return BitsOfScalar(_ZN3JSC22EdenGCActivityCallback12lastGCLengthERNS_4HeapE(in.gpr[0], in.gpr[1]));
}

static std::uint64_t Call8(const Inputs& in) {
    return BitsOfScalar(_ZN3JSC22FullGCActivityCallback12lastGCLengthERNS_4HeapE(in.gpr[0], in.gpr[1]));
}

static std::uint64_t Call9(const Inputs& in) {
    return _ZN3JSC6JSCell9getObjectEv(in.gpr[0]);
}

static std::uint64_t Call10(const Inputs& in) {
    return _ZN3JSC8Bindings13RuntimeObject15createPrototypeERNS_2VMERNS_14JSGlobalObjectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call11(const Inputs& in) {
    return _ZN3JSC8Bindings13RuntimeObject15subspaceForImplERNS_2VME(in.gpr[0]);
}

static std::uint64_t Call12(const Inputs& in) {
    return _ZN3JSC8Debugger10isAttachedEPNS_14JSGlobalObjectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call13(const Inputs& in) {
    return _ZN3WTF10StringView16GraphemeClusters8IteratorC1EOS2_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call14(const Inputs& in) {
    return _ZN3WTF10StringView16GraphemeClusters8IteratorC2EOS2_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call15(const Inputs& in) {
    return _ZN3WTF15AutomaticThread9isWaitingERKNS_14AbstractLockerE(in.gpr[0]);
}

static std::uint64_t Call16(const Inputs& in) {
    _ZN3WTF24AutomaticThreadConditionC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call17(const Inputs& in) {
    _ZN3WTF24AutomaticThreadConditionC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call18(const Inputs& in) {
    return _ZN7WebCore10JSLocationC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call19(const Inputs& in) {
    return _ZN7WebCore10JSLocationC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call20(const Inputs& in) {
    return _ZN7WebCore11DisplayList20FillRectWithGradientC1ERKNS_9FloatRectERNS_8GradientE(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call21(const Inputs& in) {
    return _ZN7WebCore11DisplayList20FillRectWithGradientC2ERKNS_9FloatRectERNS_8GradientE(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call22(const Inputs& in) {
    return _ZN7WebCore11JSDOMMatrixC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call23(const Inputs& in) {
    return _ZN7WebCore11JSDOMMatrixC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call24(const Inputs& in) {
    return _ZN7WebCore11JSImageDataC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call25(const Inputs& in) {
    return _ZN7WebCore11JSImageDataC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call26(const Inputs& in) {
    return _ZN7WebCore11SimpleRangeC1EONS_13BoundaryPointES2_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call27(const Inputs& in) {
    return _ZN7WebCore11SimpleRangeC1ERKNS_13BoundaryPointES3_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call28(const Inputs& in) {
    return _ZN7WebCore11SimpleRangeC2EONS_13BoundaryPointES2_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call29(const Inputs& in) {
    return _ZN7WebCore11SimpleRangeC2ERKNS_13BoundaryPointES3_(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call30(const Inputs& in) {
    return _ZN7WebCore12JSTimeRangesC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call31(const Inputs& in) {
    return _ZN7WebCore12JSTimeRangesC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call32(const Inputs& in) {
    return _ZN7WebCore12NamedNodeMap3refEv(in.gpr[0]);
}

static std::uint64_t Call33(const Inputs& in) {
    _ZN7WebCore13GraphicsLayer12setZPositionEf(in.gpr[0], Scalar<float>(in.xmm[0]));
    return 0;
}

static std::uint64_t Call34(const Inputs& in) {
    return _ZN7WebCore13HitTestResultC1ERKNS_15HitTestLocationE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call35(const Inputs& in) {
    _ZN7WebCore13HitTestResultC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call36(const Inputs& in) {
    return _ZN7WebCore13HitTestResultC2ERKNS_15HitTestLocationE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call37(const Inputs& in) {
    _ZN7WebCore13HitTestResultC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call38(const Inputs& in) {
    return _ZN7WebCore13JSDOMRectListC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call39(const Inputs& in) {
    return _ZN7WebCore13JSDOMRectListC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call40(const Inputs& in) {
    return _ZN7WebCore14DocumentLoader21setCustomHeaderFieldsEON3WTF6VectorINS_18CustomHeaderFieldsELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call41(const Inputs& in) {
    return _ZN7WebCore14ResourceHandle11clearClientEv(in.gpr[0]);
}

static std::uint64_t Call42(const Inputs& in) {
    return _ZN7WebCore14ResourceHandle12firstRequestEv(in.gpr[0]);
}

static std::uint64_t Call43(const Inputs& in) {
    _ZN7WebCore15GraphicsContext20endTransparencyLayerEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call44(const Inputs& in) {
    _ZN7WebCore15GraphicsContext22beginTransparencyLayerEf(in.gpr[0]);
    return 0;
}

static std::uint64_t Call45(const Inputs& in) {
    return _ZN7WebCore15HitTestLocationC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call46(const Inputs& in) {
    _ZN7WebCore15HitTestLocationC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call47(const Inputs& in) {
    return _ZN7WebCore15HitTestLocationC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call48(const Inputs& in) {
    _ZN7WebCore15HitTestLocationC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call49(const Inputs& in) {
    return _ZN7WebCore15JSFetchResponseC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call50(const Inputs& in) {
    return _ZN7WebCore15JSFetchResponseC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call51(const Inputs& in) {
    return _ZN7WebCore16ScriptController11windowProxyEv(in.gpr[0]);
}

static std::uint64_t Call52(const Inputs& in) {
    return _ZN7WebCore16VisibleSelectionC1Ev(in.gpr[0]);
}

static std::uint64_t Call53(const Inputs& in) {
    return _ZN7WebCore16VisibleSelectionC2Ev(in.gpr[0]);
}

static std::uint64_t Call54(const Inputs& in) {
    return _ZN7WebCore17FrameLoaderClient11allowScriptEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call55(const Inputs& in) {
    return _ZN7WebCore17JSDOMRectReadOnlyC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call56(const Inputs& in) {
    return _ZN7WebCore17JSDOMRectReadOnlyC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call57(const Inputs& in) {
    return _ZN7WebCore17LibWebRTCProvideraSERKS0_(in.gpr[0]);
}

static std::uint64_t Call58(const Inputs& in) {
    _ZN7WebCore18TextureMapperLayer10setOpacityEf(in.gpr[0], Scalar<float>(in.xmm[0]));
    return 0;
}

static std::uint64_t Call59(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer11setPositionERKNS_10FloatPointE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call60(const Inputs& in) {
    _ZN7WebCore18TextureMapperLayer12setTransformERKNS_20TransformationMatrixE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call61(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer14setAnchorPointERKNS_12FloatPoint3DE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call62(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer14setPreserves3DEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call63(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer15setBoundsOriginERKNS_10FloatPointE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call64(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer15setContentsRectERKNS_9FloatRectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call65(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer15setDrawsContentEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call66(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer16setMasksToBoundsEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call67(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer17setContentsOpaqueEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call68(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer17setRepaintCounterEbi(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call69(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer18setContentsVisibleEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call70(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer19setContentsTileSizeERKNS_9FloatSizeE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call71(const Inputs& in) {
    _ZN7WebCore18TextureMapperLayer20setChildrenTransformERKNS_20TransformationMatrixE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call72(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer20setContentsTilePhaseERKNS_9FloatSizeE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call73(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer21setBackfaceVisibilityEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call74(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer23setContentsClippingRectERKNS_16FloatRoundedRectE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call75(const Inputs& in) {
    return _ZN7WebCore18TextureMapperLayer7setSizeERKNS_9FloatSizeE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call76(const Inputs& in) {
    return _ZN7WebCore19JSAnimationTimelineC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call77(const Inputs& in) {
    return _ZN7WebCore19JSAnimationTimelineC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call78(const Inputs& in) {
    return _ZN7WebCore19JSDOMMatrixReadOnlyC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call79(const Inputs& in) {
    return _ZN7WebCore19JSDOMMatrixReadOnlyC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call80(const Inputs& in) {
    _ZN7WebCore20PasteboardCustomData5EntryC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call81(const Inputs& in) {
    _ZN7WebCore20PasteboardCustomData5EntryC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call82(const Inputs& in) {
    return _ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call83(const Inputs& in) {
    return _ZN7WebCore20PasteboardCustomDataC1EOS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call84(const Inputs& in) {
    _ZN7WebCore20PasteboardCustomDataC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call85(const Inputs& in) {
    return _ZN7WebCore20PasteboardCustomDataC2EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call86(const Inputs& in) {
    return _ZN7WebCore20PasteboardCustomDataC2EOS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call87(const Inputs& in) {
    _ZN7WebCore20PasteboardCustomDataC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call88(const Inputs& in) {
    _ZN7WebCore20ScrollingCoordinator13pageDestroyedEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call89(const Inputs& in) {
    _ZN7WebCore21InspectorFrontendHost16disconnectClientEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call90(const Inputs& in) {
    _ZN7WebCore21InspectorFrontendHostC1EPNS_23InspectorFrontendClientEPNS_4PageE(in.gpr[0], in.gpr[1], in.gpr[2]);
    return 0;
}

static std::uint64_t Call91(const Inputs& in) {
    _ZN7WebCore21InspectorFrontendHostC2EPNS_23InspectorFrontendClientEPNS_4PageE(in.gpr[0], in.gpr[1], in.gpr[2]);
    return 0;
}

static std::uint64_t Call92(const Inputs& in) {
    _ZN7WebCore24CoordinatedGraphicsLayer21invalidateCoordinatorEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call93(const Inputs& in) {
    _ZN7WebCore26ContextDestructionObserver16contextDestroyedEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call94(const Inputs& in) {
    return _ZN7WebCore26MessagePortChannelRegistryC1EON3WTF8FunctionIFvRKNS_21MessagePortIdentifierENS1_16ObjectIdentifierINS_21ProcessIdentifierTypeEEEONS1_17CompletionHandlerIFvNS_26MessagePortChannelProvider11HasActivityEEEEEEE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call95(const Inputs& in) {
    return _ZN7WebCore26MessagePortChannelRegistryC2EON3WTF8FunctionIFvRKNS_21MessagePortIdentifierENS1_16ObjectIdentifierINS_21ProcessIdentifierTypeEEEONS1_17CompletionHandlerIFvNS_26MessagePortChannelProvider11HasActivityEEEEEEE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call96(const Inputs& in) {
    _ZN7WebCore27CSSComputedStyleDeclaration3refEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call97(const Inputs& in) {
    return _ZN7WebCore27PlatformMediaSessionManager12restrictionsENS_20PlatformMediaSession9MediaTypeE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call98(const Inputs& in) {
    return _ZN7WebCore27PlatformMediaSessionManager14addRestrictionENS_20PlatformMediaSession9MediaTypeEj(in.gpr[0], in.gpr[1], in.gpr[2]);
}

static std::uint64_t Call99(const Inputs& in) {
    return _ZN7WebCore28InspectorFrontendClientLocal11isUnderTestEv(in.gpr[0]);
}

static std::uint64_t Call100(const Inputs& in) {
    _ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call101(const Inputs& in) {
    _ZN7WebCore4Page22removeLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call102(const Inputs& in) {
    return _ZN7WebCore4Page24setCORSDisablingPatternsEON3WTF6VectorINS_21UserContentURLPatternELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call103(const Inputs& in) {
    _ZN7WebCore4Page29startTrackingRenderingUpdatesEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call104(const Inputs& in) {
    _ZN7WebCore4Page34setInitialScaleIgnoringContentSizeEf(in.gpr[0], Scalar<float>(in.xmm[0]));
    return 0;
}

static std::uint64_t Call105(const Inputs& in) {
    return _ZN7WebCore4Page45setOutsideViewportThrottlingEnabledForTestingEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call106(const Inputs& in) {
    _ZN7WebCore4PathC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call107(const Inputs& in) {
    _ZN7WebCore4PathC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call108(const Inputs& in) {
    _ZN7WebCore6Editor7CommandC1Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call109(const Inputs& in) {
    _ZN7WebCore6Editor7CommandC2Ev(in.gpr[0]);
    return 0;
}

static std::uint64_t Call110(const Inputs& in) {
    return _ZN7WebCore6JSFileC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call111(const Inputs& in) {
    return _ZN7WebCore6JSFileC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call112(const Inputs& in) {
    _ZN7WebCore6Widget12setFrameRectERKNS_7IntRectE(in.gpr[0], in.gpr[1]);
    return 0;
}

static std::uint64_t Call113(const Inputs& in) {
    return _ZN7WebCore7CSSRule10setCssTextERKN3WTF6StringE(in.gpr[0]);
}

static std::uint64_t Call114(const Inputs& in) {
    return _ZN7WebCore7JSRangeC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call115(const Inputs& in) {
    return _ZN7WebCore7JSRangeC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call116(const Inputs& in) {
    return _ZN7WebCore8Document16setXMLStandaloneEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call117(const Inputs& in) {
    _ZN7WebCore8Document25startTrackingStyleRecalcsEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call118(const Inputs& in) {
    return _ZN7WebCore8JSDOMURLC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call119(const Inputs& in) {
    return _ZN7WebCore8JSDOMURLC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call120(const Inputs& in) {
    return _ZN7WebCore8JSPath2DC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call121(const Inputs& in) {
    return _ZN7WebCore8JSPath2DC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call122(const Inputs& in) {
    return _ZN7WebCore9JSDOMRectC1ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call123(const Inputs& in) {
    return _ZN7WebCore9JSDOMRectC2ERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call124(const Inputs& in) {
    _ZN9Inspector14ConsoleMessage14incrementCountEv(in.gpr[0]);
    return 0;
}

static std::uint64_t Call125(const Inputs& in) {
    return _ZN9Inspector22InspectorDebuggerAgent20setSuppressAllPausesEb(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call126(const Inputs& in) {
    return _ZN9Inspector22RemoteInspectionTargetaSERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call127(const Inputs& in) {
    return _ZN9Inspector24RemoteControllableTargetaSERKS0_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call128(const Inputs& in) {
    return _ZNK3JSC4Yarr17RegularExpression13matchedLengthEv(in.gpr[0]);
}

static std::uint64_t Call129(const Inputs& in) {
    return _ZNK3WTF10StringView16GraphemeClusters8IteratoreqERKS2_(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call130(const Inputs& in) {
    return _ZNK7WebCore11FrameLoader20activeDocumentLoaderEv(in.gpr[0]);
}

static std::uint64_t Call131(const Inputs& in) {
    return BitsOfScalar(_ZNK7WebCore11MediaPlayer6volumeEv(in.gpr[0]));
}

static std::uint64_t Call132(const Inputs& in) {
    return _ZNK7WebCore11MessagePort18isLocallyReachableEv(in.gpr[0]);
}

static std::uint64_t Call133(const Inputs& in) {
    return _ZNK7WebCore11RenderStyle10lineHeightEv(in.gpr[0]);
}

static std::uint64_t Call134(const Inputs& in) {
    return _ZNK7WebCore11RenderStyle11fontCascadeEv(in.gpr[0]);
}

static std::uint64_t Call135(const Inputs& in) {
    return _ZNK7WebCore11RenderStyle15fontDescriptionEv(in.gpr[0]);
}

static std::uint64_t Call136(const Inputs& in) {
    return _ZNK7WebCore14ResourceHandle6clientEv(in.gpr[0]);
}

static std::uint64_t Call137(const Inputs& in) {
    return _ZNK7WebCore16HTMLImageElement11cachedImageEv(in.gpr[0]);
}

static std::uint64_t Call138(const Inputs& in) {
    return _ZNK7WebCore16HTMLImageElement36pendingDecodePromisesCountForTestingEv(in.gpr[0]);
}

static std::uint64_t Call139(const Inputs& in) {
    return _ZNK7WebCore16HTMLImageElement8completeEv(in.gpr[0]);
}

static std::uint64_t Call140(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement10isURLFieldEv(in.gpr[0]);
}

static std::uint64_t Call141(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement11isDateFieldEv(in.gpr[0]);
}

static std::uint64_t Call142(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement11isTextFieldEv(in.gpr[0]);
}

static std::uint64_t Call143(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement11isTimeFieldEv(in.gpr[0]);
}

static std::uint64_t Call144(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement11isWeekFieldEv(in.gpr[0]);
}

static std::uint64_t Call145(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement12isEmailFieldEv(in.gpr[0]);
}

static std::uint64_t Call146(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement12isFileUploadEv(in.gpr[0]);
}

static std::uint64_t Call147(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement12isMonthFieldEv(in.gpr[0]);
}

static std::uint64_t Call148(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement13isNumberFieldEv(in.gpr[0]);
}

static std::uint64_t Call149(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement13isSearchFieldEv(in.gpr[0]);
}

static std::uint64_t Call150(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement15isPasswordFieldEv(in.gpr[0]);
}

static std::uint64_t Call151(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement16isTelephoneFieldEv(in.gpr[0]);
}

static std::uint64_t Call152(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement20isDateTimeLocalFieldEv(in.gpr[0]);
}

static std::uint64_t Call153(const Inputs& in) {
    return _ZNK7WebCore16HTMLInputElement6isTextEv(in.gpr[0]);
}

static std::uint64_t Call154(const Inputs& in) {
    return BitsOfScalar(_ZNK7WebCore16HTMLMediaElement12playbackRateEv(in.gpr[0]));
}

static std::uint64_t Call155(const Inputs& in) {
    return _ZNK7WebCore16HTMLMediaElement27webkitClosedCaptionsVisibleEv(in.gpr[0]);
}

static std::uint64_t Call156(const Inputs& in) {
    return _ZNK7WebCore16HTMLMediaElement6pausedEv(in.gpr[0]);
}

static std::uint64_t Call157(const Inputs& in) {
    return BitsOfScalar(_ZNK7WebCore16HTMLMediaElement6volumeEv(in.gpr[0]));
}

static std::uint64_t Call158(const Inputs& in) {
    return _ZNK7WebCore16HTMLMediaElement7seekingEv(in.gpr[0]);
}

static std::uint64_t Call159(const Inputs& in) {
    return _ZNK7WebCore16JSXMLHttpRequest7wrappedEv(in.gpr[0]);
}

static std::uint64_t Call160(const Inputs& in) {
    return _ZNK7WebCore17JSDOMGlobalObject13worldIsNormalEv(in.gpr[0]);
}

static std::uint64_t Call161(const Inputs& in) {
    return _ZNK7WebCore17MouseRelatedEvent1xEv(in.gpr[0]);
}

static std::uint64_t Call162(const Inputs& in) {
    return _ZNK7WebCore17MouseRelatedEvent1yEv(in.gpr[0]);
}

static std::uint64_t Call163(const Inputs& in) {
    return _ZNK7WebCore18TextureMapperLayer10hasFiltersEv(in.gpr[0]);
}

static std::uint64_t Call164(const Inputs& in) {
    return _ZNK7WebCore18TextureMapperLayer12drawsContentEv(in.gpr[0]);
}

static std::uint64_t Call165(const Inputs& in) {
    return _ZNK7WebCore18TextureMapperLayer18contentsAreVisibleEv(in.gpr[0]);
}

static std::uint64_t Call166(const Inputs& in) {
    return BitsOfScalar(_ZNK7WebCore18TextureMapperLayer7opacityEv(in.gpr[0]));
}

static std::uint64_t Call167(const Inputs& in) {
    return _ZNK7WebCore18TextureMapperLayer9layerRectEv(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call168(const Inputs& in) {
    return _ZNK7WebCore18TextureMapperLayer9transformEv(in.gpr[0], in.gpr[1]);
}

static std::uint64_t Call169(const Inputs& in) {
    return _ZNK7WebCore21RenderLayerCompositor15rootRenderLayerEv(in.gpr[0]);
}

static std::uint64_t Call170(const Inputs& in) {
    return _ZNK7WebCore23FrameLoaderStateMachine15firstLayoutDoneEv(in.gpr[0]);
}

static std::uint64_t Call171(const Inputs& in) {
    return _ZNK7WebCore23FrameLoaderStateMachine23committingFirstRealLoadEv(in.gpr[0]);
}

static std::uint64_t Call172(const Inputs& in) {
    return _ZNK7WebCore23FrameLoaderStateMachine28creatingInitialEmptyDocumentEv(in.gpr[0]);
}

static std::uint64_t Call173(const Inputs& in) {
    return _ZNK7WebCore23FrameLoaderStateMachine32isDisplayingInitialEmptyDocumentEv(in.gpr[0]);
}

static std::uint64_t Call174(const Inputs& in) {
    return _ZNK7WebCore24CachedResourceHandleBase3getEv(in.gpr[0]);
}

static std::uint64_t Call175(const Inputs& in) {
    return _ZNK7WebCore24CachedResourceHandleBasecvMS0_PNS_14CachedResourceEEv(in.gpr[0]);
}

static std::uint64_t Call176(const Inputs& in) {
    return _ZNK7WebCore24CachedResourceHandleBasentEv(in.gpr[0]);
}

static std::uint64_t Call177(const Inputs& in) {
    return _ZNK7WebCore24CoordinatedGraphicsLayer17usesContentsLayerEv(in.gpr[0]);
}

static std::uint64_t Call178(const Inputs& in) {
    return _ZNK7WebCore25BackForwardItemIdentifier7isValidEv(in.gpr[0]);
}

static std::uint64_t Call179(const Inputs& in) {
    return _ZNK7WebCore26HTMLTextFormControlElement26isInnerTextElementEditableEv(in.gpr[0]);
}

static std::uint64_t Call180(const Inputs& in) {
    return _ZNK7WebCore28InspectorFrontendClientLocal28userInterfaceLayoutDirectionEv(in.gpr[0]);
}

static std::uint64_t Call181(const Inputs& in) {
    return _ZNK7WebCore4Node13ownerDocumentEv(in.gpr[0]);
}

static std::uint64_t Call182(const Inputs& in) {
    return _ZNK7WebCore4Page15visibilityStateEv(in.gpr[0]);
}

static std::uint64_t Call183(const Inputs& in) {
    return _ZNK7WebCore4Page23isMonitoringWheelEventsEv(in.gpr[0]);
}

static std::uint64_t Call184(const Inputs& in) {
    return _ZNK7WebCore4Page23remoteInspectionAllowedEv(in.gpr[0]);
}

static std::uint64_t Call185(const Inputs& in) {
    return _ZNK7WebCore4Path6isNullEv(in.gpr[0]);
}

static std::uint64_t Call186(const Inputs& in) {
    return _ZNK7WebCore6JSNode8nodeTypeERN3JSC14JSGlobalObjectE(in.gpr[0]);
}

static std::uint64_t Call187(const Inputs& in) {
    return _ZNK7WebCore9DOMWindow8documentEv(in.gpr[0]);
}

static std::uint64_t Call188(const Inputs& in) {
    return _ZNK7WebCore9FrameView14didFirstLayoutEv(in.gpr[0]);
}

static std::uint64_t Call189(const Inputs& in) {
    return _ZNK7WebCore9SampleMap5emptyEv(in.gpr[0]);
}

static std::uint64_t Call190(const Inputs& in) {
    return _ZNK9Inspector14ConsoleMessage6sourceEv(in.gpr[0]);
}

static std::uint64_t Call191(const Inputs& in) {
    return _ZNK9Inspector22InspectorDebuggerAgent17breakpointsActiveEv(in.gpr[0]);
}

static std::uint64_t Call192(const Inputs& in) {
    return _ZNK9Inspector22InspectorDebuggerAgent8isPausedEv(in.gpr[0]);
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
    {&Call0, 4, {}, {{0, 0x47f9c76df131c0cfull}, {1, 0xeeedf22d02738c32ull}, {2, 0x3352008c3413c667ull}, {3, 0xf134d976e0633bbdull}, {4, 0x362ea6c0b0502572ull}, {5, 0xb7a2c75450e3d739ull}, {6, 0xd86c3957fab8f154ull}, {7, 0x3a988ff16524234eull}, {8, 0x3ffb1e8daedb35b8ull}, {9, 0x566c5ddf8e039f99ull}, {10, 0xce14aaab6eafede2ull}, {11, 0x43b7f1d5fdce3162ull}}},
    {&Call1, 1, {}, {{6, 0xd870a857fa6586f3ull}, {11, 0xf04d78d063bb69fdull}, {17, 0xa094136af94ac734ull}, {23, 0xad9361f23ee2479bull}, {26, 0xe3cde125456cc943ull}, {38, 0x37089b8ea9ddf7b4ull}, {39, 0x7813daafcbbede2cull}, {44, 0x68c5891a8d7a0b91ull}, {45, 0x7978dea4d93c4ae5ull}, {48, 0x426bd23214f17d22ull}, {50, 0x6ad3da525d89931bull}, {53, 0x786a21c0a8f3d6f4ull}}},
    {&Call2, 1, {}, {{8, 0x14d1b78cf59c68b7ull}, {12, 0x3d7991216a7dd8f7ull}, {23, 0xad9361f23ee2479bull}, {32, 0x4a3fd8801cc0fe33ull}, {44, 0x68c5881a8d7a0162ull}, {48, 0x426bd23214f17d22ull}, {50, 0x6ad3d9525d89f568ull}, {56, 0xe8f84cdd5999ce59ull}, {60, 0xfa87bf6cfff94a4cull}, {62, 0xb5a4509e096e36a1ull}, {77, 0x2b11011b16b1f85dull}, {83, 0x30341042d7f82e99ull}}},
    {&Call3, 4, {}, {{1, 0xeef8122d02edc092ull}, {2, 0xe7204f8a8985592aull}, {3, 0x797041035bf6c408ull}, {4, 0x3626ddc0b02136e7ull}, {5, 0xb7ac285453649f06ull}, {6, 0x89726268dbb68c33ull}, {7, 0x3a130ff16065fbceull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615ed8318763caull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x7f82f1d7fbe55b49ull}, {14, 0x13f181be47c039b6ull}}},
    {&Call4, 4, {0x8ull, 0x10ull, 0x20ull, 0x80ull, 0x100ull, 0x200ull, 0x1000ull, 0x4000ull, 0x53b7ull, 0x53b8ull, 0x53b9ull, 0xffffffffffffac47ull}, {{0, 0x83b4c83099dc8a7aull}, {1, 0x0af42d1d7073d298ull}, {2, 0x40d178523169ea4cull}, {3, 0x0d9791fc4cb2622aull}, {5, 0x634595ab95f7cbe0ull}, {6, 0x1b8c13f781bd56a1ull}, {7, 0x6538e170c490b52eull}, {8, 0x4c01ed8f52200823ull}, {10, 0x3a7b507b3fa9665cull}, {11, 0xa0d5db4edae2e1b0ull}, {12, 0x3050ef86caab4a82ull}, {13, 0xacb8ccddf3f59305ull}}},
    {&Call6, 3, {0x0ull, 0x1ull, 0x2ull, 0x8ull, 0x9ull, 0xaull, 0xbull, 0xfffffffffffffff5ull, 0xfffffffffffffffeull}, {{1, 0x551d496f7808265eull}, {2, 0x4e0946a161b9ca19ull}, {3, 0xd3708a2a6e420328ull}, {8, 0x1669abe0ba4df4a5ull}, {10, 0xf1a1b4860ca9596full}, {14, 0x84ccc21e80f3d172ull}, {15, 0x4230256063e7c5e3ull}, {19, 0x122010c47519e7f0ull}, {20, 0x6274ab9aea329385ull}, {21, 0x39fac3cc435e98d3ull}, {29, 0x8566b586e4accfb2ull}, {32, 0x9d47e648b3c719fcull}}},
    {&Call7, 6, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd73bdb5781a559e4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x13c4ec8ccf91ab98ull}, {9, 0xa3354fd8251a1909ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {14, 0x13f181be47c039b6ull}}},
    {&Call8, 6, {}, {{1, 0x51da3a28fd46a9eaull}, {2, 0x351e458c18184f92ull}, {3, 0xf134e376e0611abfull}, {4, 0x3646dfc0b378c25dull}, {5, 0xb8590e542d5112feull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa3d317d823243c51ull}, {10, 0x1256a0fa8c6eac22ull}, {13, 0x0903d1d44f106329ull}, {14, 0x13f181be47c039b6ull}}},
    {&Call9, 4, {0x1ull, 0x2ull, 0x4ull, 0x10ull, 0x16ull, 0x17ull, 0x18ull, 0xffffffffffffffe8ull}, {{0, 0x8410e830a590179aull}, {1, 0x931eb16dbbac5340ull}, {2, 0x116c865bd74c0297ull}, {3, 0x0db011fc4db800aaull}, {5, 0x6ce6e8aec598bed1ull}, {6, 0x69c1cbf411496279ull}, {7, 0xe4a7495f7618af4full}, {8, 0x472eea06fd0826b3ull}, {10, 0x2714e002cc9da9d5ull}, {11, 0x6e920421072f242full}, {12, 0x2fff7786385a7beaull}, {13, 0xb216bb31bf747885ull}}},
    {&Call10, 4, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0x2d54a9700298522dull}, {4, 0x3626ddc0b02136e7ull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x40114e8e51a30108ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1670f1066b3ull}, {14, 0x13f181be47c039b6ull}}},
    {&Call11, 4, {0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x400ull, 0x1000ull, 0x4000ull, 0x54f7ull, 0x54f8ull, 0x54f9ull, 0xffffffffffffab07ull}, {{0, 0x83b6883099c984baull}, {1, 0xeb407b1e3e213c85ull}, {2, 0x814b7614db3a8135ull}, {3, 0x0d96d1fc4cbb286aull}, {5, 0x1c4b005a116d0efeull}, {6, 0x1b8a53f781885061ull}, {7, 0xb1ce97e90f6ff7deull}, {8, 0xe138dd596f3bdd9full}, {10, 0xe47b9e711c4c6034ull}, {11, 0x7b25ff757f0a3db9ull}, {12, 0x3051af86caa000c2ull}, {13, 0x7663db360f208dd6ull}}},
    {&Call12, 1, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347268c344865f1ull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa1615dd831877d99ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {14, 0x13f181be47c039b6ull}}},
    {&Call13, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xafefab59da4ff8d7ull}, {2, 0x6bfaffa06ae46ae9ull}, {3, 0x54ea92d913adec19ull}, {5, 0x0dd8dd6a627c1d6eull}, {6, 0xdff9bc6d36c8dfddull}, {7, 0x81809bc550505274ull}, {8, 0xd949a53c1cdc52beull}, {10, 0x886b9a9e94fc0ec5ull}, {11, 0xe4705388de0ede5full}, {12, 0x7061f0187d6cecfdull}, {13, 0x742cd77e8f792325ull}, {14, 0x6087b903fa1975b3ull}}},
    {&Call14, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xafefab59da4ff8d7ull}, {2, 0x6bfaffa06ae46ae9ull}, {3, 0x54ea92d913adec19ull}, {5, 0x0dd8dd6a627c1d6eull}, {6, 0xdff9bc6d36c8dfddull}, {7, 0x81809bc550505274ull}, {8, 0xd949a53c1cdc52beull}, {10, 0x886b9a9e94fc0ec5ull}, {11, 0xe4705388de0ede5full}, {12, 0x7061f0187d6cecfdull}, {13, 0x742cd77e8f792325ull}, {14, 0x6087b903fa1975b3ull}}},
    {&Call15, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7ec52bbd57b95ull}, {6, 0x69c1caf411496baaull}, {7, 0x81809bc550505274ull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call16, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x088de98b98aad6b8ull}, {1, 0xb53ef8ae0afa35a5ull}, {2, 0x7cfa0e1a7c6cd700ull}, {3, 0xba836735d2b4a610ull}, {5, 0xc1e21ed2b02983e8ull}, {6, 0xb3838c4a5547a75dull}, {7, 0xbbe9fa14ff7eaffaull}, {8, 0xf4ebb32b406ed38cull}, {10, 0xa2cedd1b0f340e83ull}, {11, 0x4bcdcf40e0ec7842ull}, {12, 0xb892dbe79168a28aull}, {13, 0x097980cbb1618299ull}}},
    {&Call17, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x088de98b98aad6b8ull}, {1, 0xb53ef8ae0afa35a5ull}, {2, 0x7cfa0e1a7c6cd700ull}, {3, 0xba836735d2b4a610ull}, {5, 0xc1e21ed2b02983e8ull}, {6, 0xb3838c4a5547a75dull}, {7, 0xbbe9fa14ff7eaffaull}, {8, 0xf4ebb32b406ed38cull}, {10, 0xa2cedd1b0f340e83ull}, {11, 0x4bcdcf40e0ec7842ull}, {12, 0xb892dbe79168a28aull}, {13, 0x097980cbb1618299ull}}},
    {&Call18, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x5217837c90a3f4eaull}, {4, 0xa2616403489c6cdcull}, {5, 0x0f5a7fcd3523d208ull}, {6, 0x56c6d5687d7c53f8ull}, {9, 0x41382a6b5e372486ull}, {13, 0xfef027ab412abaa9ull}, {17, 0x4271084ffcd0002eull}, {18, 0x0d6279c14a48097cull}, {23, 0x65926691d5c4f093ull}, {24, 0xa1e733b5be73c259ull}, {25, 0x4f9a5a4a6354b16aull}}},
    {&Call19, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x5217837c90a3f4eaull}, {4, 0xa2616403489c6cdcull}, {5, 0x0f5a7fcd3523d208ull}, {6, 0x56c6d5687d7c53f8ull}, {9, 0x41382a6b5e372486ull}, {13, 0xfef027ab412abaa9ull}, {17, 0x4271084ffcd0002eull}, {18, 0x0d6279c14a48097cull}, {23, 0x65926691d5c4f093ull}, {24, 0xa1e733b5be73c259ull}, {25, 0x4f9a5a4a6354b16aull}}},
    {&Call20, 3, {}, {{1, 0xeca9d4d2113a6036ull}, {2, 0xc7c64173afce5fddull}, {3, 0xf16c31f0412b22caull}, {5, 0xd4465164cef91886ull}, {6, 0xc7636e735324687cull}, {7, 0x2fc5b75343efee83ull}, {10, 0xadcefe445d90849eull}, {15, 0x104761b9dd1e485full}, {16, 0x3c3b9e7fd54b9f3dull}, {17, 0x0572e11d2e8e4bd5ull}, {20, 0x34b038070bddd4dbull}, {22, 0x1ca29b2715abf990ull}}},
    {&Call21, 3, {}, {{1, 0xeca9d4d2113a6036ull}, {2, 0xc7c64173afce5fddull}, {3, 0xf16c31f0412b22caull}, {5, 0xd4465164cef91886ull}, {6, 0xc7636e735324687cull}, {7, 0x2fc5b75343efee83ull}, {10, 0xadcefe445d90849eull}, {15, 0x104761b9dd1e485full}, {16, 0x3c3b9e7fd54b9f3dull}, {17, 0x0572e11d2e8e4bd5ull}, {20, 0x34b038070bddd4dbull}, {22, 0x1ca29b2715abf990ull}}},
    {&Call22, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call23, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call24, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call25, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call26, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x0a314746bd244dc8ull}, {5, 0x060510f1100fc0b2ull}, {6, 0xeb18f1bf3b642381ull}, {7, 0xe0029201b05eda22ull}, {8, 0xfe3355d56928d8f4ull}, {11, 0x6e0edddb9342e339ull}, {12, 0x8c3e68fb47e45806ull}, {13, 0xfb92191174b02aafull}, {14, 0x046a15ecdffc9a91ull}, {15, 0x2145f58593104334ull}, {17, 0xfb4a4086e2c947f8ull}, {19, 0x72b5713b66e4827aull}}},
    {&Call27, 3, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{13, 0xf9bf480b76415285ull}, {17, 0x45409fbd41e6c033ull}, {23, 0x0e4d047b90249bfbull}, {28, 0xe6680d0f46204664ull}, {29, 0x95677499d7bad549ull}, {30, 0x2c6b929e2c07e678ull}, {32, 0x83ee3cc85cda8a32ull}, {36, 0x18f395ba2689fe51ull}, {41, 0x8e76d4e0ab8328ebull}, {53, 0x6d84c01b97923b02ull}, {63, 0xbd2a0dafecd4611eull}, {68, 0x63eaf8827b1f5b79ull}}},
    {&Call28, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x0a314746bd244dc8ull}, {5, 0x060510f1100fc0b2ull}, {6, 0xeb18f1bf3b642381ull}, {7, 0xe0029201b05eda22ull}, {8, 0xfe3355d56928d8f4ull}, {11, 0x6e0edddb9342e339ull}, {12, 0x8c3e68fb47e45806ull}, {13, 0xfb92191174b02aafull}, {14, 0x046a15ecdffc9a91ull}, {15, 0x2145f58593104334ull}, {17, 0xfb4a4086e2c947f8ull}, {19, 0x72b5713b66e4827aull}}},
    {&Call29, 3, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{13, 0xf9bf480b76415285ull}, {17, 0x45409fbd41e6c033ull}, {23, 0x0e4d047b90249bfbull}, {28, 0xe6680d0f46204664ull}, {29, 0x95677499d7bad549ull}, {30, 0x2c6b929e2c07e678ull}, {32, 0x83ee3cc85cda8a32ull}, {36, 0x18f395ba2689fe51ull}, {41, 0x8e76d4e0ab8328ebull}, {53, 0x6d84c01b97923b02ull}, {63, 0xbd2a0dafecd4611eull}, {68, 0x63eaf8827b1f5b79ull}}},
    {&Call30, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call31, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call32, 4, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{0, 0x7afc96adcd55bf2cull}, {6, 0xb7db36edbb33a654ull}, {7, 0xa3a0c10c0b619a30ull}, {8, 0x41f565270fc8e877ull}, {12, 0x75c85b5ad3c3a7e5ull}, {14, 0xfd24a568ff4c9aabull}, {26, 0x3e9b0141bee32a6bull}, {27, 0x6748da51f81a99a9ull}, {32, 0x24061eaa34eed580ull}, {35, 0x0cc8b8d49d71a488ull}, {40, 0x548dca78a43d75c8ull}, {42, 0xa6320b7f4150b5ceull}}},
    {&Call33, 0, {}, {{1, 0xf739f292260bc2f5ull}, {2, 0x24ac0913406445d1ull}, {3, 0x7bc15026e930def0ull}, {4, 0x3626dfc0b021225dull}, {5, 0xbec31e0a251380c3ull}, {6, 0xd74a5feb185f52d1ull}, {7, 0x31e054a242cf858bull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x49cefff9bc535120ull}, {10, 0xec47cf62b7767017ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x07be0c735b5839ceull}}},
    {&Call34, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x5faa63cbbbd8c2cbull}, {2, 0xcb2be5b582e3f088ull}, {3, 0x2c7f7cef47cab54cull}, {5, 0x358d87c79157e3ddull}, {6, 0x39cce7019000d9a4ull}, {7, 0x98027bd2ad9fc4bcull}, {8, 0xeb4fa65730c2ad54ull}, {10, 0x61402ea99855ce86ull}, {11, 0x3a42056fa703e044ull}, {12, 0x059869a8a892152bull}, {13, 0x6ed678f5973cc243ull}, {14, 0x754e4e10b4c71e76ull}}},
    {&Call35, 0, {0x0ull, 0x1ull, 0xffull, 0x100ull, 0x101ull, 0xfffffffffffffeffull, 0xffffffffffffffffull}, {{0, 0x2388f9029059c53cull}, {1, 0x669d3ba80b3e858eull}, {2, 0xa12360ac26f7f299ull}, {3, 0x38134ab4144e62b6ull}, {5, 0x81f99c57931d72bbull}, {6, 0x1646b288a6e65a8full}, {7, 0x0bdf5cf4590b3bcdull}, {8, 0x630b6f4c267139e1ull}, {10, 0x52631342881a3889ull}, {11, 0x4487183a7c900a93ull}, {12, 0x4a39d210f2d71ab0ull}, {13, 0xbeb9d9e2c028fcf9ull}}},
    {&Call36, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x5faa63cbbbd8c2cbull}, {2, 0xcb2be5b582e3f088ull}, {3, 0x2c7f7cef47cab54cull}, {5, 0x358d87c79157e3ddull}, {6, 0x39cce7019000d9a4ull}, {7, 0x98027bd2ad9fc4bcull}, {8, 0xeb4fa65730c2ad54ull}, {10, 0x61402ea99855ce86ull}, {11, 0x3a42056fa703e044ull}, {12, 0x059869a8a892152bull}, {13, 0x6ed678f5973cc243ull}, {14, 0x754e4e10b4c71e76ull}}},
    {&Call37, 0, {0x0ull, 0x1ull, 0xffull, 0x100ull, 0x101ull, 0xfffffffffffffeffull, 0xffffffffffffffffull}, {{0, 0x2388f9029059c53cull}, {1, 0x669d3ba80b3e858eull}, {2, 0xa12360ac26f7f299ull}, {3, 0x38134ab4144e62b6ull}, {5, 0x81f99c57931d72bbull}, {6, 0x1646b288a6e65a8full}, {7, 0x0bdf5cf4590b3bcdull}, {8, 0x630b6f4c267139e1ull}, {10, 0x52631342881a3889ull}, {11, 0x4487183a7c900a93ull}, {12, 0x4a39d210f2d71ab0ull}, {13, 0xbeb9d9e2c028fcf9ull}}},
    {&Call38, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call39, 4, {}, {{2, 0xee380507747c202cull}, {3, 0x1db8f573dd10053full}, {4, 0x59cb29f88f9d32dfull}, {5, 0x9e2df702514a4b09ull}, {6, 0xf6e70e65732ee6a5ull}, {9, 0xa399b44a656d1f51ull}, {13, 0x611e2c3c859fd34eull}, {17, 0x597325fe31326425ull}, {18, 0x6c8029ae5f08bfa3ull}, {23, 0xd5ae0d53b68fb04dull}, {24, 0xf9da1bd89b5fd0cfull}, {25, 0x5681907fabf65ef2ull}}},
    {&Call40, 3, {}, {{1, 0x55740da03c2ffef3ull}, {2, 0x6b98fbca81426ddbull}, {3, 0x8e3c587efbcdfd9eull}, {4, 0x9c923252907a6b60ull}, {5, 0x5275dd66f34c389dull}, {6, 0xb5235f8a39b8b04dull}, {7, 0xd899683c232444d9ull}, {8, 0x0b6156d3d0a21f93ull}, {9, 0xa5cb7686049bdd51ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0xfe8a084a260352eaull}, {15, 0x51d3f6cde4c2c061ull}}},
    {&Call41, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{2, 0x924a53fee31a7943ull}, {3, 0x0d6c51fc74a7ffebull}, {5, 0xc58c31a96d1504f7ull}, {7, 0x1543e7f6bb4d68feull}, {8, 0xbc11db0f3f466e9dull}, {29, 0x005e17bef9d37e39ull}, {30, 0x3d89c53ca137628cull}, {32, 0x767982e7f8a333c8ull}, {33, 0xdcb890364e3d7cdeull}, {41, 0xb1995c08b9bb37dcull}, {44, 0x95002d4ea2d96d33ull}, {50, 0x2adee55002f179b4ull}}},
    {&Call42, 4, {0xfull, 0x10ull, 0x11ull, 0xffffffffffffffefull}, {{0, 0xd6a0652495542840ull}, {1, 0x34a7efa0d6fe498full}, {2, 0x741feec803d44d8aull}, {3, 0x0d6c61fc74a684dbull}, {5, 0x6a7a26924e09264aull}, {6, 0xae67cf25238481ebull}, {7, 0xd27b9701bb0fabafull}, {8, 0xba21f69c5988953full}, {10, 0x790b468b71b35379ull}, {11, 0x892c4bd23a771265ull}, {12, 0xc3a61fbcb6677cd5ull}, {13, 0xfb40bcab4ab16ba8ull}}},
    {&Call43, 0, {}, {{1, 0xd601f84fe7dff17aull}, {2, 0x723037b48ca4a7c9ull}, {3, 0x83eeff1208f1b548ull}, {4, 0x06c6c226cdd597dcull}, {5, 0x33786b5e835bbfd4ull}, {6, 0xd30e884f8834aef0ull}, {7, 0x0c0ccfc453953862ull}, {8, 0x7b7f3750967d2205ull}, {9, 0xc74b6296322e4301ull}, {10, 0x6fcf0a2e87d30117ull}, {11, 0x5b33a3e5ea45e2f3ull}, {12, 0x13f1effa432c4c4aull}}},
    {&Call44, 0, {}, {{1, 0x238f83e0398157c3ull}, {2, 0xc82b4399735dce0full}, {3, 0x357be50a9d215657ull}, {4, 0xaf274d271437388cull}, {5, 0x107ea03a023bd589ull}, {6, 0xc9b02eb2a586ce1aull}, {7, 0xedbdf9cd8d9cb0dcull}, {8, 0x14f515c82406c5e3ull}, {9, 0xd2bb0ccb255cb7d9ull}, {10, 0x5c9e861a137a9b06ull}, {11, 0xb10d986fd8f83d72ull}, {12, 0x772221ddb4c9d4b1ull}}},
    {&Call45, 1, {}, {{1, 0xdfd985b18900d430ull}, {2, 0x63cb5a51fa861b3bull}, {3, 0xad39e9d20c88ecd6ull}, {4, 0x13aa2a47ae35134dull}, {5, 0x0934755ebe48f6c6ull}, {6, 0x6572135906c8a529ull}, {7, 0x863137dd97586a9eull}, {8, 0x94ae19fa0f870726ull}, {9, 0xf4f255617471ac29ull}, {10, 0xff7dcfa3d42092e0ull}, {13, 0x2b5ea9aba951c45aull}, {15, 0xdf929326a568524aull}}},
    {&Call46, 0, {0xffull, 0x100ull, 0x101ull, 0xfffffffffffffeffull}, {{0, 0x68fd8e00ea41c273ull}, {1, 0x4dde116f9f181d07ull}, {2, 0xa6c411a95bed3d32ull}, {3, 0x8bf01c200ffe2708ull}, {5, 0xe3f1212c75a27a29ull}, {6, 0x923488ea62445ad1ull}, {7, 0x5081b5fe0596cfcdull}, {8, 0x09afdd0c29193698ull}, {10, 0xa497e9aeaaa56e62ull}, {11, 0x467678281d94c9b1ull}, {12, 0x297cf03b1f07aa14ull}, {13, 0x6ec195321b13f7f1ull}}},
    {&Call47, 1, {}, {{1, 0xdfd985b18900d430ull}, {2, 0x63cb5a51fa861b3bull}, {3, 0xad39e9d20c88ecd6ull}, {4, 0x13aa2a47ae35134dull}, {5, 0x0934755ebe48f6c6ull}, {6, 0x6572135906c8a529ull}, {7, 0x863137dd97586a9eull}, {8, 0x94ae19fa0f870726ull}, {9, 0xf4f255617471ac29ull}, {10, 0xff7dcfa3d42092e0ull}, {13, 0x2b5ea9aba951c45aull}, {15, 0xdf929326a568524aull}}},
    {&Call48, 0, {0xffull, 0x100ull, 0x101ull, 0xfffffffffffffeffull}, {{0, 0x68fd8e00ea41c273ull}, {1, 0x4dde116f9f181d07ull}, {2, 0xa6c411a95bed3d32ull}, {3, 0x8bf01c200ffe2708ull}, {5, 0xe3f1212c75a27a29ull}, {6, 0x923488ea62445ad1ull}, {7, 0x5081b5fe0596cfcdull}, {8, 0x09afdd0c29193698ull}, {10, 0xa497e9aeaaa56e62ull}, {11, 0x467678281d94c9b1ull}, {12, 0x297cf03b1f07aa14ull}, {13, 0x6ec195321b13f7f1ull}}},
    {&Call49, 4, {}, {{2, 0x1b1be81b6e0da6a9ull}, {3, 0xfd371a6da5e690a2ull}, {4, 0x868af706d57d4e19ull}, {5, 0x89a4ff2039604bb6ull}, {6, 0x41c8109d5e8f775full}, {9, 0xdfa7a5d39a29302aull}, {13, 0x93283b23d8f51ba9ull}, {17, 0xb1178f0c4b670b32ull}, {18, 0xdf86562c835d4fd3ull}, {23, 0xb8e61e0fbd32215cull}, {24, 0x9b776fbdc0fcef70ull}, {25, 0x33b3bfaf4ce87225ull}}},
    {&Call50, 4, {}, {{2, 0x1b1be81b6e0da6a9ull}, {3, 0xfd371a6da5e690a2ull}, {4, 0x868af706d57d4e19ull}, {5, 0x89a4ff2039604bb6ull}, {6, 0x41c8109d5e8f775full}, {9, 0xdfa7a5d39a29302aull}, {13, 0x93283b23d8f51ba9ull}, {17, 0xb1178f0c4b670b32ull}, {18, 0xdf86562c835d4fd3ull}, {23, 0xb8e61e0fbd32215cull}, {24, 0x9b776fbdc0fcef70ull}, {25, 0x33b3bfaf4ce87225ull}}},
    {&Call51, 4, {}, {{5, 0xfcddf85616819e76ull}, {17, 0xa094126af94ac9e7ull}, {19, 0x589b071005db46b9ull}, {25, 0xd0421b7564381daaull}, {26, 0xe3cde125456cc943ull}, {27, 0x7caad6447994514eull}, {30, 0x2b04dcce669d0439ull}, {32, 0xc348af8456eeae64ull}, {35, 0xc6efb2b022c4bf14ull}, {38, 0xd6f78489b90baacfull}, {40, 0x05ab94c4c3b96707ull}, {50, 0x6ad3da525d89931bull}}},
    {&Call52, 1, {0x0ull, 0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0xefull, 0xf0ull, 0xf1ull, 0xfbull, 0xfcull, 0xfdull, 0xffffffffffffff03ull, 0xffffffffffffff0full, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x055dca3a2802849cull}, {1, 0x52960d679cf1f876ull}, {2, 0xb7f9c4e5fd00c421ull}, {3, 0xe54236e380b0ff91ull}, {5, 0xf0d8d50c92569526ull}, {6, 0x7476fd6347f6af3aull}, {7, 0x06e5b57e81530e07ull}, {8, 0x773849f9695795f6ull}, {10, 0xf492ccbe2a9ded3bull}, {11, 0x14b1797c80b1b8fbull}, {12, 0x7dc690c55c9434b7ull}, {13, 0xd972cd6d53ffea34ull}}},
    {&Call53, 1, {0x0ull, 0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0xefull, 0xf0ull, 0xf1ull, 0xfbull, 0xfcull, 0xfdull, 0xffffffffffffff03ull, 0xffffffffffffff0full, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x055dca3a2802849cull}, {1, 0x52960d679cf1f876ull}, {2, 0xb7f9c4e5fd00c421ull}, {3, 0xe54236e380b0ff91ull}, {5, 0xf0d8d50c92569526ull}, {6, 0x7476fd6347f6af3aull}, {7, 0x06e5b57e81530e07ull}, {8, 0x773849f9695795f6ull}, {10, 0xf492ccbe2a9ded3bull}, {11, 0x14b1797c80b1b8fbull}, {12, 0x7dc690c55c9434b7ull}, {13, 0xd972cd6d53ffea34ull}}},
    {&Call54, 3, {}, {{0, 0xddbc6f69213ba157ull}, {1, 0x44dc6a28527e291aull}, {2, 0x34bbb78c1537df92ull}, {3, 0xf04bb976ea64055dull}, {4, 0x37a6dfc0be92a25dull}, {5, 0xb899a0542b5462feull}, {6, 0xd778f95783fc2c94ull}, {7, 0x4706aff289b82faeull}, {8, 0x14057e8cf19d3b98ull}, {9, 0xa2fbe5d82a980661ull}, {10, 0x11a082ad90a226faull}, {11, 0xf04d79d063bb7e4aull}}},
    {&Call55, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call56, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call57, 4, {}, {{0, 0xddbc6f69213ba157ull}, {1, 0xeef8122d02edc092ull}, {2, 0xe7903f8a8dc4a67aull}, {3, 0x2d2f79700e86229dull}, {4, 0x657d87c229117ec5ull}, {5, 0xfc99e8560b35f746ull}, {6, 0x2b7ee1506c121edcull}, {7, 0x3a130ff16065fbceull}, {8, 0x4081a68e55e56e90ull}, {9, 0x56a655df88f15f31ull}, {10, 0xcd3ea2ab756a1d9aull}, {11, 0x438921d5fc6b3ad2ull}}},
    {&Call58, 0, {}, {{1, 0xd3439e479be0f705ull}, {2, 0x07322ca91029a5e8ull}, {3, 0x7cb66783d1e94294ull}, {4, 0xde4d332328c2b3afull}, {5, 0x3a295445cbf14f6bull}, {6, 0x2a5f5c0a795a74deull}, {7, 0xc702b42335e5db67ull}, {8, 0xf5bfe481fc4aa96eull}, {9, 0x7033766af51f4c1cull}, {10, 0x155ca6b75688b3a0ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x29601bf0171016c4ull}}},
    {&Call59, 4, {}, {{1, 0x048734505e231401ull}, {2, 0xe718278a88b87522ull}, {3, 0x3f097804b60701beull}, {4, 0x877812ee7b5f63f6ull}, {5, 0x4fbe7dfa9ac4a38bull}, {6, 0x1210aba64de4ca0dull}, {7, 0xd39bbc9bce41d06eull}, {8, 0x2c169f6557c8d88dull}, {9, 0x732565d195be1e49ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0xe8ba54f768e321b2ull}, {15, 0xd73e4d4ac444a19eull}}},
    {&Call60, 0, {}, {{1, 0xd494ff8098dbd1c8ull}, {2, 0xbf6a69343e1f9603ull}, {3, 0x7e5626a60c0ebbfaull}, {4, 0x05c3f03138aa91fcull}, {5, 0x2418b620435bff20ull}, {6, 0x9ea96cf6ecf0df01ull}, {7, 0xf44b0ffc2c4e977dull}, {8, 0x3c55b42c91a62c3full}, {9, 0x85911376f0a0614bull}, {10, 0x1388910dd8f2fa19ull}, {13, 0x55d7560eb2f33a50ull}, {15, 0x948a783abd4bf93full}}},
    {&Call61, 4, {}, {{1, 0xbd76aea61a465851ull}, {2, 0xd751bcfde7683fe8ull}, {3, 0x109a39a37dd4b7f1ull}, {4, 0xaaed0d682090da05ull}, {5, 0x3732e4e4e4824237ull}, {6, 0xf619d3195a7c0b87ull}, {7, 0x0dce20132d57548cull}, {8, 0x2273d477ae6e4dc7ull}, {9, 0x62df2fb7c6ef189aull}, {10, 0x72b9bb473005ee91ull}, {13, 0x0903d1d44f106329ull}, {15, 0xc6a25cc2232eafd4ull}}},
    {&Call62, 3, {0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffffdull, 0xfffffffeull, 0xffffffffull, 0xffffffff00000001ull}, {{0, 0x6d9d3a8b22bd0500ull}, {1, 0x1aac62d33ad00c3dull}, {2, 0x4a8bb427b15ecc13ull}, {3, 0x9b4144e7974efa35ull}, {5, 0xf3ce47bf6ecf3906ull}, {6, 0xc32949fa056fdcaaull}, {7, 0xc9c671ebfc0758daull}, {8, 0xf2c87aecd29e8557ull}, {10, 0x0acf8f8a315d77d6ull}, {11, 0x9902f220cb6b5286ull}, {12, 0x73308e8fc2951772ull}, {13, 0xab6ad122ad62761aull}}},
    {&Call63, 4, {}, {{1, 0xa6e594a5a1a75b4cull}, {2, 0x852390995a26608eull}, {3, 0xa05e484e434ccc06ull}, {4, 0x6b319856fa792d75ull}, {5, 0xa4faecb935c73837ull}, {6, 0xb5f6786112ecd997ull}, {7, 0x4ebb50dee663c98aull}, {8, 0xe6073d8a2614a439ull}, {9, 0xee59fcded6a3b172ull}, {10, 0xf1acee0865665563ull}, {13, 0x0903d1d44f106329ull}, {15, 0x8196492a00d2442aull}}},
    {&Call64, 3, {}, {{1, 0xe8d29fd65279ae0cull}, {2, 0x439e4b8cdd01945cull}, {3, 0x77f55ad9dec2983full}, {4, 0xb6cf22833c81b870ull}, {5, 0x049155d5706730a3ull}, {6, 0xfec379c66de7a2caull}, {7, 0xbb945e2e90b58f0aull}, {8, 0xd9d749adaa45d5ceull}, {9, 0x8b87c147c2743f64ull}, {10, 0x45c30b7832378df3ull}, {13, 0x1a411f6a35c46f2full}, {15, 0xc2706668ba7b97e8ull}}},
    {&Call65, 3, {0x1ull, 0x2ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffffaull, 0xfffffffbull, 0xfffffffcull, 0xffffffff00000004ull}, {{0, 0x5972198435c92da1ull}, {1, 0xef9d21c7a89523d4ull}, {2, 0x422d41a3c3045685ull}, {3, 0x3580e485dbbd07a1ull}, {5, 0x8c828d1eb4b65321ull}, {6, 0xf7ca3920139ca7e2ull}, {7, 0x912e70272c9b809aull}, {8, 0xf1c3935f51669729ull}, {10, 0xaa66b5c53f256d24ull}, {11, 0x4e8d5cc349562878ull}, {12, 0x8f1ce90d47a7078eull}, {13, 0x9b3a5384626ae0a2ull}}},
    {&Call66, 3, {0x1ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffffcull, 0xfffffffdull, 0xfffffffeull, 0xffffffff00000002ull}, {{0, 0xf00fbd62d56a9e07ull}, {1, 0x6182aa0a2184b0fbull}, {2, 0xac6c47267d61e162ull}, {3, 0x48e90816e0d201aeull}, {5, 0x13c121093e5d9823ull}, {6, 0xd14c84f866bd2ba1ull}, {7, 0xa2f508a0da528498ull}, {8, 0xfcfd045e09f904ffull}, {10, 0xa93aafbb73744dbeull}, {11, 0x6473a61d0e2e3f0bull}, {12, 0x41532139333411b0ull}, {13, 0x7d220723ae456812ull}}},
    {&Call67, 3, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x5ull, 0x8ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xffffffeeull, 0xffffffefull, 0xfffffff0ull, 0xffffffff00000010ull, 0xfffffffffffffffbull}, {{0, 0xf00fbd62d56a9e07ull}, {1, 0xd611b88add5fe0e5ull}, {2, 0x1232f1b3ea6f2469ull}, {3, 0x648300e2488265f4ull}, {5, 0xad03d05f932391fbull}, {6, 0xd65e690d96bba4e6ull}, {7, 0x9cb3c15360524094ull}, {8, 0x3f14db23d731af47ull}, {10, 0x1f77703ec1bbd4ffull}, {11, 0x529dc98eca51529bull}, {12, 0xaaa26123f7b25392ull}, {13, 0xa68aa3eeea25f040ull}}},
    {&Call68, 3, {0x1ull, 0x2ull, 0x4ull, 0x7ull, 0x8ull, 0x9ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffefeull, 0xfffffeffull, 0xffffff00ull, 0xffffffff00000100ull, 0xfffffffffffffff7ull}, {{0, 0x795128860c7c8d6dull}, {1, 0x92f94d6be34784aeull}, {2, 0x1ea7f53b1769a326ull}, {3, 0xbb3b8d955a24b8b2ull}, {5, 0x4dcb0964cbf63fd5ull}, {6, 0x3bfa2d2f23e8a321ull}, {7, 0x74ae2184ad075aedull}, {8, 0x3e9c46f8eadedb6full}, {10, 0xae3962fb5908772dull}, {11, 0xf9608095f3ce0fa1ull}, {12, 0xdc648b7cf2fa4a72ull}, {13, 0x863c09bfdf840638ull}}},
    {&Call69, 3, {0x1ull, 0x2ull, 0x4ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xfffffff6ull, 0xfffffff7ull, 0xfffffff8ull, 0xffffffff00000008ull}, {{0, 0x9e376cc6f693e3feull}, {1, 0x5ddb6a0e61a55bb7ull}, {2, 0x7e38c624698ed463ull}, {3, 0x04088e7558d01478ull}, {5, 0x3d76e3d1aa01ee8full}, {6, 0x649de7dc8f93d3e3ull}, {7, 0x7694e6d508556dd7ull}, {8, 0x732b8ee82fb4d827ull}, {10, 0xdfa9928b603b1b4aull}, {11, 0xc9f88a93af6a8738ull}, {12, 0xfb516a900a8e8d1bull}, {13, 0x270f723ea4df5d86ull}}},
    {&Call70, 4, {}, {{1, 0x829990af143a059cull}, {2, 0x5e2f9b3479ed4d4eull}, {3, 0x4eb0618e79758d02ull}, {4, 0xda7d78cd30b21862ull}, {5, 0x0e09eaae4a9835feull}, {6, 0x7529a1452d5856c5ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x66aab7194c31c70dull}, {9, 0xb17281d0158140c9ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {15, 0xb41d4458364fbc1eull}}},
    {&Call71, 0, {}, {{1, 0xfe1ff47b4411175aull}, {2, 0xea9861636a16ffe0ull}, {3, 0x285d04c7a2635ad2ull}, {4, 0x72a6c9aa77c20c63ull}, {5, 0xfd03d8b000bf1456ull}, {6, 0x532fa2716803c012ull}, {7, 0xd3d7351c7cb1eda1ull}, {8, 0xbacf6c391eea1543ull}, {9, 0xa4a67a913f6fe8a6ull}, {10, 0xe9d387c7541a29e1ull}, {13, 0xbcfa211c50906b8cull}, {15, 0x48dfe9fe89e36fa4ull}}},
    {&Call72, 4, {}, {{1, 0xdc7e78afc58925c2ull}, {2, 0x6ef441c24302b12cull}, {3, 0x31feb878ad1c7aa5ull}, {4, 0x7a20414aa40c56ecull}, {5, 0xb4214f15062e059eull}, {6, 0x5babdf32802ccf3eull}, {7, 0xa27aa6c7238e62ceull}, {8, 0xedbce773275ebd4bull}, {9, 0x78efa2e41e6fef18ull}, {10, 0xdd1ecd863dca52f8ull}, {13, 0xe39052e15c62b91cull}, {15, 0x9937416b584fc6d3ull}}},
    {&Call73, 3, {0x1ull, 0x2ull, 0x4ull, 0x5ull, 0x6ull, 0x8ull, 0x10ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xffffffdeull, 0xffffffdfull, 0xffffffe0ull, 0xffffffff00000020ull, 0xfffffffffffffffaull}, {{0, 0xa66e93bf6033a1d5ull}, {1, 0xa89c22e6e9428944ull}, {2, 0xd7c43fd77ee2d31full}, {3, 0x8d0c7d09b84d93caull}, {5, 0x75b0692acf91bf7eull}, {6, 0x93d4ef7aba3dfaa3ull}, {7, 0x5e23638046e3b6a1ull}, {8, 0x1c6bc9171659bdadull}, {10, 0xce444b79453d674full}, {11, 0x86c43e960d792301ull}, {12, 0xac7f74baa5d4c52dull}, {13, 0x7bce52e5fb0529c4ull}}},
    {&Call74, 3, {}, {{1, 0x465f436f43e15125ull}, {2, 0x2bad80fe7de3a562ull}, {3, 0x99a22d6bea4bf35cull}, {4, 0x84eba13b722125b1ull}, {5, 0xa015a72093fcc3e4ull}, {6, 0x024a6b38006aef25ull}, {7, 0xbc872c4a74f40b9aull}, {8, 0xd948fb23da314a32ull}, {9, 0x8247fb8958bb8668ull}, {10, 0x8debb37d54b6b846ull}, {13, 0x5f4b7582c1ed8e27ull}, {15, 0x56ec1d67ec94610dull}}},
    {&Call75, 4, {}, {{1, 0x4de49d0d6e441158ull}, {2, 0xce2e72024be5e785ull}, {3, 0x6e96362cd9586f3aull}, {4, 0x2a6c9e42d253a576ull}, {5, 0x7a7458f7cd3db06bull}, {6, 0x1cb942811587ac06ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x93f3bb1e81573f46ull}, {9, 0x7bcab4b621fe66d0ull}, {10, 0x1256a2ad8c6ebe9aull}, {13, 0x0903d1d44f106329ull}, {15, 0x5fd6f487cf63a62dull}}},
    {&Call76, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call77, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call78, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call79, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call80, 0, {}, {{1, 0x2a4b00d36f5fc45aull}, {2, 0x7eae292f001d6d0eull}, {3, 0x22c0f24caa7abf8bull}, {4, 0xe304962ad2d9e67cull}, {5, 0xac65e155875ef9fbull}, {6, 0x9bdff4ca23dd82a9ull}, {7, 0x66c90a93fd40ea66ull}, {8, 0x5c1bb3e260fc81b2ull}, {9, 0x3e3b219a4a438c65ull}, {10, 0x6628d310fe82ca37ull}, {11, 0x953bc50b0b7bfd2eull}, {12, 0x01b8bc243aa59f55ull}}},
    {&Call81, 0, {}, {{1, 0x2a4b00d36f5fc45aull}, {2, 0x7eae292f001d6d0eull}, {3, 0x22c0f24caa7abf8bull}, {4, 0xe304962ad2d9e67cull}, {5, 0xac65e155875ef9fbull}, {6, 0x9bdff4ca23dd82a9ull}, {7, 0x66c90a93fd40ea66ull}, {8, 0x5c1bb3e260fc81b2ull}, {9, 0x3e3b219a4a438c65ull}, {10, 0x6628d310fe82ca37ull}, {11, 0x953bc50b0b7bfd2eull}, {12, 0x01b8bc243aa59f55ull}}},
    {&Call82, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xc20ab1811de7b603ull}, {5, 0x9c6d2cd20ac3c120ull}, {6, 0x9dfaef1783767d61ull}, {7, 0x0f610f7a37db9614ull}, {8, 0x2e107f0c0390b214ull}, {11, 0xec90165aa6f3bb56ull}, {12, 0x77d33861605c5a72ull}, {13, 0x48674dbfb3f1e2eaull}, {14, 0xe15f345bcca0f7aaull}, {15, 0xe8b8afd53ce75dabull}, {17, 0x65d1cb7083fef6b3ull}, {19, 0xc3dffab3936c2c97ull}}},
    {&Call83, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xc30e91779d4e7818ull}, {2, 0x9de07de5c24ac32aull}, {3, 0xe9d7448f66fb3333ull}, {5, 0xbf69696b72dd96f4ull}, {6, 0x96a5d2ed4cac280dull}, {7, 0x38eef50a4a54cb53ull}, {8, 0x75a7d1d42c9b1753ull}, {10, 0x7e006e25968ec5a3ull}, {11, 0x349cb322cc01a182ull}, {12, 0x84cf3caf3148f5c8ull}, {13, 0xfe9448014026ee16ull}, {14, 0xd496aed70e10905aull}}},
    {&Call84, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x48ca1bc471c4f0e1ull}, {1, 0x8130322bfb8049c6ull}, {2, 0xb240f3b18425c757ull}, {3, 0xe55469d33bf38188ull}, {5, 0x09d79add1fc1baa8ull}, {6, 0x93861f2ec14691a6ull}, {7, 0x38eef50a4a54cb53ull}, {8, 0x8d45290b1732d25full}, {10, 0xefb81c54f047e157ull}, {11, 0x14fbc052b3484907ull}, {12, 0x84cf3caf3148f5c8ull}, {13, 0xd110bf002e25413cull}}},
    {&Call85, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xc20ab1811de7b603ull}, {5, 0x9c6d2cd20ac3c120ull}, {6, 0x9dfaef1783767d61ull}, {7, 0x0f610f7a37db9614ull}, {8, 0x2e107f0c0390b214ull}, {11, 0xec90165aa6f3bb56ull}, {12, 0x77d33861605c5a72ull}, {13, 0x48674dbfb3f1e2eaull}, {14, 0xe15f345bcca0f7aaull}, {15, 0xe8b8afd53ce75dabull}, {17, 0x65d1cb7083fef6b3ull}, {19, 0xc3dffab3936c2c97ull}}},
    {&Call86, 3, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0xc30e91779d4e7818ull}, {2, 0x9de07de5c24ac32aull}, {3, 0xe9d7448f66fb3333ull}, {5, 0xbf69696b72dd96f4ull}, {6, 0x96a5d2ed4cac280dull}, {7, 0x38eef50a4a54cb53ull}, {8, 0x75a7d1d42c9b1753ull}, {10, 0x7e006e25968ec5a3ull}, {11, 0x349cb322cc01a182ull}, {12, 0x84cf3caf3148f5c8ull}, {13, 0xfe9448014026ee16ull}, {14, 0xd496aed70e10905aull}}},
    {&Call87, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x48ca1bc471c4f0e1ull}, {1, 0x8130322bfb8049c6ull}, {2, 0xb240f3b18425c757ull}, {3, 0xe55469d33bf38188ull}, {5, 0x09d79add1fc1baa8ull}, {6, 0x93861f2ec14691a6ull}, {7, 0x38eef50a4a54cb53ull}, {8, 0x8d45290b1732d25full}, {10, 0xefb81c54f047e157ull}, {11, 0x14fbc052b3484907ull}, {12, 0x84cf3caf3148f5c8ull}, {13, 0xd110bf002e25413cull}}},
    {&Call88, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x846f29492076389cull}, {1, 0x63b198c4cd68d265ull}, {2, 0x7e03cb506b135c7cull}, {3, 0xcf602d892174fe72ull}, {5, 0xa5846a1a4dbadd57ull}, {6, 0x141bb1317d671615ull}, {7, 0x96bd369c3d00573aull}, {8, 0x49abd1f13bdcf3faull}, {10, 0xd2dfe08ca51bfcfcull}, {11, 0xa0b4e2d15cd28807ull}, {12, 0xa37f2ef626e08219ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call89, 0, {}, {{1, 0x11db40f15c100c93ull}, {2, 0x3347278c34486f22ull}, {3, 0x30d380ab63fd3bdaull}, {4, 0x3626dfc0b021225dull}, {5, 0x448320c0b116dae4ull}, {6, 0x55c5e8b09d4043d4ull}, {7, 0x66c90a93fd40ea66ull}, {8, 0xf44942704adf94f2ull}, {9, 0xea255b8887a79edbull}, {10, 0x5d181fd63970984bull}, {11, 0x953bc50b0b7bfd2eull}, {12, 0x75786132d5507155ull}}},
    {&Call90, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull}, {{0, 0x9a7169015cf32688ull}, {1, 0x899a090d0c4cc7e4ull}, {2, 0xd40a0921ae453b4aull}, {3, 0xfc403f5c7a4b0da0ull}, {5, 0x4175e4fe82bfd30full}, {6, 0x2b50816e0a578c21ull}, {7, 0x97dc85ee7005f981ull}, {8, 0x82be8d5b6cef3986ull}, {10, 0x39f3a43efc33605aull}, {11, 0xf75937383d73c7a3ull}, {12, 0xfa9bf374ba9c1d06ull}, {13, 0x303ede33e85467fdull}}},
    {&Call91, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull}, {{0, 0x9a7169015cf32688ull}, {1, 0x899a090d0c4cc7e4ull}, {2, 0xd40a0921ae453b4aull}, {3, 0xfc403f5c7a4b0da0ull}, {5, 0x4175e4fe82bfd30full}, {6, 0x2b50816e0a578c21ull}, {7, 0x97dc85ee7005f981ull}, {8, 0x82be8d5b6cef3986ull}, {10, 0x39f3a43efc33605aull}, {11, 0xf75937383d73c7a3ull}, {12, 0xfa9bf374ba9c1d06ull}, {13, 0x303ede33e85467fdull}}},
    {&Call92, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829ba324bc2780aull}, {2, 0x201bf2c2ae60c7a3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xfdc0a02b5d8933ebull}, {6, 0xdfddd8cb94f7a818ull}, {7, 0x9673313d8913aec5ull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xcbcb4b86c0746111ull}, {11, 0xc9a4d5d12516dcf5ull}, {12, 0xa831fa0aa7167c27ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call93, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x2ce17a6156834292ull}, {1, 0x86cb5f72539d9235ull}, {2, 0xf89b98528a3f88ccull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x1b1a11af98814f57ull}, {6, 0xfdd80cf979d3e460ull}, {7, 0x160a3c4a01242cc9ull}, {8, 0x83c068950253634dull}, {10, 0x6f0df91966e44695ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x70f5856a46caea08ull}, {13, 0xd110bf002e25413cull}}},
    {&Call94, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x142f68d349d228c6ull}, {2, 0xa0cd8ea818360adaull}, {3, 0x54ea92d913adec19ull}, {5, 0xa3552d6fd395bd7bull}, {6, 0x7fb6fdec614f6824ull}, {7, 0x160a3c4a01242cc9ull}, {8, 0x0e2598af666c0d27ull}, {10, 0xfa9587fbaba627e3ull}, {11, 0xe4705388de0ede5full}, {12, 0x213671cc6d16f94cull}, {13, 0x9dac8059e7f49aacull}, {14, 0xed2e5f6bf06b39a3ull}}},
    {&Call95, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x142f68d349d228c6ull}, {2, 0xa0cd8ea818360adaull}, {3, 0x54ea92d913adec19ull}, {5, 0xa3552d6fd395bd7bull}, {6, 0x7fb6fdec614f6824ull}, {7, 0x160a3c4a01242cc9ull}, {8, 0x0e2598af666c0d27ull}, {10, 0xfa9587fbaba627e3ull}, {11, 0xe4705388de0ede5full}, {12, 0x213671cc6d16f94cull}, {13, 0x9dac8059e7f49aacull}, {14, 0xed2e5f6bf06b39a3ull}}},
    {&Call96, 0, {}, {{1, 0xc3ec2def5be72c5dull}, {2, 0x46abb7089f75ed94ull}, {3, 0xb1d14d449e7fe83full}, {4, 0x136d9d0017ea4171ull}, {5, 0xa0831f730a3a2901ull}, {6, 0x19f0ffbe58086c08ull}, {7, 0xf4c6c3e278b48ee7ull}, {8, 0x3dd109eb4d2666c3ull}, {9, 0xd3ce9a2a837e8857ull}, {10, 0x8778c401a2a32605ull}, {11, 0xde2a3949ff560cb4ull}, {12, 0x9d8fea07c239c552ull}}},
    {&Call97, 3, {}, {{11, 0xf04d79d063bb7e4aull}, {12, 0x3d3f902168b24344ull}, {19, 0x589b081005db3f74ull}, {21, 0x8265dbb3fb2d9e9full}, {30, 0x26bcccce0806f269ull}, {33, 0x7b76d0bdfb990ffaull}, {44, 0x68c5891a8d7a0b91ull}, {55, 0xf4ee545bd3b67905ull}, {57, 0x0d98d0c47b863571ull}, {60, 0x25015b8f2cfb8f06ull}, {73, 0x2ab52334a06c46fcull}, {80, 0x6768c1632b6d3f5bull}}},
    {&Call98, 3, {}, {{11, 0xcad13278036cb191ull}, {12, 0x67d9f5d7e6491263ull}, {19, 0x0223b12e06c8507aull}, {21, 0xac77b6741b02f569ull}, {30, 0xdb5c03f20d98eedeull}, {33, 0xf502c75cff9a8410ull}, {44, 0x9a0bcd3d7d90d204ull}, {55, 0xa68b8541880a4497ull}, {57, 0xb406ae2f58c46b18ull}, {60, 0x6a294e45cb3a2248ull}, {73, 0xc85c94a4c5411457ull}, {80, 0x0e758f75cda77c67ull}}},
    {&Call99, 1, {}, {{1, 0x44423a285186a9eaull}, {3, 0xf134d976e0633bbdull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {15, 0x7d21b0bde31fc437ull}, {17, 0xa094126af94ac9e7ull}, {23, 0xad9360f23ee241d4ull}, {24, 0x40de8fb4aae9c17bull}, {29, 0x59fe1ceb4e87b4a3ull}, {31, 0x72a23a86695fcbaeull}}},
    {&Call100, 0, {}, {{1, 0xbbe70e5dbeb0e383ull}, {2, 0xfa41a434a43b3e7eull}, {3, 0x05e03c262fea5dc9ull}, {4, 0x57f766b844b740d1ull}, {5, 0x18a0c931b17409d0ull}, {6, 0x8b5b5a8fb10daab0ull}, {7, 0x09a95df8514d9a75ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xcb00c410807113cfull}, {10, 0xd83715440e258455ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x9c9bce4f61fa7482ull}}},
    {&Call101, 0, {}, {{1, 0xa707374dd0d92d12ull}, {2, 0x19f9a2ed22bd2048ull}, {3, 0x692dd8c0967f5617ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xaaf4ad69756e3d17ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x43b946be7645e690ull}, {9, 0x0659e66949e0e4b2ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x2079f55108948a96ull}}},
    {&Call102, 3, {}, {{1, 0xc9ab4c9c2583a8f0ull}, {2, 0xdb40e546b8e17402ull}, {3, 0x0213b3f6274be8e1ull}, {4, 0x6da1d631c97dfb8full}, {5, 0x72d27635df49821bull}, {6, 0x14150a552a43fc0full}, {7, 0x486357f2f0c77e26ull}, {8, 0x9a84885e8cd4d6c1ull}, {9, 0xc6826700f4fd81f7ull}, {10, 0x4d56a95003d5de5aull}, {13, 0x0903d1d44f106329ull}, {15, 0xed820f6c41cec129ull}}},
    {&Call103, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x0228a30a7b503340ull}, {1, 0x4a9d6da7dc6301e7ull}, {2, 0x4374c50e3708e692ull}, {3, 0x7b0e39bdacf1eabeull}, {5, 0xb410f4c9669765d9ull}, {6, 0x9f6d347229effd59ull}, {7, 0x4d3d2cba298c5414ull}, {8, 0xe33be8878c105e6aull}, {10, 0x6188dbd3c5545091ull}, {11, 0x75962bc380a8038aull}, {12, 0xebf2afd626300e48ull}, {13, 0x9bd006c33ada20e2ull}}},
    {&Call104, 0, {}, {{1, 0x00c7441e41b8cddaull}, {2, 0x4fde59c6094c43e5ull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0x1c5ee53f2f7a00a7ull}, {6, 0x12db17d12db44819ull}, {7, 0x3182cfed7f16f45bull}, {8, 0x6b4cf885fd7811e8ull}, {9, 0xffb31556b2709f9full}, {10, 0xcbf875060e2f3408ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0xa982911513682a42ull}}},
    {&Call105, 1, {0x0ull, 0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0xfcull, 0xfdull, 0xfeull, 0xffffffffffffff02ull, 0xfffffffffffffffeull}, {{0, 0x4ff9ac6b04f9dda5ull}, {1, 0x8cd315ba13ec338dull}, {2, 0x67e22bc38fdd2a33ull}, {3, 0xc44871fa3ca4828bull}, {5, 0xfd6f46b25ddbbc76ull}, {6, 0x64c9e2561adf443bull}, {7, 0x120df344bb2c933full}, {8, 0xd426aeeebb19d8eaull}, {10, 0x3d8cc6019955e704ull}, {11, 0xdf71c1feb8cf63e4ull}, {12, 0xf6cac31a40240b59ull}, {13, 0xd4ed1015359eabadull}}},
    {&Call106, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x154fb12f534b08aeull}, {1, 0x6b1b140df1b03675ull}, {2, 0x22ace122d9929b43ull}, {3, 0x3b85d23dd137356full}, {5, 0x15d8848fe0b7a32dull}, {6, 0xe65223c64e07556bull}, {7, 0xaf2fcc00dfbc0082ull}, {8, 0x5b990cb1a8116d4full}, {10, 0x09a467ed74a7648dull}, {11, 0x830ccb1ca5cfd085ull}, {12, 0x1bac9570fd567997ull}, {13, 0x4174eefdd3e0f680ull}}},
    {&Call107, 0, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x154fb12f534b08aeull}, {1, 0x6b1b140df1b03675ull}, {2, 0x22ace122d9929b43ull}, {3, 0x3b85d23dd137356full}, {5, 0x15d8848fe0b7a32dull}, {6, 0xe65223c64e07556bull}, {7, 0xaf2fcc00dfbc0082ull}, {8, 0x5b990cb1a8116d4full}, {10, 0x09a467ed74a7648dull}, {11, 0x830ccb1ca5cfd085ull}, {12, 0x1bac9570fd567997ull}, {13, 0x4174eefdd3e0f680ull}}},
    {&Call108, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0xa010c4ed80981b0cull}, {1, 0xa085064445d4b193ull}, {2, 0x2d8b6b894e3f5db1ull}, {3, 0xe55469d33bf38188ull}, {5, 0xa5846a1a4dbadd57ull}, {6, 0x9b0cb356a0f4a17dull}, {7, 0x96bd369c3d00573aull}, {8, 0xc8f93e2d2608f7fdull}, {10, 0x87d7b9524107127eull}, {11, 0x5ac1b63d94a02205ull}, {12, 0xe471c570916bd08full}, {13, 0x688cd42ead663b43ull}}},
    {&Call109, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0xa010c4ed80981b0cull}, {1, 0xa085064445d4b193ull}, {2, 0x2d8b6b894e3f5db1ull}, {3, 0xe55469d33bf38188ull}, {5, 0xa5846a1a4dbadd57ull}, {6, 0x9b0cb356a0f4a17dull}, {7, 0x96bd369c3d00573aull}, {8, 0xc8f93e2d2608f7fdull}, {10, 0x87d7b9524107127eull}, {11, 0x5ac1b63d94a02205ull}, {12, 0xe471c570916bd08full}, {13, 0x688cd42ead663b43ull}}},
    {&Call110, 4, {}, {{2, 0xc46c325cc522754eull}, {3, 0xec3109db3aff3c35ull}, {4, 0xe8616c95a0ac1952ull}, {5, 0x7c02c5a79f1b6b21ull}, {6, 0x69eca10bd85a87e1ull}, {9, 0x6609f9473a56faecull}, {13, 0xfe2a6d8c06e9baecull}, {17, 0x1582c92e4a19af0aull}, {18, 0x104638ff205fb316ull}, {23, 0xb86a9686f44b30d3ull}, {24, 0x2eed55dda7041c29ull}, {25, 0xd28f170dc853c4bfull}}},
    {&Call111, 4, {}, {{2, 0xc46c325cc522754eull}, {3, 0xec3109db3aff3c35ull}, {4, 0xe8616c95a0ac1952ull}, {5, 0x7c02c5a79f1b6b21ull}, {6, 0x69eca10bd85a87e1ull}, {9, 0x6609f9473a56faecull}, {13, 0xfe2a6d8c06e9baecull}, {17, 0x1582c92e4a19af0aull}, {18, 0x104638ff205fb316ull}, {23, 0xb86a9686f44b30d3ull}, {24, 0x2eed55dda7041c29ull}, {25, 0xd28f170dc853c4bfull}}},
    {&Call112, 0, {}, {{1, 0xe291d3d6147aea77ull}, {2, 0xbd925b368d751eebull}, {3, 0x383a6b8b7ba788d0ull}, {4, 0x00afc3e87662ae8cull}, {5, 0x9c92f99b0a496378ull}, {6, 0x4efe954013b5d925ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x778a3b9ab0f4f4b7ull}, {10, 0xb6729d613393dfc1ull}, {13, 0x0903d1d44f106329ull}, {15, 0x3edd539114fd606eull}}},
    {&Call113, 4, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x516c00308bb0b576ull}, {1, 0x78502cc483f72bafull}, {2, 0x4992d86f0e980489ull}, {3, 0x632ae9abfba9f05bull}, {5, 0xf97537beadd41dd4ull}, {6, 0xbeceb25705707090ull}, {7, 0x0236335ee26a824cull}, {8, 0x56e4996dd6e9acc1ull}, {10, 0x38b1f79f242c8b31ull}, {11, 0x5b10f669984efc20ull}, {12, 0x5ff1bef5d65a64ecull}, {13, 0x77816635e8d468feull}}},
    {&Call114, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call115, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call116, 1, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{0, 0xdb174ef8be55e485ull}, {1, 0x1b243c89fd00b3cbull}, {2, 0xa30991ea17fdc0b7ull}, {3, 0x467192f91a2c5785ull}, {5, 0x53936668da36e819ull}, {6, 0xa2f33f527d9e517full}, {7, 0x8054201ccf8bd947ull}, {8, 0x79f70bd7e191268full}, {10, 0x33d609d4040c6119ull}, {11, 0xcc472ae7050d5aceull}, {12, 0x71978a5a0a99e3d1ull}, {13, 0x04250b4cdce826a1ull}}},
    {&Call117, 0, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0xa226e18b8ae6101eull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xb79d97d74b7f6db8ull}, {5, 0xbb08bc3224b10a0bull}, {6, 0x69c1cbf411496279ull}, {7, 0x0dc2eb1a752c35e0ull}, {8, 0x76f2146897780797ull}, {10, 0xb0075978d7d67dffull}, {11, 0x7573497f64a7e2deull}, {12, 0xa22a90dd68e12efeull}, {13, 0x688cd42ead663b43ull}}},
    {&Call118, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x5217837c90a3f4eaull}, {4, 0xa2616403489c6cdcull}, {5, 0x0f5a7fcd3523d208ull}, {6, 0x56c6d5687d7c53f8ull}, {9, 0x41382a6b5e372486ull}, {13, 0xfef027ab412abaa9ull}, {17, 0x4271084ffcd0002eull}, {18, 0x0d6279c14a48097cull}, {23, 0x65926691d5c4f093ull}, {24, 0xa1e733b5be73c259ull}, {25, 0x4f9a5a4a6354b16aull}}},
    {&Call119, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x5217837c90a3f4eaull}, {4, 0xa2616403489c6cdcull}, {5, 0x0f5a7fcd3523d208ull}, {6, 0x56c6d5687d7c53f8ull}, {9, 0x41382a6b5e372486ull}, {13, 0xfef027ab412abaa9ull}, {17, 0x4271084ffcd0002eull}, {18, 0x0d6279c14a48097cull}, {23, 0x65926691d5c4f093ull}, {24, 0xa1e733b5be73c259ull}, {25, 0x4f9a5a4a6354b16aull}}},
    {&Call120, 4, {}, {{2, 0x6f7c07921680ed0eull}, {3, 0xd7a177689733bd2bull}, {4, 0x41b960310b3b9268ull}, {5, 0x7188760bed8eeb6cull}, {6, 0x56bbc71b789e7edfull}, {9, 0xb935ee5c163f14acull}, {13, 0x934edb1b4c5f465eull}, {17, 0x51c1797f521661c1ull}, {18, 0xb9a04f109852c09aull}, {23, 0xadd23b155f801657ull}, {24, 0xd568e0a2bed0a305ull}, {25, 0x096b0fc40be7b96cull}}},
    {&Call121, 4, {}, {{2, 0x6f7c07921680ed0eull}, {3, 0xd7a177689733bd2bull}, {4, 0x41b960310b3b9268ull}, {5, 0x7188760bed8eeb6cull}, {6, 0x56bbc71b789e7edfull}, {9, 0xb935ee5c163f14acull}, {13, 0x934edb1b4c5f465eull}, {17, 0x51c1797f521661c1ull}, {18, 0xb9a04f109852c09aull}, {23, 0xadd23b155f801657ull}, {24, 0xd568e0a2bed0a305ull}, {25, 0x096b0fc40be7b96cull}}},
    {&Call122, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call123, 4, {}, {{2, 0x32c4a969673bc331ull}, {3, 0x4a61237c51b9ff0aull}, {4, 0x3cb1fbedc7c74302ull}, {5, 0xf4a50f9c9092ac48ull}, {6, 0x8b48fb4acc4a44d8ull}, {9, 0xb3d0320c667299c6ull}, {13, 0xb64ddfa9062b5f11ull}, {17, 0xfc3772d60ee8a325ull}, {18, 0x82eace3ccb5d849eull}, {23, 0x400b9c504c422475ull}, {24, 0x2ff02df79dfb947cull}, {25, 0x58d910e69096c5a0ull}}},
    {&Call124, 0, {}, {{1, 0x324e5f6d0c67f9e3ull}, {2, 0xf081878a38219f72ull}, {3, 0xf82b55ebe129f6a8ull}, {4, 0x5f78de99e5c48b3eull}, {5, 0x3a296a3ee53dfa11ull}, {6, 0x93960693d4331311ull}, {7, 0x368b3a9d0c39dd14ull}, {8, 0xa0c9a177899bf809ull}, {9, 0x86004b8f6d8bdb38ull}, {10, 0x639340960daf0c4full}, {11, 0x8cad5ab30d2a2ae8ull}, {12, 0x9c0fa3b08717ab17ull}}},
    {&Call125, 4, {0x1ull, 0x2ull, 0x4ull, 0x6ull, 0x7ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0xffffff7eull, 0xffffff7full, 0xffffff80ull, 0xffffffff00000080ull, 0xfffffffffffffff8ull}, {{1, 0x55c7985ba1713364ull}, {2, 0x109c4fa3aaab53dcull}, {3, 0x06973e242786213cull}, {8, 0xe788e685fd4d1c23ull}, {10, 0xc0142008da53aca6ull}, {14, 0xb036006787ae260dull}, {15, 0x10198434d8b3b82eull}, {19, 0xafa54768effb5754ull}, {20, 0xfb15708da396a91cull}, {21, 0x0dee6c408aac8ba5ull}, {29, 0x2cdf2793afdc3d46ull}, {32, 0x9680b30deef90fc9ull}}},
    {&Call126, 4, {}, {{1, 0x8c5bb5f07f622155ull}, {2, 0x3de5ca28a2b46204ull}, {3, 0x7e8ea58d53423072ull}, {4, 0x657d87c229117ec5ull}, {5, 0xfc99e8560b35f746ull}, {6, 0xa20a15598563e399ull}, {7, 0x297762918654f7eeull}, {8, 0x4081a68e55e56e90ull}, {9, 0x8c50c9df07df1c27ull}, {10, 0xcd3ea2ab756a1d9aull}, {13, 0x7fa5f9d7f5019221ull}, {15, 0xddb08caa2f72881bull}}},
    {&Call127, 4, {}, {{1, 0x8c5bb5f07f622155ull}, {2, 0x3de5ca28a2b46204ull}, {3, 0x3b2db157c59fb739ull}, {4, 0x657d87c229117ec5ull}, {5, 0xfc99e8560b35f746ull}, {6, 0xa20a15598563e399ull}, {7, 0x297762918654f7eeull}, {8, 0x4081a68e55e56e90ull}, {9, 0xc240d450605cf460ull}, {10, 0xcd3ea2ab756a1d9aull}, {13, 0x7fa5f9d7f5019221ull}, {15, 0x62822382aa06cfddull}}},
    {&Call128, 3, {}, {{2, 0x3347278c34486f22ull}, {9, 0xa32c7dd824fa5439ull}, {15, 0x7d21f6bde31d0d49ull}, {25, 0xd04223756439c852ull}, {26, 0xe3cde125456cc943ull}, {32, 0x4a3fd7801cc0847cull}, {37, 0x6e3fb112aca7acd3ull}, {38, 0x361b348ea0dc61bfull}, {40, 0x05ab94c4c3b96707ull}, {42, 0x0216300095541cb4ull}, {48, 0x426bd33214f177d5ull}, {53, 0x777f49c0d1e985fcull}}},
    {&Call129, 1, {}, {{2, 0x3347268c344865f1ull}, {32, 0x4a3fd7801cc0847cull}, {38, 0x37089b8ea9ddf7b4ull}, {48, 0x426bd33214f177d5ull}, {85, 0x1c1f3183577ba0daull}, {101, 0x22f6901d6cd3f4eeull}, {119, 0x6b2d972c5cf12ddbull}, {120, 0xf05f33576783773eull}, {128, 0xdf2eebcb554bc078ull}, {146, 0xd0abc43feb5a208eull}, {150, 0x1a3bd9d5f3fb2b33ull}, {156, 0x446acd658400d52full}}},
    {&Call130, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a96353c02624cull}, {1, 0xd529ba30a3c2780aull}, {2, 0x8859bf5d74a4ddabull}, {3, 0x0d81d9fc73e4c9c3ull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x1c19c876025f1a33ull}, {7, 0x81809bc550505274ull}, {8, 0xaa49b8a19ac1dc41ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45a73b1df9f86ull}, {12, 0x344bef86e5dd3682ull}, {13, 0x27424c2cdd61a97bull}}},
    {&Call131, 6, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134e276e062ec0aull}, {4, 0x3626dfc0b021225dull}, {5, 0xb8590e542d5112feull}, {6, 0x2b3c61157a45d934ull}, {7, 0x474431f28adc0bceull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x0c81b9050d1c98d7ull}, {10, 0x12569f478c6f51caull}, {11, 0xef33d3d0a6b96013ull}, {12, 0x94608a307166215aull}}},
    {&Call132, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829bb324bc272ddull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44830fa3ca71918ull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1caf411496baaull}, {7, 0x81809cc55050683bull}, {8, 0x4dde0fa6a836811aull}, {10, 0xb0075878d7d67788ull}, {11, 0x8ef45a73b1df9f86ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call133, 4, {0x4ull, 0x8ull, 0xbull, 0xcull, 0xdull, 0xfffffffffffffff3ull}, {{0, 0x376a9c353c023fbeull}, {1, 0x650d5d10214c2461ull}, {2, 0xf5942f319240453full}, {3, 0x0de5ddfc4fbd170eull}, {5, 0xf8518fe0f0a3367cull}, {6, 0x69c1d7f41149ed9dull}, {7, 0x12737a5a9eb6d010ull}, {8, 0x19eaee6dca8ff461ull}, {10, 0xb1a1afea5239774bull}, {11, 0x8eee66a7448ead1cull}, {12, 0x602c00b3d7518510ull}, {13, 0xe8afa0383897a2c7ull}}},
    {&Call134, 4, {0x8ull, 0x10ull, 0x17ull, 0x18ull, 0x19ull, 0xffffffffffffffe7ull}, {{0, 0x376a88353c02fdbaull}, {1, 0xd36aa720da9fe0b5ull}, {2, 0x457026f0a74d74b2ull}, {3, 0x0de429fc4fb05bf2ull}, {5, 0x7b714614d9d74470ull}, {6, 0x69c1c3f411492f91ull}, {7, 0xa9ac68090196fb43ull}, {8, 0x04f578cca4c47c0aull}, {10, 0x52384fe9cdb3c96dull}, {11, 0x6f9b3cfab3adfe16ull}, {12, 0x602c0cb3d751f17cull}, {13, 0xbaa262fef7179628ull}}},
    {&Call135, 4, {0x1full, 0x20ull, 0x21ull, 0xffffffffffffffdfull}, {{0, 0x376a70353c0512f2ull}, {1, 0x31b17462dfbe77a1ull}, {2, 0xa81c2dd0b62186a1ull}, {3, 0x0de441fc4fb3333aull}, {5, 0xccb84295c88d5817ull}, {6, 0x69c1abf4114a58d9ull}, {7, 0x46b46a5b0571e97cull}, {8, 0x568ab029f95fd1feull}, {10, 0x21b0461397928187ull}, {11, 0xcc1089059db2a48dull}, {12, 0x602cd4b3d7544d54ull}, {13, 0x82efdac6e85cb6e1ull}}},
    {&Call136, 4, {}, {{1, 0x44423a285186a9eaull}, {3, 0x2ce6a97000b7642dull}, {5, 0xfcbc485615d75c66ull}, {6, 0xd870a257fa65a8c1ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {15, 0x229afd0a39aa6d5eull}, {17, 0x0554ea6fd18065dfull}, {23, 0xad9360f23ee241d4ull}, {24, 0x40dde9b4aaf3ef09ull}, {29, 0x59fd4ceb4e9f9533ull}, {31, 0x72a17a8669470d6eull}}},
    {&Call137, 4, {}, {{3, 0xf134d976e0633bbdull}, {6, 0x2bd9915066b3a7ecull}, {12, 0x3d7990216a7dd144ull}, {15, 0xd46468b8a8f6809full}, {24, 0x8d4377b2c06addb3ull}, {26, 0xe8f0c9251ad4a29bull}, {28, 0xcecf8b45b68b5050ull}, {29, 0x59fd4ceb4e9f9533ull}, {30, 0x2a7cbcce6e317599ull}, {32, 0xc348af8456eeae64ull}, {33, 0xe07fc8b91e18b183ull}, {35, 0xc6efb2b022c4bf14ull}}},
    {&Call138, 3, {}, {{3, 0xf134d976e0633bbdull}, {6, 0xd83aa957f8504ea4ull}, {12, 0x3d7990216a7dd144ull}, {15, 0x7cec00bdeed70247ull}, {24, 0x40dde7b4aaf3a2e3ull}, {26, 0xe3cde125456cc943ull}, {28, 0xcecf8b45b68b5050ull}, {29, 0x59fd4ceb4e9f9533ull}, {30, 0x7e63a5825bcc3ac6ull}, {32, 0x4a3fd7801cc0847cull}, {33, 0x7b76d0bdfb990ffaull}, {35, 0xc6a9b2b0200fad14ull}}},
    {&Call139, 1, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x7ull, 0x8ull, 0x9ull, 0xfffffffffffffff7ull, 0xfffffffffffffffcull}, {{8, 0xc10fbdf3e885c03cull}, {14, 0x41a3c03556109025ull}, {15, 0xc19d1d325a76e025ull}, {17, 0x2919c656fc51c322ull}, {24, 0xd155d284700107d5ull}, {26, 0x28a64657d60acd59ull}, {29, 0xb98c06172526cff8ull}, {31, 0x02765fb7136f8fabull}, {33, 0x81eb6834a3ae74e6ull}, {39, 0xa8f7427de2d0a98bull}, {40, 0x646ea8de8da03fbfull}, {41, 0x6eea60dd30acf973ull}}},
    {&Call140, 1, {0x7ffffull, 0x80000ull, 0x80001ull, 0xfffffffffff7ffffull}, {{0, 0x376a90353c024812ull}, {2, 0x47d9806d6c1a54a6ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x5f55a83bcdadff79ull}, {6, 0x69c1cbf411496279ull}, {8, 0x6911f4413149874dull}, {10, 0xdb651f9b40e72bb3ull}, {13, 0x17c2bf08eae448d7ull}, {14, 0xc553ab233cd96694ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0xd82f91ca475d927dull}}},
    {&Call141, 1, {0x7ull, 0x8ull, 0x9ull, 0xfffffffffffffff7ull}, {{0, 0x376a90353c024812ull}, {2, 0x61b84ee0d7634928ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x65af302dbf0ce4a4ull}, {6, 0x69c1cbf411496279ull}, {8, 0x7e21d59560ec4796ull}, {10, 0x361835d0f17e75bcull}, {13, 0x73acf68f2c1c4081ull}, {14, 0x5b633a1dc7fe3636ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0xdfdc3415744318fdull}}},
    {&Call142, 1, {0x20ull, 0x400ull, 0x800ull, 0x8000ull, 0x20000ull, 0x80000ull, 0x200000ull, 0x2a8c1full, 0x2a8c20ull, 0x2a8c21ull, 0xffffffffffd573dfull}, {{0, 0x376a91353c0242cdull}, {2, 0x73245e8435c0aa85ull}, {3, 0xc44830fa3ca71918ull}, {5, 0x77acba6fb4e700a4ull}, {6, 0x69c1caf411496baaull}, {8, 0x03dc54af79c1cb9full}, {10, 0x0590a5d4eef97d9bull}, {13, 0x70b6f90d19c8bb7bull}, {14, 0x86b126bc5594f302ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d38470010962ull}, {29, 0xd8f924feeddbaf1dull}}},
    {&Call143, 1, {0x3ffffull, 0x40000ull, 0x40001ull, 0xfffffffffffbffffull}, {{0, 0x376a90353c024812ull}, {2, 0x1157def8077178e8ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x63661f77bafd2d57ull}, {6, 0x69c1cbf411496279ull}, {8, 0x0513e2faea611d6bull}, {10, 0xb2449cd466d9bb08ull}, {13, 0x4a6c6886b1bc5030ull}, {14, 0xfdafe7d3d778f50aull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x6b240d3f66756482ull}}},
    {&Call144, 1, {0xfffffull, 0x100000ull, 0x100001ull, 0xffffffffffefffffull}, {{0, 0x376a90353c024812ull}, {2, 0xc61c61285da5f4c3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xba9398edc145e16bull}, {6, 0x69c1cbf411496279ull}, {8, 0x8a210ffd98e86a65ull}, {10, 0xee461cb765d77a44ull}, {13, 0xecc8e41985fdc47full}, {14, 0x51e79985364e33faull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x9714adc30731066eull}}},
    {&Call145, 1, {0x1full, 0x20ull, 0x21ull, 0xffffffffffffffdfull}, {{0, 0x376a90353c024812ull}, {2, 0xf47325d25765f2d9ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xccb7cc35c8f0be57ull}, {6, 0x69c1cbf411496279ull}, {8, 0x54c58829e93929f6ull}, {10, 0xbfc5de1767bd05cfull}, {13, 0x82efbac6e85ffc81ull}, {14, 0x48197bc8ce45c605ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x662534f85a33c276ull}}},
    {&Call146, 1, {0x3full, 0x40ull, 0x41ull, 0xffffffffffffffbfull}, {{0, 0x376a90353c024812ull}, {2, 0xbd6f283bf5f121c8ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xd62dc5a491cb25feull}, {6, 0x69c1cbf411496279ull}, {8, 0x4f0ff70c69a13447ull}, {10, 0x5856cf4f9bc0f863ull}, {13, 0xab6d0564f02d3e18ull}, {14, 0xcc554efcb42bd6ddull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0xc5cc6e3d6fc89eb4ull}}},
    {&Call147, 1, {0x1ffull, 0x200ull, 0x201ull, 0xfffffffffffffdffull}, {{0, 0x376a90353c024812ull}, {2, 0xf813b4501997b309ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x767c7ba22dfa3c02ull}, {6, 0x69c1cbf411496279ull}, {8, 0x3363c68e2c66d1e9ull}, {10, 0x297589f0af83cd6full}, {13, 0xaaee8c52638368eaull}, {14, 0xf60f61af512f3da6ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x70902f4119c53307ull}}},
    {&Call148, 1, {0x3ffull, 0x400ull, 0x401ull, 0xfffffffffffffbffull}, {{0, 0x376a90353c024812ull}, {2, 0xc72cc89384a4bf20ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x010e6c0238b2016full}, {6, 0x69c1cbf411496279ull}, {8, 0x5f05fef0165e069full}, {10, 0x69f0b64d650a3223ull}, {13, 0xf6c8c6fdbfc9b883ull}, {14, 0xec52c83a8fbf35bcull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x9d3e01812a352ab4ull}}},
    {&Call149, 1, {0x7fffull, 0x8000ull, 0x8001ull, 0xffffffffffff7fffull}, {{0, 0x376a90353c024812ull}, {2, 0xde084b559e3c8402ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xd614b11eec557974ull}, {6, 0x69c1cbf411496279ull}, {8, 0x292d095b57dcf27eull}, {10, 0x78ca815c13b6fb9eull}, {13, 0x0ab3cf4ea6e3373eull}, {14, 0x4b85e4976cf0bccfull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x3e543f068944fd0bull}}},
    {&Call150, 1, {0x7ffull, 0x800ull, 0x801ull, 0xfffffffffffff7ffull}, {{0, 0x376a90353c024812ull}, {2, 0xa2e81957ff4ee929ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xc4e2b6dad4347f8full}, {6, 0x69c1cbf411496279ull}, {8, 0x71264ca8d3d88419ull}, {10, 0xb37d5b073a5e4902ull}, {13, 0x60113174dc17666full}, {14, 0x3a91ef5fcadb14c5ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x223a595415580f42ull}}},
    {&Call151, 1, {0x1ffffull, 0x20000ull, 0x20001ull, 0xfffffffffffdffffull}, {{0, 0x376a90353c024812ull}, {2, 0xd246a7671c3ba7f0ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x04964db61c32369full}, {6, 0x69c1cbf411496279ull}, {8, 0x6bb8355124874972ull}, {10, 0x285cc465f92e65a8ull}, {13, 0x242e748353c343bdull}, {14, 0xaa876a865077fb78ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x8dc763f83b0345dcull}}},
    {&Call152, 1, {0xfull, 0x10ull, 0x11ull, 0xffffffffffffffefull}, {{0, 0x376a90353c024812ull}, {2, 0xbe90e6ce54fe7a62ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xbf7bf694e018f8baull}, {6, 0x69c1cbf411496279ull}, {8, 0x52ecce9b1078d537ull}, {10, 0x790b39ed71b08379ull}, {13, 0xfb40ccab4ab2cc98ull}, {14, 0x7527da9d0f2e2337ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d284700107d5ull}, {29, 0x338529c725d27c8cull}}},
    {&Call153, 1, {0x20ull, 0x800ull, 0x8000ull, 0x20000ull, 0x80000ull, 0x200000ull, 0x2a881full, 0x2a8820ull, 0x2a8821ull, 0xffffffffffd577dfull}, {{0, 0x376a91353c0242cdull}, {2, 0xd73c13243148e6ecull}, {3, 0xc44830fa3ca71918ull}, {5, 0xae50cc837ef2837eull}, {6, 0x69c1caf411496baaull}, {8, 0x56d9f806321b7ab8ull}, {10, 0xb342e57183c8b792ull}, {13, 0x4371bb0d27946093ull}, {14, 0x935a50b1bc98f973ull}, {15, 0xc19d1c325a76e676ull}, {24, 0xd155d38470010962ull}, {29, 0xaaedcbb444d3c171ull}}},
    {&Call154, 6, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xef1687761e42b39dull}, {4, 0x37a3c5c0bf8514b5ull}, {5, 0xb8d6665436dd6e76ull}, {6, 0xbfce09aa822a0f3eull}, {7, 0x486357f2f0c77e26ull}, {8, 0x13c4ec8ccf91ab98ull}, {9, 0xa16168d83186e0c8ull}, {10, 0x1110d8ad95c8c532ull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d6c2a21697281acull}}},
    {&Call155, 1, {0x20000ull, 0x40000ull, 0x5ffffull, 0x60000ull, 0x60001ull, 0xfffffffffff9ffffull}, {{0, 0x376a90353c024812ull}, {1, 0xb1025047404298f1ull}, {2, 0x045a1f71a955ded8ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xc08c8f36e19ba476ull}, {6, 0x69c1cbf411496279ull}, {7, 0xb578312f22ead7f3ull}, {8, 0xd26a143ef76b499full}, {10, 0x329f8fd8c1b7c3c4ull}, {11, 0x5be5bf84542588d0ull}, {12, 0x344be786e5dd011aull}, {13, 0x3cb58662f8716884ull}}},
    {&Call156, 3, {0x2ull, 0x8ull, 0x100ull, 0x109ull, 0x10aull, 0x10bull, 0xfffffffffffffef5ull}, {{0, 0x376a91353c0242cdull}, {1, 0xce94669219dfce1full}, {2, 0x681b3e512009d3ebull}, {3, 0xc44830fa3ca71918ull}, {5, 0x258647453e117c6full}, {6, 0x69c1cbf411496279ull}, {7, 0x76b47dc7717e1702ull}, {8, 0x83929d3e49beeaa0ull}, {10, 0x0e7899d6c9b98c50ull}, {11, 0xb87be382c7dc8275ull}, {12, 0x344be786e5dd011aull}, {13, 0x53f574d8d118ac3eull}}},
    {&Call157, 6, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3553cd8c1bdb367aull}, {3, 0xf134d976e0633bbdull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd8709f57fa644fa6ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xd6a7431969446702ull}, {10, 0x12569dad8c6f4e1bull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7991216a7dd8f7ull}}},
    {&Call158, 3, {0x1ull, 0x2ull, 0x8ull, 0x100ull, 0x10aull, 0x10bull, 0x10cull, 0xfffffffffffffef4ull}, {{0, 0x376a91353c0242cdull}, {1, 0xd442355d5d0daacdull}, {2, 0x627ceb282a614c1cull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x182b0d58a7713215ull}, {6, 0x69c1cbf411496279ull}, {7, 0x3af99546915b7c50ull}, {8, 0xbff907e357ca4a5dull}, {10, 0xb02c64ea8a236125ull}, {11, 0x9f6491e97328de3aull}, {12, 0x344be786e5dd011aull}, {13, 0x83c75f3bcfb277baull}}},
    {&Call159, 4, {0x8ull, 0x10ull, 0x27ull, 0x28ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0x1000000000000ull, 0x2000000000000ull, 0x4000000000000ull, 0x8000000000000ull, 0x10000000000000ull, 0x20000000000000ull, 0x40000000000000ull, 0x80000000000000ull, 0x100000000000000ull, 0x200000000000000ull, 0x400000000000000ull, 0x800000000000000ull, 0x1000000000000000ull, 0x2000000000000000ull, 0x3ff0000000000028ull, 0x4000000000000000ull, 0x4000000000000028ull, 0x4008000000000028ull, 0x4010000000000028ull, 0x4014000000000028ull, 0x4018000000000028ull, 0x401c000000000028ull, 0x8000000000000000ull, 0xbff0000000000028ull, 0xc000000000000028ull, 0xffffffffffffffd7ull, 0xffffffffffffffd8ull, 0xffffffffffffffd9ull}, {{0, 0xd6e2be9fe29e1f38ull}, {1, 0x2663d69565ab165eull}, {2, 0x24013ddbe5da2178ull}, {3, 0x3bb7e8023ca6cb9cull}, {5, 0xe4c9499b92668668ull}, {6, 0xdb68c0ed3c9dec5full}, {7, 0xbc25c338847914c3ull}, {8, 0x82a39b36ec70e4b3ull}, {10, 0xcf67ee97e49c08b9ull}, {11, 0x961c02efbb323256ull}, {12, 0xca53622068a27542ull}, {13, 0x9e492a4441eba9f4ull}}},
    {&Call160, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44830fa3ca71918ull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1caf411496baaull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075878d7d67788ull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call161, 3, {0x2ull, 0x4ull, 0x5ull, 0x6ull, 0x7ull, 0xfffffffffffffff9ull}, {{0, 0x376a90353c024812ull}, {1, 0x20b6215f274ef211ull}, {2, 0xf85492383e8cc53eull}, {3, 0x65df2dfae5834111ull}, {5, 0xa033e313fee775cfull}, {6, 0xa49e60fc52bf0939ull}, {7, 0x5819d39bdedceae6ull}, {8, 0x1bdfbe4abbe12a87ull}, {10, 0xec72171e2766898aull}, {11, 0x25444ac96a09e416ull}, {12, 0x3445f986e557e58cull}, {13, 0x4736eec6311692a0ull}}},
    {&Call162, 3, {0x2ull, 0x4ull, 0x5ull, 0x6ull, 0x7ull, 0xfffffffffffffff9ull}, {{0, 0x376a90353c024812ull}, {1, 0x20b8b95f26c1abd9ull}, {2, 0xf84ef2383e69541eull}, {3, 0x2e6470712c171d87ull}, {5, 0xa03aee13fe53159eull}, {6, 0x7c0838f49e152f41ull}, {7, 0x5816b69bdeb89f2full}, {8, 0x1be23c4aba30f6bdull}, {10, 0xec72171e2766898aull}, {11, 0x25444ac96a09e416ull}, {12, 0x344c8f86e5e726a2ull}, {13, 0x473ab0c6317dc106ull}}},
    {&Call163, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829bb324bc272ddull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44830fa3ca71918ull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45a73b1df9f86ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd42ead663b43ull}}},
    {&Call164, 1, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x5ull, 0xfffffffffffffffbull, 0xfffffffffffffffdull}, {{0, 0x376a91353c0242cdull}, {1, 0x44f1dee95f7566caull}, {2, 0xcc961ad4800b1a50ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x9092dac1f8a65f7cull}, {6, 0x69c1cbf411496279ull}, {7, 0x077d4a1ed6d559bbull}, {8, 0x06732b36128fbcd6ull}, {10, 0xf25b2770902ad5f9ull}, {11, 0x767e8e7b4589b975ull}, {12, 0x344be786e5dd011aull}, {13, 0x1f29116ef56d0d8cull}}},
    {&Call165, 1, {0x1ull, 0x2ull, 0x3ull, 0x4ull, 0x7ull, 0x8ull, 0x9ull, 0xfffffffffffffff7ull, 0xfffffffffffffffcull}, {{0, 0x376a91353c0242cdull}, {1, 0x22fd69b84b035220ull}, {2, 0xe2d4b65474243eb6ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x0da94713989109e1ull}, {6, 0x69c1cbf411496279ull}, {7, 0x7319de2af652b899ull}, {8, 0xc10fbdf3e885c03cull}, {10, 0x11a04bf36fe93de1ull}, {11, 0xb9fc7e80a54dd8feull}, {12, 0x344be786e5dd011aull}, {13, 0x62c7ee01ba816547ull}}},
    {&Call166, 5, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3479278c13b07522ull}, {3, 0xf03ad976e9cbe9bdull}, {4, 0x370ad7c0bacd84b5ull}, {5, 0xb916f85438d99e76ull}, {6, 0xeaabf3ee49841719ull}, {7, 0x46b96ff284c173eeull}, {8, 0x13b24e8cccbb0108ull}, {9, 0x0435d01066d2125bull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3e40e82171034bccull}}},
    {&Call167, 4, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{1, 0x44381cc5a85a5069ull}, {2, 0x0844234935e614c9ull}, {3, 0x6016771a8bb43fdfull}, {5, 0xa99fe027c9daa6f8ull}, {6, 0x2cfcd32f60cae121ull}, {7, 0x663c0e8e2ad12ebaull}, {8, 0x870f31d2ef9f20b0ull}, {10, 0xc85e5e866a3e465dull}, {11, 0xa2e7b5041c1bd3daull}, {12, 0xe4ba7b7557cd2629ull}, {13, 0x28d1217e65be51aeull}, {14, 0x4e554db98a226633ull}}},
    {&Call168, 4, {}, {{1, 0x01be1ce838cf4e02ull}, {2, 0xafeee9a451159be6ull}, {3, 0xde54a051b4f660d5ull}, {4, 0x6884cbb1222d989dull}, {5, 0x9a2127b0a1f0a626ull}, {6, 0xde6739033f8d9b19ull}, {7, 0x7f2a422cfdd63b96ull}, {8, 0xb2f621d1c1626be2ull}, {9, 0xabc9adcb46f93581ull}, {10, 0x200e4242c5d8f5bfull}, {13, 0x8b11e518d86e8591ull}, {15, 0xcb2bebb17f19bac7ull}}},
    {&Call169, 4, {}, {{1, 0xeeed1a2d02727f0aull}, {3, 0xe1dd647b558fb699ull}, {5, 0xfcbc485615d75c66ull}, {6, 0xd870a957fa658ca4ull}, {8, 0x14d1b68cf59c6740ull}, {11, 0xf04d79d063bb7e4aull}, {15, 0x8a0bd8defda858daull}, {17, 0x0554ea6fd18065dfull}, {23, 0xcf0618f12bed65acull}, {24, 0x40dde7b4aaf3a2e3ull}, {29, 0x59fd4ceb4e9f9533ull}, {31, 0x72a17a8669470d6eull}}},
    {&Call170, 1, {0x3ull, 0x4ull, 0x5ull, 0xfffffffffffffffbull}, {{0, 0x376a90353c024812ull}, {1, 0x740e987ffe8f74d3ull}, {2, 0x2881900e925f3d13ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xba20142ec8cea18dull}, {6, 0x69c1cbf411496279ull}, {7, 0x25e686e79f2539baull}, {8, 0x758b366026411facull}, {10, 0x560b4f6e33abe4ffull}, {11, 0xafebfb66df0a266dull}, {12, 0x344be786e5dd011aull}, {13, 0xa6d1599b258c7f70ull}}},
    {&Call171, 1, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull}, {{0, 0x376a90353c024812ull}, {1, 0x97a9ba65bddbfb2bull}, {2, 0x295200a01f888a8cull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xc10d9627fc82d638ull}, {6, 0x69c1cbf411496279ull}, {7, 0x01636c3e9c4156c1ull}, {8, 0xfc6490837816fb2eull}, {10, 0x34e590dbae783711ull}, {11, 0xaae4d14b5b7252e4ull}, {12, 0x344be786e5dd011aull}, {13, 0x43fcb061ae59801aull}}},
    {&Call172, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call173, 1, {0x1ull, 0x2ull, 0x3ull, 0xfffffffffffffffdull}, {{0, 0x376a90353c024812ull}, {1, 0xa06e9f47d0e3dbf3ull}, {2, 0x51817e879b5b9793ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x658363175f1ee8dbull}, {6, 0x69c1cbf411496279ull}, {7, 0x0b5c11e3462f22a9ull}, {8, 0x5a168d0e5a64eaf9ull}, {10, 0x118a91b648671050ull}, {11, 0xb6aaff375d955e97ull}, {12, 0x344be786e5dd011aull}, {13, 0xa54e722163fac2c2ull}}},
    {&Call174, 4, {}, {{1, 0x51da3a28fd46a9eaull}, {2, 0xe718278a88b87522ull}, {3, 0x494d9c18638c0292ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0x4baa50c8b4221ba4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0x56a515df88e97cf1ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7990216a7dd144ull}}},
    {&Call175, 4, {0x0ull, 0x1ull, 0x2ull, 0xfffffffffffffffeull}, {{0, 0x376a90353c024812ull}, {1, 0x97a9ba65bddbfb2bull}, {2, 0x295201a01f889c33ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x3ef268dffc82d1e6ull}, {6, 0x69c1cbf411496279ull}, {7, 0xfe9c92c69c415113ull}, {8, 0xfc6490837816fb2eull}, {10, 0x34e590dbae783711ull}, {11, 0x551b2db35b724836ull}, {12, 0x344be786e5dd011aull}, {13, 0xbc034e99ae598fa4ull}}},
    {&Call176, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call177, 1, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf134da76e0632262ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac27545364e159ull}, {6, 0xd870a857fa6586f3ull}, {7, 0x486356f2f0c70875ull}, {8, 0x14d1b78cf59c68b7ull}, {9, 0xa1615ed8318763caull}, {10, 0x1256a3ad8c6eb0cdull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7991216a7dd8f7ull}}},
    {&Call178, 1, {0x0ull, 0x1ull, 0x2ull, 0x4ull, 0x8ull, 0x10ull, 0x20ull, 0x40ull, 0x80ull, 0x100ull, 0x200ull, 0x400ull, 0x800ull, 0x1000ull, 0x2000ull, 0x4000ull, 0x8000ull, 0x10000ull, 0x20000ull, 0x40000ull, 0x80000ull, 0x100000ull, 0x200000ull, 0x400000ull, 0x800000ull, 0x1000000ull, 0x2000000ull, 0x4000000ull, 0x8000000ull, 0x10000000ull, 0x20000000ull, 0x40000000ull, 0x80000000ull, 0x100000000ull, 0x200000000ull, 0x400000000ull, 0x800000000ull, 0x1000000000ull, 0x2000000000ull, 0x4000000000ull, 0x8000000000ull, 0x10000000000ull, 0x20000000000ull, 0x40000000000ull, 0x80000000000ull, 0x100000000000ull, 0x200000000000ull, 0x400000000000ull, 0x800000000000ull, 0x1000000000000ull, 0x2000000000000ull, 0x4000000000000ull, 0x8000000000000ull, 0x10000000000000ull, 0x20000000000000ull, 0x40000000000000ull, 0x80000000000000ull, 0x100000000000000ull, 0x200000000000000ull, 0x400000000000000ull, 0x800000000000000ull, 0x1000000000000000ull, 0x2000000000000000ull, 0x3ff0000000000001ull, 0x4000000000000000ull, 0x4000000000000001ull, 0x4008000000000001ull, 0x4010000000000001ull, 0x4014000000000001ull, 0x4018000000000001ull, 0x401c000000000001ull, 0x8000000000000000ull, 0xbff0000000000001ull, 0xc000000000000001ull, 0xfffffffffffffffeull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {1, 0xb386c7a6de6ef5ffull}, {2, 0xa6abf9d98044ec2dull}, {3, 0xc44830fa3ca71918ull}, {5, 0x8be41b91bc8a4954ull}, {6, 0x69c1caf411496baaull}, {7, 0xfb90ffba5a70dd18ull}, {8, 0x4687c162227fd60full}, {10, 0xffe6c292f3d16902ull}, {11, 0x0810f456e3d4f114ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0xdd715b00cbfcc846ull}}},
    {&Call179, 1, {0x1ull, 0x2ull, 0x10ull, 0x12ull, 0x13ull, 0x14ull, 0xffffffffffffffecull}, {{0, 0x376a91353c0242cdull}, {1, 0xdfaefd4a45c44b57ull}, {2, 0x339864ade9661512ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x521294a9c08e0b9bull}, {6, 0x69c1caf411496baaull}, {7, 0x6d0d1d753007b05aull}, {8, 0x4eb7603ad92fa549ull}, {10, 0x340c985c4ba0da14ull}, {11, 0xa68e898fe7a82d14ull}, {12, 0x344be786e5dd011aull}, {13, 0x63b2ced6a9fbee21ull}}},
    {&Call180, 1, {}, {{2, 0x3347278c34486f22ull}, {4, 0x3626dec0b0212caaull}, {5, 0xb7ac27545364e159ull}, {8, 0x14d1b78cf59c68b7ull}, {12, 0x3d7962216a7a1312ull}, {17, 0xa094136af94ac734ull}, {21, 0x4e92460c4c608b4full}, {26, 0xe3cde025456cbf8cull}, {27, 0xd3678d41069ed95bull}, {32, 0x4a3fd7801cc0847cull}, {38, 0x37089c8ea9ddc907ull}, {40, 0x05ab54c4c3bff247ull}}},
    {&Call181, 4, {}, {{2, 0x3347278c34486f22ull}, {3, 0xf134e376e0611abfull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xfd5f373ef2fe6bb4ull}, {12, 0x3d79a0216a7c6a74ull}, {16, 0x45d8cf365b44405dull}, {18, 0x3a867c5a326dfd39ull}, {23, 0xcf05f8f12bf2ac0cull}, {24, 0xeaa3a8466510b452ull}, {27, 0x7c7dce447f982ce6ull}, {30, 0x2ad5a4ce68c68061ull}, {32, 0x4a3fd7801cc0847cull}}},
    {&Call182, 3, {0x2ull, 0x100ull, 0x101ull, 0x102ull, 0x103ull, 0xfffffffffffffefdull}, {{0, 0x376a91353c0242cdull}, {1, 0xf908139853713e5aull}, {2, 0x76cae2a89374d019ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0x48ecd347877c62feull}, {6, 0x69c1cbf411496279ull}, {7, 0xdfddb1bc0493f19aull}, {8, 0xb389f17f39c50b88ull}, {10, 0x0e0f57b0db623266ull}, {11, 0xfbcf65471e144787ull}, {12, 0x344be786e5dd011aull}, {13, 0xc5f67a6c7b8b5244ull}}},
    {&Call183, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {1, 0x8829bb324bc272ddull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44830fa3ca71918ull}, {5, 0xb5e7ec52bbd57b95ull}, {6, 0x69c1caf411496baaull}, {7, 0x81809cc55050683bull}, {8, 0x4dde0fa6a836811aull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45a73b1df9f86ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call184, 1, {}, {{2, 0x3347278c34486f22ull}, {6, 0xd8708657fa64b83dull}, {9, 0xa1615dd831877d99ull}, {18, 0x3a867c5a326dfd39ull}, {26, 0xe3cde125456cc943ull}, {29, 0x59fd4ceb4e9f9533ull}, {38, 0x37089c8ea9ddc907ull}, {44, 0x68c5891a8d7a0b91ull}, {47, 0x6aa7c509dadceca0ull}, {50, 0x6ad3da525d89931bull}, {51, 0x7a1a7eea5ab2cc13ull}, {54, 0xb86e57412e670848ull}}},
    {&Call185, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829ba324bc2780aull}, {2, 0x82f8975d24748fa3ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call186, 4, {0x1ull, 0x2ull, 0x4ull, 0x8ull, 0xeull, 0xfull, 0x10ull, 0x1ffffffffffffull, 0x2000000000000ull, 0x4000000000000ull, 0x8000000000000ull, 0x10000000000000ull, 0x20000000000000ull, 0x40000000000000ull, 0x80000000000000ull, 0x100000000000000ull, 0x200000000000000ull, 0x400000000000000ull, 0x800000000000000ull, 0x1000000000000000ull, 0x2000000000000000ull, 0x3ff2000000000000ull, 0x4000000000000000ull, 0x4002000000000000ull, 0x400a000000000000ull, 0x4012000000000000ull, 0x4016000000000000ull, 0x401a000000000000ull, 0x401e000000000000ull, 0x8000000000000000ull, 0xbff2000000000000ull, 0xc002000000000000ull, 0xfffdffffffffffffull, 0xfffe000000000000ull, 0xfffe000000000001ull, 0xfffffffffffffff0ull}, {{0, 0xc6889632b312624cull}, {1, 0x844598f5465cbed7ull}, {2, 0x5c42e560119d937eull}, {3, 0x85aa2af833b7c326ull}, {5, 0x99c6da85c0df90deull}, {6, 0xb1bbcbf2d2996279ull}, {7, 0x05c6bddaf32a0eb1ull}, {8, 0x619247c850391008ull}, {10, 0xd0a14a4891a52adbull}, {11, 0xed352bc5663e039bull}, {12, 0x9b61ee839c8d3d3full}, {13, 0xaf4e82be19855943ull}}},
    {&Call187, 4, {}, {{1, 0xa2e70a2e0351d771ull}, {2, 0x3347278c34486f22ull}, {3, 0x0ecbbc8ee066ead8ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870a957fa658ca4ull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xdb1d82e3b9fae239ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0x4395a9d5fc9a313aull}, {12, 0x945139f839884f2cull}}},
    {&Call188, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a91353c0242cdull}, {1, 0x8829bb324bc272ddull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809cc55050683bull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075878d7d67788ull}, {11, 0x8ef45a73b1df9f86ull}, {12, 0x344be886e5dd7ae9ull}, {13, 0x688cd52ead6632b0ull}}},
    {&Call189, 1, {0x0ull, 0x1ull, 0xffffffffffffffffull}, {{0, 0x376a90353c024812ull}, {1, 0x8829bb324bc272ddull}, {2, 0x82f8985d2474f5d4ull}, {3, 0xc44831fa3ca72f4bull}, {5, 0xb5e7eb52bbd54dc2ull}, {6, 0x69c1cbf411496279ull}, {7, 0x81809bc550505274ull}, {8, 0x4dde10a6a83678c9ull}, {10, 0xb0075978d7d67dffull}, {11, 0x8ef45b73b1df9655ull}, {12, 0x344be786e5dd011aull}, {13, 0x688cd42ead663b43ull}}},
    {&Call190, 1, {}, {{1, 0x44423a285186a9eaull}, {2, 0x3347278c34486f22ull}, {3, 0xf1341876e0646bc8ull}, {4, 0x3626dfc0b021225dull}, {5, 0xb7ac285453649f06ull}, {6, 0xd870cb57fa66a57aull}, {7, 0x486357f2f0c77e26ull}, {8, 0x14d1b68cf59c6740ull}, {9, 0xa16115d83185b7f1ull}, {10, 0x1256a2ad8c6ebe9aull}, {11, 0xf04d79d063bb7e4aull}, {12, 0x3d7990216a7dd144ull}}},
    {&Call191, 1, {0x1ull, 0x4ull, 0x5ull, 0x6ull, 0x1full, 0x20ull, 0x21ull, 0xffffffffffffffdfull, 0xfffffffffffffffaull}, {{1, 0xef82f325481786deull}, {2, 0x95e6f87f1cfdcf3full}, {3, 0xc44831fa3ca72f4bull}, {8, 0x03517d23140d2c1full}, {10, 0x907f312bf505e649ull}, {14, 0xf524a0d697ded383ull}, {15, 0xc19d1c325a76e676ull}, {19, 0x3ebbb6c9031e6084ull}, {20, 0xc47aa4dfbbd44580ull}, {21, 0x4e91ef0c4c7d5f18ull}, {29, 0x1d7641cc23daacbaull}, {32, 0x42d3116d0b5b64aeull}}},
    {&Call192, 1, {0x3ull, 0x4ull, 0x5ull, 0xfull, 0x10ull, 0x11ull, 0xffffffffffffffefull, 0xfffffffffffffffbull}, {{1, 0x5799f61703ba8659ull}, {2, 0x0bb000adc0622e49ull}, {3, 0xc44830fa3ca71918ull}, {8, 0x995a694dc4bddfe3ull}, {10, 0x1ea710174fc9ddc8ull}, {14, 0xfcfa216d2b1a9096ull}, {15, 0xc19d1d325a76e025ull}, {19, 0x3d108da61f296df0ull}, {20, 0x12ac3dd9688eac73ull}, {21, 0x4e91ef0c4c7d5f18ull}, {29, 0x523c538da634d26full}, {32, 0x1d9499905c4eb250ull}}},
};

static const int NullFirst[] = {1, 2, 4, 5, 8, 10, 14, 15, 16, 31, 32, 34, 36, 39, 40, 41, 42, 43, 45, 47, 50, 51, 52, 57, 61, 64, 65, 66, 67, 68, 72, 79, 80, 83, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 112, 116, 123, 124, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191};

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
    Require(checked == 2304);
    return 0;
}
