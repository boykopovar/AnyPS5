#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore geometry guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

template <typename T>
static T Garbage() {
    T value;
    std::memset(&value, 0xab, sizeof(value));
    return value;
}

template <typename T, typename Construct>
static bool Constructs(Construct construct, const T& expected) {
    T out = Garbage<T>();
    construct(&out);
    return std::memcmp(&out, &expected, sizeof(out)) == 0;
}

template <typename T>
static T& Good() {
    static T value{};
    return value;
}

static unsigned char* MapPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

int main() {
    WebCore::FloatRect rect{{1.0f, 2.0f}, {3.0f, 4.0f}};
    _ZN7WebCore9FloatRect5uniteERKS0_(&rect, WebCore::FloatRect{{0.0f, 0.0f}, {2.0f, 2.0f}});
    Require(rect.m_location.m_x == 0.0f && rect.m_location.m_y == 0.0f && rect.m_size.m_width == 4.0f && rect.m_size.m_height == 6.0f);
    _ZN7WebCore9FloatRect5uniteERKS0_(&rect, WebCore::FloatRect{{9.0f, 9.0f}, {0.0f, 1.0f}});
    Require(rect.m_size.m_width == 4.0f && rect.m_size.m_height == 6.0f);
    WebCore::FloatRect empty{};
    _ZN7WebCore9FloatRect5uniteERKS0_(&empty, rect);
    Require(empty.m_size.m_width == 4.0f && empty.m_size.m_height == 6.0f);
    _ZN7WebCore9FloatRect5scaleEff(&rect, 2.0f, 3.0f);
    Require(rect.m_location.m_x == 0.0f && rect.m_location.m_y == 0.0f && rect.m_size.m_width == 8.0f && rect.m_size.m_height == 18.0f);
    Require(_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(&rect, WebCore::FloatPoint{0.0f, 0.0f}, 0));
    Require(!_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(&rect, WebCore::FloatPoint{0.0f, 0.0f}, 1));
    WebCore::IntRect intRect{{0, 0}, {4, 4}};
    _ZN7WebCore7IntRect5uniteERKS0_(&intRect, WebCore::IntRect{{2, 2}, {4, 4}});
    Require(intRect.m_size.m_width == 6 && intRect.m_size.m_height == 6);
    Require(_ZNK7WebCore7IntRect8containsERKS0_(&intRect, WebCore::IntRect{{1, 1}, {2, 2}}));
    WebCore::FloatPoint point{3.0f, 4.0f};
    _ZN7WebCore10FloatPoint9normalizeEv(&point);
    Require(point.m_x > 0.599f && point.m_x < 0.601f && point.m_y > 0.799f && point.m_y < 0.801f);
    WebCore::IntPoint intPoint{};
    _ZN7WebCore8IntPointC1ERKNS_10FloatPointE(&intPoint, WebCore::FloatPoint{2.5f, -3.5f});
    Require(intPoint.m_x == 2 && intPoint.m_y == -3);
    WebCore::FloatQuad quad{{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
    const auto box = _ZNK7WebCore9FloatQuad11boundingBoxEv(&quad);
    Require(box.m_size.m_width == 2.0f && box.m_size.m_height == 2.0f && _ZNK7WebCore9FloatQuad13isRectilinearEv(&quad));
    Require(_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(&quad, WebCore::FloatPoint{1.0f, 1.0f}));
    WebCore::LayoutRect layout{};
    _ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(&layout, WebCore::FloatRect{{1.0f, 2.0f}, {3.0f, 4.0f}});
    Require(layout.m_location.m_x.m_value == 64 && layout.m_size.m_height.m_value == 256);
    const WebCore::FloatRect floatRect{{1.0f, 2.0f}, {3.0f, 4.0f}};
    const WebCore::FloatRoundedRect::Radii floatRadii{{1.0f, 1.0f}, {2.0f, 2.0f}, {3.0f, 3.0f}, {4.0f, 4.0f}};
    const WebCore::LayoutRect layoutRect{{{64}, {128}}, {{192}, {256}}};
    const WebCore::LayoutSize topLeft{{64}, {64}}, topRight{{128}, {128}}, bottomLeft{{192}, {192}}, bottomRight{{256}, {256}};
    const WebCore::RoundedRect roundedRect{layoutRect, {topLeft, topRight, bottomLeft, bottomRight}};
    Require(Constructs([](WebCore::FloatPoint* out) { _ZN7WebCore10FloatPointC1ERKNS_8IntPointE(out, WebCore::IntPoint{3, -4}); }, WebCore::FloatPoint{3.0f, -4.0f}));
    Require(Constructs([](WebCore::FloatPoint* out) { _ZN7WebCore10FloatPointC2ERKNS_8IntPointE(out, WebCore::IntPoint{3, -4}); }, WebCore::FloatPoint{3.0f, -4.0f}));
    Require(Constructs([](WebCore::FloatRect* out) { _ZN7WebCore9FloatRectC1ERKNS_7IntRectE(out, WebCore::IntRect{{1, 2}, {3, 4}}); }, floatRect));
    Require(Constructs([](WebCore::FloatRect* out) { _ZN7WebCore9FloatRectC2ERKNS_7IntRectE(out, WebCore::IntRect{{1, 2}, {3, 4}}); }, floatRect));
    Require(Constructs([&](WebCore::FloatRoundedRect* out) { _ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(out, floatRect, floatRadii); }, WebCore::FloatRoundedRect{floatRect, floatRadii}));
    Require(Constructs([&](WebCore::FloatRoundedRect* out) { _ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(out, floatRect, floatRadii); }, WebCore::FloatRoundedRect{floatRect, floatRadii}));
    Require(Constructs([](WebCore::IntPoint* out) { _ZN7WebCore8IntPointC2ERKNS_10FloatPointE(out, WebCore::FloatPoint{2.5f, -3.5f}); }, WebCore::IntPoint{2, -3}));
    Require(Constructs([](WebCore::IntSize* out) { _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(out, WebCore::FloatSize{5.75f, -1.25f}); }, WebCore::IntSize{5, -1}));
    Require(Constructs([](WebCore::IntSize* out) { _ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(out, WebCore::FloatSize{5.75f, -1.25f}); }, WebCore::IntSize{5, -1}));
    Require(Constructs([&](WebCore::IntRect* out) { _ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(out, layoutRect); }, WebCore::IntRect{{1, 2}, {3, 4}}));
    Require(Constructs([&](WebCore::IntRect* out) { _ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(out, layoutRect); }, WebCore::IntRect{{1, 2}, {3, 4}}));
    Require(Constructs([](WebCore::IntRect* out) { _ZN7WebCore7IntRectC1ERKNS_9FloatRectE(out, WebCore::FloatRect{{1.5f, 2.5f}, {3.5f, 4.5f}}); }, WebCore::IntRect{{1, 2}, {3, 4}}));
    Require(Constructs([](WebCore::IntRect* out) { _ZN7WebCore7IntRectC2ERKNS_9FloatRectE(out, WebCore::FloatRect{{1.5f, 2.5f}, {3.5f, 4.5f}}); }, WebCore::IntRect{{1, 2}, {3, 4}}));
    Require(Constructs([&](WebCore::LayoutRect* out) { _ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(out, floatRect); }, layoutRect));
    Require(Constructs([&](WebCore::LayoutRect* out) { _ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(out, floatRect); }, layoutRect));
    Require(Constructs([&](WebCore::RoundedRect* out) { _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(out, layoutRect, topLeft, topRight, bottomLeft, bottomRight); }, roundedRect));
    Require(Constructs([&](WebCore::RoundedRect* out) { _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(out, layoutRect, topLeft, topRight, bottomLeft, bottomRight); }, roundedRect));
    Require(_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_(WebCore::FloatRect{{0.0f, 0.0f}, {4.0f, 4.0f}}, WebCore::FloatRect{{0.0f, 0.0f}, {4.0f, 4.0f}}) != 0);

    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10FloatPoint9normalizeEv(reinterpret_cast<WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10FloatPoint9normalizeEv"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10FloatPointC1ERKNS_8IntPointE(reinterpret_cast<WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::IntPoint>())); return 0; }, "_ZN7WebCore10FloatPointC1ERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10FloatPointC1ERKNS_8IntPointE(&Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10FloatPointC1ERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10FloatPointC2ERKNS_8IntPointE(reinterpret_cast<WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::IntPoint>())); return 0; }, "_ZN7WebCore10FloatPointC2ERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10FloatPointC2ERKNS_8IntPointE(&Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10FloatPointC2ERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE(reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::AffineTransform>())); return 0; }, "_ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE(&Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::AffineTransform*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE(reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::TransformationMatrix>())); return 0; }, "_ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE(&Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::TransformationMatrix*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>(), Good<WebCore::FloatPoint>())); return 0; }, "_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(&Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>())); return 0; }, "_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(&Good<WebCore::FloatPoint>(), Good<WebCore::FloatPoint>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}), Good<WebCore::FloatSize>(), Good<WebCore::FloatSize>())); return 0; }, "_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(&Good<WebCore::FloatSize>(), *reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}), Good<WebCore::FloatSize>())); return 0; }, "_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(&Good<WebCore::FloatSize>(), Good<WebCore::FloatSize>(), *reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatSize6isZeroEv(reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatSize6isZeroEv"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect5scaleEff(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), 1, 1)); return 0; }, "_ZN7WebCore9FloatRect5scaleEff"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect5uniteERKS0_(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore9FloatRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect5uniteERKS0_(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore9FloatRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect9intersectERKS0_(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore9FloatRect9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect9intersectERKS0_(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore9FloatRect9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect6extendERKNS_10FloatPointE(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>())); return 0; }, "_ZN7WebCore9FloatRect6extendERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRect6extendERKNS_10FloatPointE(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore9FloatRect6extendERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRectC1ERKNS_7IntRectE(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZN7WebCore9FloatRectC1ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRectC1ERKNS_7IntRectE(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore9FloatRectC1ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRectC2ERKNS_7IntRectE(reinterpret_cast<WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZN7WebCore9FloatRectC2ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore9FloatRectC2ERKNS_7IntRectE(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore9FloatRectC2ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect10intersectsERKS0_(reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZNK7WebCore9FloatRect10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect10intersectsERKS0_(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatRect10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>(), 0)); return 0; }, "_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}), 0)); return 0; }, "_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect8containsERKS0_(reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZNK7WebCore9FloatRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatRect8containsERKS0_(&Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatQuad12containsQuadERKS0_(reinterpret_cast<const WebCore::FloatQuad*>(std::uintptr_t{0x10}), Good<WebCore::FloatQuad>())); return 0; }, "_ZNK7WebCore9FloatQuad12containsQuadERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatQuad12containsQuadERKS0_(&Good<WebCore::FloatQuad>(), *reinterpret_cast<const WebCore::FloatQuad*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatQuad12containsQuadERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(reinterpret_cast<const WebCore::FloatQuad*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>())); return 0; }, "_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(&Good<WebCore::FloatQuad>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore9FloatQuad13isRectilinearEv(reinterpret_cast<const WebCore::FloatQuad*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore9FloatQuad13isRectilinearEv"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(reinterpret_cast<WebCore::FloatRoundedRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>(), Good<WebCore::FloatRoundedRect::Radii>())); return 0; }, "_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(&Good<WebCore::FloatRoundedRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRoundedRect::Radii>())); return 0; }, "_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(&Good<WebCore::FloatRoundedRect>(), Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRoundedRect::Radii*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(reinterpret_cast<WebCore::FloatRoundedRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>(), Good<WebCore::FloatRoundedRect::Radii>())); return 0; }, "_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(&Good<WebCore::FloatRoundedRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRoundedRect::Radii>())); return 0; }, "_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(&Good<WebCore::FloatRoundedRect>(), Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRoundedRect::Radii*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore8IntPointC1ERKNS_10FloatPointE(reinterpret_cast<WebCore::IntPoint*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>())); return 0; }, "_ZN7WebCore8IntPointC1ERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore8IntPointC1ERKNS_10FloatPointE(&Good<WebCore::IntPoint>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore8IntPointC1ERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore8IntPointC2ERKNS_10FloatPointE(reinterpret_cast<WebCore::IntPoint*>(std::uintptr_t{0x10}), Good<WebCore::FloatPoint>())); return 0; }, "_ZN7WebCore8IntPointC2ERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore8IntPointC2ERKNS_10FloatPointE(&Good<WebCore::IntPoint>(), *reinterpret_cast<const WebCore::FloatPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore8IntPointC2ERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10}), Good<WebCore::IntPoint>(), Good<WebCore::IntPoint>())); return 0; }, "_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(&Good<WebCore::IntPoint>(), *reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10}), Good<WebCore::IntPoint>())); return 0; }, "_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(&Good<WebCore::IntPoint>(), Good<WebCore::IntPoint>(), *reinterpret_cast<const WebCore::IntPoint*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(reinterpret_cast<WebCore::IntSize*>(std::uintptr_t{0x10}), Good<WebCore::FloatSize>())); return 0; }, "_ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(&Good<WebCore::IntSize>(), *reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(reinterpret_cast<WebCore::IntSize*>(std::uintptr_t{0x10}), Good<WebCore::FloatSize>())); return 0; }, "_ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(&Good<WebCore::IntSize>(), *reinterpret_cast<const WebCore::FloatSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(reinterpret_cast<const WebCore::IntSize*>(std::uintptr_t{0x10}), Good<WebCore::IntSize>(), Good<WebCore::IntSize>())); return 0; }, "_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(&Good<WebCore::IntSize>(), *reinterpret_cast<const WebCore::IntSize*>(std::uintptr_t{0x10}), Good<WebCore::IntSize>())); return 0; }, "_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(&Good<WebCore::IntSize>(), Good<WebCore::IntSize>(), *reinterpret_cast<const WebCore::IntSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRect5scaleEf(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), 1)); return 0; }, "_ZN7WebCore7IntRect5scaleEf"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRect5uniteERKS0_(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZN7WebCore7IntRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRect5uniteERKS0_(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRect9intersectERKS0_(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZN7WebCore7IntRect9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRect9intersectERKS0_(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRect9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>())); return 0; }, "_ZN7WebCore7IntRectC1ERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRectC1ERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC1ERKNS_9FloatRectE(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore7IntRectC1ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC1ERKNS_9FloatRectE(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRectC1ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>())); return 0; }, "_ZN7WebCore7IntRectC2ERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRectC2ERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC2ERKNS_9FloatRectE(reinterpret_cast<WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore7IntRectC2ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore7IntRectC2ERKNS_9FloatRectE(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore7IntRectC2ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntRect10intersectsERKS0_(reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZNK7WebCore7IntRect10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntRect10intersectsERKS0_(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore7IntRect10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntRect8containsERKS0_(reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}), Good<WebCore::IntRect>())); return 0; }, "_ZNK7WebCore7IntRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore7IntRect8containsERKS0_(&Good<WebCore::IntRect>(), *reinterpret_cast<const WebCore::IntRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore7IntRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRect5scaleEf(reinterpret_cast<WebCore::LayoutRect*>(std::uintptr_t{0x10}), 1)); return 0; }, "_ZN7WebCore10LayoutRect5scaleEf"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRect5uniteERKS0_(reinterpret_cast<WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>())); return 0; }, "_ZN7WebCore10LayoutRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRect5uniteERKS0_(&Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10LayoutRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(reinterpret_cast<WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(&Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(reinterpret_cast<WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(&Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10LayoutRect8containsERKS0_(reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>())); return 0; }, "_ZNK7WebCore10LayoutRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore10LayoutRect8containsERKS0_(&Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore10LayoutRect8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(reinterpret_cast<WebCore::RoundedRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(reinterpret_cast<WebCore::RoundedRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}), Good<WebCore::LayoutSize>())); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&Good<WebCore::RoundedRect>(), Good<WebCore::LayoutRect>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), Good<WebCore::LayoutSize>(), *reinterpret_cast<const WebCore::LayoutSize*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(reinterpret_cast<const WebCore::RoundedRect*>(std::uintptr_t{0x10}), Good<WebCore::LayoutRect>())); return 0; }, "_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(&Good<WebCore::RoundedRect>(), *reinterpret_cast<const WebCore::LayoutRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_(*reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}), Good<WebCore::FloatRect>())); return 0; }, "_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_"));
    Require(FaultsAsNotImplemented([&] { static_cast<void>(_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_(Good<WebCore::FloatRect>(), *reinterpret_cast<const WebCore::FloatRect*>(std::uintptr_t{0x10}))); return 0; }, "_ZN7WebCore25TextureMapperBackingStore25calculateExposedTileEdgesERKNS_9FloatRectES3_"));

    unsigned char* page = MapPage();
    auto* readOnlyRect = reinterpret_cast<WebCore::FloatRect*>(page);
    auto* readOnlyIntRect = reinterpret_cast<WebCore::IntRect*>(page + 64);
    *readOnlyRect = WebCore::FloatRect{{1.0f, 1.0f}, {2.0f, 2.0f}};
    *readOnlyIntRect = WebCore::IntRect{{1, 1}, {2, 2}};
    unsigned char before[4096];
    std::memcpy(before, page, sizeof(before));
    ProtectReadOnly(page);
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore9FloatRect5uniteERKS0_(readOnlyRect, WebCore::FloatRect{{0.0f, 0.0f}, {5.0f, 5.0f}}); return 0; }, "_ZN7WebCore9FloatRect5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore9FloatRect9intersectERKS0_(readOnlyRect, WebCore::FloatRect{{0.0f, 0.0f}, {5.0f, 5.0f}}); return 0; }, "_ZN7WebCore9FloatRect9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore9FloatRect5scaleEff(readOnlyRect, 2.0f, 2.0f); return 0; }, "_ZN7WebCore9FloatRect5scaleEff"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore7IntRect9intersectERKS0_(readOnlyIntRect, WebCore::IntRect{{0, 0}, {1, 1}}); return 0; }, "_ZN7WebCore7IntRect9intersectERKS0_"));
    Require(std::memcmp(before, page, sizeof(before)) == 0);
    Require(_ZNK7WebCore9FloatRect8containsERKS0_(readOnlyRect, WebCore::FloatRect{{1.5f, 1.5f}, {1.0f, 1.0f}}));
    UnmapPage(page);
    return 0;
}
