#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "RegionGeometry check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct RegionHolder {
    alignas(alignof(WebCore::Region)) char storage[sizeof(WebCore::Region)];
    WebCore::Region* ptr() { return reinterpret_cast<WebCore::Region*>(storage); }
    const WebCore::Region* ptr() const { return reinterpret_cast<const WebCore::Region*>(storage); }
};

static void TestRegionBasicsAndShape() {
    RegionHolder r1;
    _ZN7WebCore6RegionC1Ev(r1.ptr());
    Require(r1.ptr()->isEmpty());
    Require(_ZNK7WebCore6Region9totalAreaEv(r1.ptr()) == 0);
    WTF::Vector<WebCore::IntRect, 1> emptyRects = _ZNK7WebCore6Region5rectsEv(r1.ptr());
    Require(emptyRects.isEmpty());
    _ZN7WebCore6RegionD1Ev(r1.ptr());

    RegionHolder r2;
    _ZN7WebCore6RegionC2Ev(r2.ptr());
    Require(r2.ptr()->isEmpty());
    _ZN7WebCore6RegionD2Ev(r2.ptr());

    WebCore::IntRect rect{WebCore::IntPoint{10, 20}, WebCore::IntSize{30, 40}};
    RegionHolder r3;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(r3.ptr(), rect);
    Require(!r3.ptr()->isEmpty());
    Require(_ZNK7WebCore6Region9totalAreaEv(r3.ptr()) == 1200);

    RegionHolder r4;
    _ZN7WebCore6RegionC2ERKNS_7IntRectE(r4.ptr(), rect);
    Require(_ZNK7WebCore6Region9totalAreaEv(r4.ptr()) == 1200);

    RegionHolder copy1;
    _ZN7WebCore6RegionC1ERKS0_(copy1.ptr(), *r3.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(copy1.ptr()) == 1200);

    RegionHolder copy2;
    _ZN7WebCore6RegionC2ERKS0_(copy2.ptr(), *r3.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(copy2.ptr()) == 1200);

    RegionHolder move1;
    _ZN7WebCore6RegionC1EOS0_(move1.ptr(), std::move(*copy1.ptr()));
    Require(_ZNK7WebCore6Region9totalAreaEv(move1.ptr()) == 1200);

    RegionHolder move2;
    _ZN7WebCore6RegionC2EOS0_(move2.ptr(), std::move(*copy2.ptr()));
    Require(_ZNK7WebCore6Region9totalAreaEv(move2.ptr()) == 1200);

    RegionHolder assignedCopy;
    _ZN7WebCore6RegionC1Ev(assignedCopy.ptr());
    _ZN7WebCore6RegionaSERKS0_(assignedCopy.ptr(), *r3.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(assignedCopy.ptr()) == 1200);

    RegionHolder assignedMove;
    _ZN7WebCore6RegionC1Ev(assignedMove.ptr());
    _ZN7WebCore6RegionaSEOS0_(assignedMove.ptr(), std::move(*assignedCopy.ptr()));
    Require(_ZNK7WebCore6Region9totalAreaEv(assignedMove.ptr()) == 1200);

    WebCore::Region::Shape shape(rect);
    _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(&shape, WebCore::IntSize{5, -5});
    WebCore::IntRect shapeBounds = shape.bounds();
    Require(shapeBounds.m_location.m_x == 15);
    Require(shapeBounds.m_location.m_y == 15);
    Require(shapeBounds.m_size.m_width == 30);
    Require(shapeBounds.m_size.m_height == 40);

    _ZN7WebCore6RegionD1Ev(r3.ptr());
    _ZN7WebCore6RegionD1Ev(r4.ptr());
    _ZN7WebCore6RegionD1Ev(move1.ptr());
    _ZN7WebCore6RegionD1Ev(move2.ptr());
    _ZN7WebCore6RegionD1Ev(assignedCopy.ptr());
    _ZN7WebCore6RegionD1Ev(assignedMove.ptr());
}

static void TestRegionGeometryOps() {
    WebCore::IntRect rect1{WebCore::IntPoint{0, 0}, WebCore::IntSize{100, 100}};
    WebCore::IntRect rect2{WebCore::IntPoint{50, 50}, WebCore::IntSize{100, 100}};

    RegionHolder r1;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(r1.ptr(), rect1);
    RegionHolder r2;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(r2.ptr(), rect2);

    Require(_ZNK7WebCore6Region10intersectsERKS0_(r1.ptr(), *r2.ptr()));

    RegionHolder disjoint;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(disjoint.ptr(), WebCore::IntRect{WebCore::IntPoint{200, 200}, WebCore::IntSize{50, 50}});
    Require(!_ZNK7WebCore6Region10intersectsERKS0_(r1.ptr(), *disjoint.ptr()));

    WebCore::IntPoint insidePt{25, 25};
    WebCore::IntPoint outsidePt{150, 150};
    WebCore::IntPoint borderPtRight{100, 50};
    WebCore::IntPoint borderPtBottom{50, 100};
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(r1.ptr(), insidePt));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(r1.ptr(), outsidePt));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(r1.ptr(), borderPtRight));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(r1.ptr(), borderPtBottom));

    RegionHolder inner;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(inner.ptr(), WebCore::IntRect{WebCore::IntPoint{10, 10}, WebCore::IntSize{20, 20}});
    Require(_ZNK7WebCore6Region8containsERKS0_(r1.ptr(), *inner.ptr()));
    Require(!_ZNK7WebCore6Region8containsERKS0_(inner.ptr(), *r1.ptr()));
    Require(!_ZNK7WebCore6Region8containsERKS0_(r1.ptr(), *r2.ptr()));

    RegionHolder united;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(united.ptr(), rect1);
    _ZN7WebCore6Region5uniteERKS0_(united.ptr(), *r2.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(united.ptr()) == 17500);

    RegionHolder emptyR;
    _ZN7WebCore6RegionC1Ev(emptyR.ptr());
    _ZN7WebCore6Region5uniteERKS0_(united.ptr(), *emptyR.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(united.ptr()) == 17500);

    RegionHolder intersected;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(intersected.ptr(), rect1);
    _ZN7WebCore6Region9intersectERKS0_(intersected.ptr(), *r2.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(intersected.ptr()) == 2500);

    RegionHolder intersectedDisjoint;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(intersectedDisjoint.ptr(), rect1);
    _ZN7WebCore6Region9intersectERKS0_(intersectedDisjoint.ptr(), *disjoint.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(intersectedDisjoint.ptr()) == 0);

    RegionHolder subtracted;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(subtracted.ptr(), rect1);
    _ZN7WebCore6Region8subtractERKS0_(subtracted.ptr(), *r2.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(subtracted.ptr()) == 7500);

    RegionHolder subtractedSelf;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(subtractedSelf.ptr(), rect1);
    _ZN7WebCore6Region8subtractERKS0_(subtractedSelf.ptr(), *subtractedSelf.ptr());
    Require(_ZNK7WebCore6Region9totalAreaEv(subtractedSelf.ptr()) == 0);

    RegionHolder translated;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(translated.ptr(), rect1);
    _ZN7WebCore6Region9translateERKNS_7IntSizeE(translated.ptr(), WebCore::IntSize{20, 30});
    Require(translated.ptr()->bounds().m_location.m_x == 20);
    Require(translated.ptr()->bounds().m_location.m_y == 30);
    Require(_ZNK7WebCore6Region9totalAreaEv(translated.ptr()) == 10000);

    WTF::Vector<WebCore::IntRect, 1> rectList = _ZNK7WebCore6Region5rectsEv(subtracted.ptr());
    Require(!rectList.isEmpty());
    std::uint64_t sumArea = 0;
    for (std::size_t i = 0; i < rectList.size(); ++i) {
        sumArea += static_cast<std::uint64_t>(rectList[i].m_size.m_width) * static_cast<std::uint64_t>(rectList[i].m_size.m_height);
    }
    Require(sumArea == 7500);

    _ZN7WebCore6RegionD1Ev(r1.ptr());
    _ZN7WebCore6RegionD1Ev(r2.ptr());
    _ZN7WebCore6RegionD1Ev(disjoint.ptr());
    _ZN7WebCore6RegionD1Ev(inner.ptr());
    _ZN7WebCore6RegionD1Ev(united.ptr());
    _ZN7WebCore6RegionD1Ev(emptyR.ptr());
    _ZN7WebCore6RegionD1Ev(intersected.ptr());
    _ZN7WebCore6RegionD1Ev(intersectedDisjoint.ptr());
    _ZN7WebCore6RegionD1Ev(subtracted.ptr());
    _ZN7WebCore6RegionD1Ev(subtractedSelf.ptr());
    _ZN7WebCore6RegionD1Ev(translated.ptr());
}

int main() {
    TestRegionBasicsAndShape();
    TestRegionGeometryOps();
    return 0;
}
