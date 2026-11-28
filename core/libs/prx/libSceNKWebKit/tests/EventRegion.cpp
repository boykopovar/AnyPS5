#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "EventRegion check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct EventRegionHolder {
    alignas(alignof(WebCore::EventRegion)) char storage[sizeof(WebCore::EventRegion)];

    EventRegionHolder() {
        _ZN7WebCore6RegionC1Ev(region());
    }

    EventRegionHolder(std::initializer_list<WebCore::IntRect> rects) {
        _ZN7WebCore6RegionC1Ev(region());
        for (const WebCore::IntRect& rect : rects) {
            WebCore::Region piece;
            _ZN7WebCore6RegionC1ERKNS_7IntRectE(&piece, rect);
            _ZN7WebCore6Region5uniteERKS0_(region(), piece);
            _ZN7WebCore6RegionD1Ev(&piece);
        }
    }

    EventRegionHolder(const EventRegionHolder&) = delete;
    EventRegionHolder& operator=(const EventRegionHolder&) = delete;

    ~EventRegionHolder() {
        _ZN7WebCore6RegionD1Ev(region());
    }

    WebCore::EventRegion* ptr() { return reinterpret_cast<WebCore::EventRegion*>(storage); }
    WebCore::Region* region() { return &ptr()->m_region; }
};

static WebCore::IntRect Rect(int x, int y, int width, int height) {
    return WebCore::IntRect{WebCore::IntPoint{x, y}, WebCore::IntSize{width, height}};
}

static bool Equal(EventRegionHolder& a, EventRegionHolder& b) {
    bool forward = _ZNK7WebCore11EventRegioneqERKS0_(a.ptr(), *b.ptr());
    bool backward = _ZNK7WebCore11EventRegioneqERKS0_(b.ptr(), *a.ptr());
    Require(forward == backward);
    return forward;
}

static void TestEmptyRegions() {
    EventRegionHolder a;
    EventRegionHolder b;
    Require(a.region()->isRect());
    Require(Equal(a, b));
    Require(_ZNK7WebCore11EventRegioneqERKS0_(a.ptr(), *a.ptr()));
}

static void TestRectRegions() {
    EventRegionHolder a{Rect(-10, -20, 30, 40)};
    EventRegionHolder b{Rect(-10, -20, 30, 40)};
    Require(a.region()->isRect());
    Require(Equal(a, b));

    EventRegionHolder movedX{Rect(-9, -20, 30, 40)};
    EventRegionHolder movedY{Rect(-10, -21, 30, 40)};
    EventRegionHolder wider{Rect(-10, -20, 31, 40)};
    EventRegionHolder taller{Rect(-10, -20, 30, 41)};
    Require(!Equal(a, movedX));
    Require(!Equal(a, movedY));
    Require(!Equal(a, wider));
    Require(!Equal(a, taller));
}

static void TestEmptyBoundsAreCompared() {
    EventRegionHolder empty;
    WebCore::Region located;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(&located, Rect(5, 7, 0, 0));
    EventRegionHolder emptyAt;
    _ZN7WebCore6RegionaSERKS0_(emptyAt.region(), located);
    _ZN7WebCore6RegionD1Ev(&located);
    Require(emptyAt.region()->isEmpty());
    Require(!Equal(empty, emptyAt));
}

static void TestShapedRegions() {
    EventRegionHolder a{Rect(0, 0, 10, 10), Rect(20, 0, 10, 10)};
    EventRegionHolder b{Rect(0, 0, 10, 10), Rect(20, 0, 10, 10)};
    Require(!a.region()->isRect());
    Require(!b.region()->isRect());
    Require(a.region()->m_shape.get() != b.region()->m_shape.get());
    Require(Equal(a, b));
    Require(_ZNK7WebCore11EventRegioneqERKS0_(a.ptr(), *a.ptr()));

    EventRegionHolder sameBoundsRect{Rect(0, 0, 30, 10)};
    Require(sameBoundsRect.region()->isRect());
    Require(!Equal(a, sameBoundsRect));

    EventRegionHolder otherGap{Rect(0, 0, 10, 10), Rect(15, 0, 15, 10)};
    Require(!otherGap.region()->isRect());
    Require(otherGap.region()->m_bounds.m_size.m_width == a.region()->m_bounds.m_size.m_width);
    Require(!Equal(a, otherGap));

    EventRegionHolder otherSpans{Rect(0, 0, 10, 10), Rect(20, 5, 10, 5)};
    Require(!otherSpans.region()->isRect());
    Require(!Equal(a, otherSpans));
}

static void TestTranslatedCopies() {
    EventRegionHolder a{Rect(-30, -30, 10, 10), Rect(-10, -25, 5, 40)};
    EventRegionHolder copy;
    _ZN7WebCore6RegionaSERKS0_(copy.region(), *a.region());
    Require(a.region()->m_shape.get() != copy.region()->m_shape.get());
    Require(Equal(a, copy));

    _ZN7WebCore6Region9translateERKNS_7IntSizeE(copy.region(), WebCore::IntSize{3, -4});
    Require(!Equal(a, copy));
    _ZN7WebCore6Region9translateERKNS_7IntSizeE(copy.region(), WebCore::IntSize{-3, 4});
    Require(Equal(a, copy));
}

int main() {
    TestEmptyRegions();
    TestRectRegions();
    TestEmptyBoundsAreCompared();
    TestShapedRegions();
    TestTranslatedCopies();
    return 0;
}
