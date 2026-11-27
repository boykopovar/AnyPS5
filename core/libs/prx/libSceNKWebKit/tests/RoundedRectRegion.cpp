#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "RoundedRectRegion check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static WebCore::LayoutUnit Px(int pixels) {
    return WebCore::LayoutUnit{pixels * 64};
}

static WebCore::LayoutRect MakeLayoutRect(int x, int y, int width, int height) {
    return WebCore::LayoutRect{
        WebCore::LayoutPoint{Px(x), Px(y)},
        WebCore::LayoutSize{Px(width), Px(height)}
    };
}

static WebCore::RoundedRect MakeUniformRoundedRect(int x, int y, int width, int height, int radiusRaw) {
    const WebCore::LayoutSize corner{WebCore::LayoutUnit{radiusRaw}, WebCore::LayoutUnit{radiusRaw}};
    return WebCore::RoundedRect{
        MakeLayoutRect(x, y, width, height),
        WebCore::RoundedRect::Radii{corner, corner, corner, corner}
    };
}

static bool SameIntRect(const WebCore::IntRect& a, const WebCore::IntRect& b) {
    return a.m_location.m_x == b.m_location.m_x
        && a.m_location.m_y == b.m_location.m_y
        && a.m_size.m_width == b.m_size.m_width
        && a.m_size.m_height == b.m_size.m_height;
}

static void TestEmptyAndUnrounded() {
    const WebCore::RoundedRect emptyZeroWidth = MakeUniformRoundedRect(10, 20, 0, 40, 10 * 64);
    const WebCore::Region rEmptyW = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(emptyZeroWidth, 20);
    Require(rEmptyW.isEmpty());
    Require(_ZNK7WebCore6Region9totalAreaEv(&rEmptyW) == 0);
    Require(_ZNK7WebCore6Region5rectsEv(&rEmptyW).isEmpty());

    const WebCore::RoundedRect emptyZeroHeight = MakeUniformRoundedRect(10, 20, 40, 0, 10 * 64);
    const WebCore::Region rEmptyH = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(emptyZeroHeight, 0);
    Require(rEmptyH.isEmpty());
    Require(_ZNK7WebCore6Region9totalAreaEv(&rEmptyH) == 0);

    const WebCore::RoundedRect emptyNegative = MakeUniformRoundedRect(10, 20, -5, 40, 10 * 64);
    const WebCore::Region rEmptyNeg = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(emptyNegative, 20);
    Require(rEmptyNeg.isEmpty());

    const WebCore::RoundedRect unrounded = MakeUniformRoundedRect(10, 20, 30, 40, 0);
    const WebCore::Region rPlain = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(unrounded, 20);
    Require(!rPlain.isEmpty());
    Require(rPlain.isRect());
    Require(SameIntRect(rPlain.bounds(), WebCore::IntRect{{10, 20}, {30, 40}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rPlain) == 1200);

    const WebCore::Region rPlainZeroStep = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(unrounded, 0);
    Require(rPlainZeroStep.isRect());
    Require(_ZNK7WebCore6Region9totalAreaEv(&rPlainZeroStep) == 1200);

    const WebCore::RoundedRect fractionalUnrounded{
        WebCore::LayoutRect{
            WebCore::LayoutPoint{WebCore::LayoutUnit{16}, WebCore::LayoutUnit{32}},
            WebCore::LayoutSize{WebCore::LayoutUnit{100 * 64 + 16}, WebCore::LayoutUnit{80 * 64 + 32}}
        },
        WebCore::RoundedRect::Radii{}
    };
    const WebCore::Region rFrac = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(fractionalUnrounded, 20);
    Require(rFrac.isRect());
    Require(SameIntRect(rFrac.bounds(), WebCore::IntRect{{0, 0}, {101, 81}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rFrac) == 8181);
}

static void TestUniformRadiiAndThreshold() {
    const WebCore::RoundedRect uniform10 = MakeUniformRoundedRect(0, 0, 100, 100, 10 * 64);
    const WebCore::Region r10 = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(uniform10, 20);
    Require(!r10.isEmpty());
    Require(!r10.isRect());
    Require(SameIntRect(r10.bounds(), WebCore::IntRect{{0, 0}, {100, 100}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&r10) == 9964);

    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{0, 0}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{2, 2}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{97, 0}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{99, 2}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{0, 97}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{2, 99}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{97, 97}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{99, 99}));

    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{3, 0}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{0, 3}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{3, 3}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{96, 0}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{99, 3}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{3, 99}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{0, 96}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{96, 99}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{99, 96}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&r10, WebCore::IntPoint{50, 50}));

    const WebCore::RoundedRect radius9Half = MakeUniformRoundedRect(0, 0, 100, 100, 9 * 64 + 32);
    const WebCore::Region r9Half = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(radius9Half, 20);
    Require(!r9Half.isRect());
    Require(_ZNK7WebCore6Region9totalAreaEv(&r9Half) == 9964);

    const WebCore::RoundedRect radiusBelowHalf = MakeUniformRoundedRect(0, 0, 100, 100, 9 * 64 + 31);
    const WebCore::Region rBelowHalf = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(radiusBelowHalf, 20);
    Require(rBelowHalf.isRect());
    Require(_ZNK7WebCore6Region9totalAreaEv(&rBelowHalf) == 10000);
}

static void TestMultiStepAsymmetricNegativeAndCap() {
    const WebCore::RoundedRect large = MakeUniformRoundedRect(0, 0, 200, 200, 60 * 64);
    const WebCore::Region rLarge = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(large, 20);
    Require(!rLarge.isRect());
    Require(SameIntRect(rLarge.bounds(), WebCore::IntRect{{0, 0}, {200, 200}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rLarge) == 37904);

    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{0, 0}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{17, 17}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{18, 18}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{4, 37}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{5, 37}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{4, 38}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{182, 182}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{181, 181}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rLarge, WebCore::IntPoint{100, 100}));

    const WebCore::RoundedRect asymmetric{
        MakeLayoutRect(10, 20, 100, 80),
        WebCore::RoundedRect::Radii{
            WebCore::LayoutSize{Px(20), Px(40)},
            WebCore::LayoutSize{Px(0), Px(0)},
            WebCore::LayoutSize{Px(40), Px(0)},
            WebCore::LayoutSize{Px(30), Px(10)}
        }
    };
    const WebCore::Region rAsym = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(asymmetric, 20);
    Require(SameIntRect(rAsym.bounds(), WebCore::IntRect{{10, 20}, {100, 80}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rAsym) == 7901);
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{10, 20}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{15, 31}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{16, 20}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{10, 32}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{109, 20}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{10, 99}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{101, 97}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{109, 99}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{100, 99}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rAsym, WebCore::IntPoint{109, 96}));

    const WebCore::RoundedRect negative = MakeUniformRoundedRect(-50, -40, 100, 80, 20 * 64);
    const WebCore::Region rNeg = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(negative, 20);
    Require(SameIntRect(rNeg.bounds(), WebCore::IntRect{{-50, -40}, {100, 80}}));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rNeg) == 7856);
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{-50, -40}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{-45, -35}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{-44, -40}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{-50, -34}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{44, 34}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{49, 39}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{43, 39}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{49, 33}));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(&rNeg, WebCore::IntPoint{0, 0}));

    const WebCore::Region rStep1 = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(large, 1);
    const WebCore::Region rStep3 = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(large, 3);
    Require(SameIntRect(rStep1.bounds(), rStep3.bounds()));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rStep1) == _ZNK7WebCore6Region9totalAreaEv(&rStep3));
    Require(_ZNK7WebCore6Region8containsERKS0_(&rStep1, rStep3));
    Require(_ZNK7WebCore6Region8containsERKS0_(&rStep3, rStep1));
    Require(_ZNK7WebCore6Region9totalAreaEv(&rStep1) < _ZNK7WebCore6Region9totalAreaEv(&rLarge));
}

static void TestZeroStepAndGuestPointers() {
    const WebCore::RoundedRect rounded = MakeUniformRoundedRect(0, 0, 100, 100, 10 * 64);
    bool threwInvalid = false;
    try {
        static_cast<void>(_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rounded, 0));
    } catch (const std::invalid_argument&) {
        threwInvalid = true;
    }
    Require(threwInvalid);

    const auto* unreadable = reinterpret_cast<const WebCore::RoundedRect*>(0x10);
    errno = 73;
    bool threwUnreadable = false;
    try {
        static_cast<void>(_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(*unreadable, 20));
    } catch (const std::runtime_error& error) {
        threwUnreadable = std::string(error.what()) == "_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj not implemented";
    }
    Require(threwUnreadable && errno == 73);

#ifdef _WIN32
    auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<unsigned char*>(mapped);
    Require(mprotect(pages + 4096, 4096, PROT_NONE) == 0);
#endif

    const auto* straddling = reinterpret_cast<const WebCore::RoundedRect*>(pages + 4096 - 16);
    errno = 79;
    bool threwStraddling = false;
    try {
        static_cast<void>(_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(*straddling, 20));
    } catch (const std::runtime_error& error) {
        threwStraddling = std::string(error.what()) == "_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj not implemented";
    }
    Require(threwStraddling && errno == 79);

    unsigned char* atEnd = pages + 4096 - sizeof(WebCore::RoundedRect);
    std::memcpy(atEnd, &rounded, sizeof(rounded));
#ifdef _WIN32
    Require(VirtualProtect(pages, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(pages, 4096, PROT_READ) == 0);
#endif
    errno = 83;
    const WebCore::Region fromReadOnly = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(
        *reinterpret_cast<const WebCore::RoundedRect*>(atEnd), 20);
    Require(errno == 83);
    Require(_ZNK7WebCore6Region9totalAreaEv(&fromReadOnly) == 9964);

#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

int main() {
    TestEmptyAndUnrounded();
    TestUniformRadiiAndThreshold();
    TestMultiStepAsymmetricNegativeAndCap();
    TestZeroStepAndGuestPointers();
    return 0;
}
