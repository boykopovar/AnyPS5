#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
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
        std::fprintf(stderr, "AffineTransform guest pointer check failed at line %d\n", line);
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

static WebCore::AffineTransform* MapReadOnlyIdentity() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    const WebCore::AffineTransform identity{};
    std::memcpy(page, &identity, sizeof(identity));
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
    return static_cast<WebCore::AffineTransform*>(page);
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static bool SameBytes(const WebCore::AffineTransform& a, const WebCore::AffineTransform& b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

int main() {
    auto* unreadable = reinterpret_cast<WebCore::AffineTransform*>(0x10);
    WebCore::AffineTransform* readOnly = MapReadOnlyIdentity();

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransformC1Ev(unreadable); return 0; }, "_ZN7WebCore15AffineTransformC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransformC1Ev(readOnly); return 0; }, "_ZN7WebCore15AffineTransformC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransformC2Ev(readOnly); return 0; }, "_ZN7WebCore15AffineTransformC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransformC1Edddddd(readOnly, 2, 0, 0, 2, 1, 1); return 0; },
        "_ZN7WebCore15AffineTransformC1Edddddd"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransformC2Edddddd(unreadable, 2, 0, 0, 2, 1, 1); return 0; },
        "_ZN7WebCore15AffineTransformC1Edddddd"));

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7inverseEv(unreadable); }, "_ZNK7WebCore15AffineTransform7inverseEv"));
    const WebCore::OptionalAffineTransform inverse = _ZNK7WebCore15AffineTransform7inverseEv(readOnly);
    Require(inverse.m_engaged && inverse.m_value.m_transform == WebCore::AffineTransform{}.m_transform);

    WebCore::AffineTransform target;
    _ZN7WebCore15AffineTransformC1Edddddd(&target, 2, 0, 0, 2, 10, 20);
    const WebCore::AffineTransform before = target;
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5blendERKS0_d(&target, *unreadable, 0.5); return 0; },
        "_ZN7WebCore15AffineTransform5blendERKS0_d"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5blendERKS0_d(unreadable, target, 0.5); return 0; },
        "_ZN7WebCore15AffineTransform5blendERKS0_d"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore15AffineTransform5blendERKS0_d(readOnly, target, 0.5); return 0; },
        "_ZN7WebCore15AffineTransform5blendERKS0_d"));
    Require(SameBytes(target, before) && SameBytes(*readOnly, WebCore::AffineTransform{}));
    WebCore::AffineTransform expected = before;
    const WebCore::AffineTransform identity{};
    _ZN7WebCore15AffineTransform5blendERKS0_d(&expected, identity, 0.5);
    _ZN7WebCore15AffineTransform5blendERKS0_d(&target, *readOnly, 0.5);
    Require(SameBytes(target, expected) && !SameBytes(target, before));

    WebCore::Region region;
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform9mapRegionERKNS_6RegionE(unreadable, region); },
        "_ZNK7WebCore15AffineTransform9mapRegionERKNS_6RegionE"));

    UnmapPage(readOnly);
    return 0;
}
