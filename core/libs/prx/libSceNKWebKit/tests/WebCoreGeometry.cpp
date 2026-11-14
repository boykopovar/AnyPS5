#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <emmintrin.h>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore geometry check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool AlmostEqual(float a, float b, float eps = 1e-4f) {
    return std::abs(a - b) <= eps;
}

static bool AlmostEqual(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

static void TestFloatPoint() {
    WebCore::FloatPoint np = _ZN7WebCore10FloatPoint15narrowPrecisionEdd(12.345, 67.890);
    Require(AlmostEqual(np.m_x, 12.345f));
    Require(AlmostEqual(np.m_y, 67.890f));

    WebCore::FloatPoint norm{3.0f, 4.0f};
    _ZN7WebCore10FloatPoint9normalizeEv(&norm);
    Require(AlmostEqual(norm.m_x, 0.6f));
    Require(AlmostEqual(norm.m_y, 0.8f));

    WebCore::FloatPoint zeroNorm{0.0f, 0.0f};
    _ZN7WebCore10FloatPoint9normalizeEv(&zeroNorm);
    Require(AlmostEqual(zeroNorm.m_x, 0.0f));
    Require(AlmostEqual(zeroNorm.m_y, 0.0f));

    WebCore::IntPoint ip{10, 25};
    WebCore::FloatPoint fp1;
    _ZN7WebCore10FloatPointC1ERKNS_8IntPointE(&fp1, ip);
    Require(AlmostEqual(fp1.m_x, 10.0f));
    Require(AlmostEqual(fp1.m_y, 25.0f));

    WebCore::FloatPoint fp2;
    _ZN7WebCore10FloatPointC2ERKNS_8IntPointE(&fp2, ip);
    Require(AlmostEqual(fp2.m_x, 10.0f));
    Require(AlmostEqual(fp2.m_y, 25.0f));

    WebCore::AffineTransform at;
    _ZN7WebCore15AffineTransform9translateEdd(&at, 15.0, -5.0);
    WebCore::FloatPoint tp = _ZNK7WebCore10FloatPoint15matrixTransformERKNS_15AffineTransformE(&fp1, at);
    Require(AlmostEqual(tp.m_x, 25.0f));
    Require(AlmostEqual(tp.m_y, 20.0f));

    WebCore::TransformationMatrix tm;
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&tm, 2.0, 3.0);
    WebCore::FloatPoint tmp = _ZNK7WebCore10FloatPoint15matrixTransformERKNS_20TransformationMatrixE(&fp1, tm);
    Require(AlmostEqual(tmp.m_x, 20.0f));
    Require(AlmostEqual(tmp.m_y, 75.0f));

    WebCore::FloatPoint minP{0.0f, 0.0f};
    WebCore::FloatPoint maxP{100.0f, 100.0f};
    WebCore::FloatPoint outP{-10.0f, 120.0f};
    WebCore::FloatPoint clamped = _ZNK7WebCore10FloatPoint18constrainedBetweenERKS0_S2_(&outP, minP, maxP);
    Require(AlmostEqual(clamped.m_x, 0.0f));
    Require(AlmostEqual(clamped.m_y, 100.0f));
}

static void TestFloatSize() {
    WebCore::FloatSize sz1{0.0f, 0.0f};
    Require(_ZNK7WebCore9FloatSize6isZeroEv(&sz1));
    WebCore::FloatSize sz2{1.0f, 0.0f};
    Require(!_ZNK7WebCore9FloatSize6isZeroEv(&sz2));

    WebCore::FloatSize minS{5.0f, 10.0f};
    WebCore::FloatSize maxS{50.0f, 60.0f};
    WebCore::FloatSize curS{1.0f, 100.0f};
    WebCore::FloatSize clamped = _ZNK7WebCore9FloatSize18constrainedBetweenERKS0_S2_(&curS, minS, maxS);
    Require(AlmostEqual(clamped.m_width, 5.0f));
    Require(AlmostEqual(clamped.m_height, 60.0f));
}

static void TestFloatSizeFromIntSize() {
    WebCore::IntSize dimensions{-17, 23};

    WebCore::FloatSize c1{};
    _ZN7WebCore9FloatSizeC1ERKNS_7IntSizeE(&c1, dimensions);
    Require(c1.m_width == -17.0f);
    Require(c1.m_height == 23.0f);

    dimensions = {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    WebCore::FloatSize c2{};
    _ZN7WebCore9FloatSizeC2ERKNS_7IntSizeE(&c2, dimensions);
    Require(c2.m_width == static_cast<float>(std::numeric_limits<int>::min()));
    Require(c2.m_height == static_cast<float>(std::numeric_limits<int>::max()));
}

static void TestFloatRect() {
    WebCore::IntRect ir{WebCore::IntPoint{5, 10}, WebCore::IntSize{20, 30}};
    WebCore::FloatRect fr1;
    _ZN7WebCore9FloatRectC1ERKNS_7IntRectE(&fr1, ir);
    Require(AlmostEqual(fr1.m_location.m_x, 5.0f));
    Require(AlmostEqual(fr1.m_location.m_y, 10.0f));
    Require(AlmostEqual(fr1.m_size.m_width, 20.0f));
    Require(AlmostEqual(fr1.m_size.m_height, 30.0f));

    WebCore::FloatRect fr2;
    _ZN7WebCore9FloatRectC2ERKNS_7IntRectE(&fr2, ir);
    Require(AlmostEqual(fr2.m_location.m_x, 5.0f));
    Require(AlmostEqual(fr2.m_size.m_width, 20.0f));

    _ZN7WebCore9FloatRect5scaleEff(&fr1, 2.0f, 3.0f);
    Require(AlmostEqual(fr1.m_location.m_x, 10.0f));
    Require(AlmostEqual(fr1.m_location.m_y, 30.0f));
    Require(AlmostEqual(fr1.m_size.m_width, 40.0f));
    Require(AlmostEqual(fr1.m_size.m_height, 90.0f));

    WebCore::FloatRect rA{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatSize{10.0f, 10.0f}};
    WebCore::FloatRect rB{WebCore::FloatPoint{5.0f, 5.0f}, WebCore::FloatSize{15.0f, 15.0f}};
    _ZN7WebCore9FloatRect5uniteERKS0_(&rA, rB);
    Require(AlmostEqual(rA.m_location.m_x, 0.0f));
    Require(AlmostEqual(rA.m_location.m_y, 0.0f));
    Require(AlmostEqual(rA.m_size.m_width, 20.0f));
    Require(AlmostEqual(rA.m_size.m_height, 20.0f));

    WebCore::FloatRect rInter{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatSize{10.0f, 10.0f}};
    WebCore::FloatRect rOther{WebCore::FloatPoint{4.0f, 3.0f}, WebCore::FloatSize{10.0f, 10.0f}};
    _ZN7WebCore9FloatRect9intersectERKS0_(&rInter, rOther);
    Require(AlmostEqual(rInter.m_location.m_x, 4.0f));
    Require(AlmostEqual(rInter.m_location.m_y, 3.0f));
    Require(AlmostEqual(rInter.m_size.m_width, 6.0f));
    Require(AlmostEqual(rInter.m_size.m_height, 7.0f));

    WebCore::FloatRect rDisjoint{WebCore::FloatPoint{50.0f, 50.0f}, WebCore::FloatSize{10.0f, 10.0f}};
    Require(!_ZNK7WebCore9FloatRect10intersectsERKS0_(&rInter, rDisjoint));
    Require(!_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(&rInter, rDisjoint));

    WebCore::FloatRect rTouch{WebCore::FloatPoint{10.0f, 3.0f}, WebCore::FloatSize{5.0f, 5.0f}};
    Require(!_ZNK7WebCore9FloatRect10intersectsERKS0_(&rInter, rTouch));
    Require(_ZNK7WebCore9FloatRect21inclusivelyIntersectsERKS0_(&rInter, rTouch));

    WebCore::FloatPoint insidePt{5.0f, 4.0f};
    Require(_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(
        &rInter, insidePt, 0u));
    WebCore::FloatPoint outsidePt{0.0f, 0.0f};
    Require(!_ZNK7WebCore9FloatRect8containsERKNS_10FloatPointENS0_12ContainsModeE(
        &rInter, outsidePt, 0u));

    WebCore::FloatRect smallInside{WebCore::FloatPoint{5.0f, 4.0f}, WebCore::FloatSize{2.0f, 2.0f}};
    Require(_ZNK7WebCore9FloatRect8containsERKS0_(&rInter, smallInside));

    WebCore::FloatRect rExt{WebCore::FloatPoint{10.0f, 10.0f}, WebCore::FloatSize{10.0f, 10.0f}};
    WebCore::FloatPoint rExtPoint{25.0f, 25.0f};
    _ZN7WebCore9FloatRect6extendERKNS_10FloatPointE(&rExt, rExtPoint);
    Require(AlmostEqual(rExt.m_location.m_x, 10.0f));
    Require(AlmostEqual(rExt.m_location.m_y, 10.0f));
    Require(AlmostEqual(rExt.m_size.m_width, 15.0f));
    Require(AlmostEqual(rExt.m_size.m_height, 15.0f));
}

static void TestFloatQuad() {
    WebCore::FloatQuad q{
        WebCore::FloatPoint{0.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 10.0f},
        WebCore::FloatPoint{0.0f, 10.0f}
    };
    WebCore::FloatRect bb = _ZNK7WebCore9FloatQuad11boundingBoxEv(&q);
    Require(AlmostEqual(bb.m_location.m_x, 0.0f));
    Require(AlmostEqual(bb.m_location.m_y, 0.0f));
    Require(AlmostEqual(bb.m_size.m_width, 10.0f));
    Require(AlmostEqual(bb.m_size.m_height, 10.0f));

    Require(_ZNK7WebCore9FloatQuad13isRectilinearEv(&q));

    WebCore::FloatPoint inside{5.0f, 5.0f};
    WebCore::FloatPoint outside{15.0f, 5.0f};
    Require(_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(&q, inside));
    Require(!_ZNK7WebCore9FloatQuad13containsPointERKNS_10FloatPointE(&q, outside));

    WebCore::FloatQuad smallQ{
        WebCore::FloatPoint{2.0f, 2.0f},
        WebCore::FloatPoint{8.0f, 2.0f},
        WebCore::FloatPoint{8.0f, 8.0f},
        WebCore::FloatPoint{2.0f, 8.0f}
    };
    Require(_ZNK7WebCore9FloatQuad12containsQuadERKS0_(&q, smallQ));
}

static void TestFloatRoundedRect() {
    WebCore::FloatRect rect{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatSize{100.0f, 100.0f}};
    WebCore::FloatRoundedRect::Radii radii{
        WebCore::FloatSize{10.0f, 10.0f},
        WebCore::FloatSize{10.0f, 10.0f},
        WebCore::FloatSize{10.0f, 10.0f},
        WebCore::FloatSize{10.0f, 10.0f}
    };
    WebCore::FloatRoundedRect frr1;
    _ZN7WebCore16FloatRoundedRectC1ERKNS_9FloatRectERKNS0_5RadiiE(&frr1, rect, radii);
    Require(AlmostEqual(frr1.m_rect.m_size.m_width, 100.0f));
    Require(AlmostEqual(frr1.m_radii.m_topLeft.m_width, 10.0f));

    WebCore::FloatRoundedRect frr2;
    _ZN7WebCore16FloatRoundedRectC2ERKNS_9FloatRectERKNS0_5RadiiE(&frr2, rect, radii);
    Require(AlmostEqual(frr2.m_rect.m_size.m_width, 100.0f));
}

static void TestIntPointIntSizeIntRect() {
    WebCore::FloatPoint fp{15.7f, 25.2f};
    WebCore::IntPoint ip1;
    _ZN7WebCore8IntPointC1ERKNS_10FloatPointE(&ip1, fp);
    Require(ip1.m_x == 15);
    Require(ip1.m_y == 25);

    WebCore::IntPoint ip2;
    _ZN7WebCore8IntPointC2ERKNS_10FloatPointE(&ip2, fp);
    Require(ip2.m_x == 15);

    WebCore::IntPoint minP{0, 0};
    WebCore::IntPoint maxP{10, 10};
    WebCore::IntPoint testP{15, -5};
    WebCore::IntPoint clamped = _ZNK7WebCore8IntPoint18constrainedBetweenERKS0_S2_(&testP, minP, maxP);
    Require(clamped.m_x == 10);
    Require(clamped.m_y == 0);

    WebCore::FloatSize fs{20.0f, 30.0f};
    WebCore::IntSize is1;
    _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(&is1, fs);
    Require(is1.m_width == 20);
    Require(is1.m_height == 30);

    WebCore::IntSize is2;
    _ZN7WebCore7IntSizeC2ERKNS_9FloatSizeE(&is2, fs);
    Require(is2.m_width == 20);

    WebCore::IntSize minS{5, 5};
    WebCore::IntSize maxS{50, 50};
    WebCore::IntSize testS{0, 100};
    WebCore::IntSize clampedS = _ZNK7WebCore7IntSize18constrainedBetweenERKS0_S2_(&testS, minS, maxS);
    Require(clampedS.m_width == 5);
    Require(clampedS.m_height == 50);

    WebCore::IntRect ir{WebCore::IntPoint{10, 20}, WebCore::IntSize{30, 40}};
    _ZN7WebCore7IntRect5scaleEf(&ir, 2.0f);
    Require(ir.m_location.m_x == 20);
    Require(ir.m_location.m_y == 40);
    Require(ir.m_size.m_width == 60);
    Require(ir.m_size.m_height == 80);

    WebCore::IntRect irA{WebCore::IntPoint{0, 0}, WebCore::IntSize{10, 10}};
    WebCore::IntRect irB{WebCore::IntPoint{5, 5}, WebCore::IntSize{15, 15}};
    _ZN7WebCore7IntRect5uniteERKS0_(&irA, irB);
    Require(irA.m_location.m_x == 0);
    Require(irA.m_size.m_width == 20);

    WebCore::IntRect irInter{WebCore::IntPoint{0, 0}, WebCore::IntSize{10, 10}};
    WebCore::IntRect irInterTarget{WebCore::IntPoint{3, 3}, WebCore::IntSize{10, 10}};
    _ZN7WebCore7IntRect9intersectERKS0_(&irInter, irInterTarget);
    Require(irInter.m_location.m_x == 3);
    Require(irInter.m_size.m_width == 7);

    Require(_ZNK7WebCore7IntRect10intersectsERKS0_(&irInter, irInterTarget));
    WebCore::IntRect insideRect{WebCore::IntPoint{4, 4}, WebCore::IntSize{2, 2}};
    Require(_ZNK7WebCore7IntRect8containsERKS0_(&irInter, insideRect));

    WebCore::LayoutRect lr{WebCore::LayoutPoint{64, 128}, WebCore::LayoutSize{192, 256}};
    WebCore::IntRect irFromLr1;
    _ZN7WebCore7IntRectC1ERKNS_10LayoutRectE(&irFromLr1, lr);
    Require(irFromLr1.m_location.m_x == 1);
    Require(irFromLr1.m_location.m_y == 2);
    Require(irFromLr1.m_size.m_width == 3);
    Require(irFromLr1.m_size.m_height == 4);

    WebCore::IntRect irFromLr2;
    _ZN7WebCore7IntRectC2ERKNS_10LayoutRectE(&irFromLr2, lr);
    Require(irFromLr2.m_location.m_x == 1);

    WebCore::FloatRect fr{WebCore::FloatPoint{5.0f, 6.0f}, WebCore::FloatSize{7.0f, 8.0f}};
    WebCore::IntRect irFromFr1;
    _ZN7WebCore7IntRectC1ERKNS_9FloatRectE(&irFromFr1, fr);
    Require(irFromFr1.m_location.m_x == 5);
    Require(irFromFr1.m_size.m_width == 7);

    WebCore::IntRect irFromFr2;
    _ZN7WebCore7IntRectC2ERKNS_9FloatRectE(&irFromFr2, fr);
    Require(irFromFr2.m_location.m_x == 5);
}

static void TestLayoutRect() {
    WebCore::FloatRect fr{WebCore::FloatPoint{1.5f, 2.25f}, WebCore::FloatSize{10.0f, 20.0f}};
    WebCore::LayoutRect lr1;
    _ZN7WebCore10LayoutRectC1ERKNS_9FloatRectE(&lr1, fr);
    Require(lr1.m_location.m_x.m_value == static_cast<int>(std::round(1.5f * 64.0f)));
    Require(lr1.m_location.m_y.m_value == static_cast<int>(std::round(2.25f * 64.0f)));

    WebCore::LayoutRect lr2;
    _ZN7WebCore10LayoutRectC2ERKNS_9FloatRectE(&lr2, fr);
    Require(lr2.m_location.m_x.m_value == lr1.m_location.m_x.m_value);

    _ZN7WebCore10LayoutRect5scaleEf(&lr1, 2.0f);
    Require(lr1.m_location.m_x.m_value == static_cast<int>(std::round(3.0f * 64.0f)));

    WebCore::LayoutRect lrA{WebCore::LayoutPoint{0, 0}, WebCore::LayoutSize{640, 640}};
    WebCore::LayoutRect lrB{WebCore::LayoutPoint{320, 320}, WebCore::LayoutSize{640, 640}};
    _ZN7WebCore10LayoutRect5uniteERKS0_(&lrA, lrB);
    Require(lrA.m_location.m_x.m_value == 0);
    Require(lrA.m_size.m_width.m_value == 960);

    WebCore::LayoutRect insideLr{WebCore::LayoutPoint{64, 64}, WebCore::LayoutSize{128, 128}};
    Require(_ZNK7WebCore10LayoutRect8containsERKS0_(&lrA, insideLr));
}

static void TestRoundedRect() {
    WebCore::LayoutRect lr{WebCore::LayoutPoint{0, 0}, WebCore::LayoutSize{6400, 6400}};
    WebCore::LayoutSize sz{640, 640};
    WebCore::RoundedRect rr1;
    _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&rr1, lr, sz, sz, sz, sz);
    Require(rr1.m_radii.m_topLeft.m_width.m_value == 640);

    WebCore::LayoutSize maxWidth{std::numeric_limits<int>::max(), 0};
    WebCore::RoundedRect overflowing;
    _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&overflowing, lr, maxWidth, maxWidth, WebCore::LayoutSize{0, 0}, WebCore::LayoutSize{0, 0});
    Require(!_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(&overflowing, lr));

    WebCore::LayoutRect maxLr{WebCore::LayoutPoint{0, 0}, WebCore::LayoutSize{std::numeric_limits<int>::max(), 6400}};
    WebCore::RoundedRect saturating;
    _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&saturating, maxLr, maxWidth, maxWidth, WebCore::LayoutSize{0, 0}, WebCore::LayoutSize{0, 0});
    WebCore::LayoutRect smallLr{WebCore::LayoutPoint{64, 64}, WebCore::LayoutSize{128, 128}};
    Require(_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(&saturating, smallLr));

    WebCore::RoundedRect rr2;
    _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS_10LayoutSizeES6_S6_S6_(&rr2, lr, sz, sz, sz, sz);
    Require(rr2.m_radii.m_topLeft.m_width.m_value == 640);

    WebCore::LayoutRect insideLr{WebCore::LayoutPoint{1280, 1280}, WebCore::LayoutSize{640, 640}};
    Require(_ZNK7WebCore11RoundedRect8containsERKNS_10LayoutRectE(&rr1, insideLr));
}

static void TestColor() {
    WebCore::Color c1;
    c1.m_colorAndFlags = 0xFF0000FFull;

    WebCore::Color cCopy;
    _ZN7WebCore5ColorC1ERKS0_(&cCopy, c1);
    Require(cCopy.m_colorAndFlags == c1.m_colorAndFlags);

    WebCore::Color cCopy2;
    _ZN7WebCore5ColorC2ERKS0_(&cCopy2, c1);
    Require(cCopy2.m_colorAndFlags == c1.m_colorAndFlags);

    WebCore::Color cMoveSrc = c1;
    WebCore::Color cMove;
    _ZN7WebCore5ColorC1EOS0_(&cMove, std::move(cMoveSrc));
    Require(cMove.m_colorAndFlags == c1.m_colorAndFlags);

    WebCore::Color cAssign;
    _ZN7WebCore5ColoraSERKS0_(&cAssign, c1);
    Require(cAssign.m_colorAndFlags == c1.m_colorAndFlags);

    WebCore::Color cMoveAssign;
    _ZN7WebCore5ColoraSEOS0_(&cMoveAssign, std::move(cAssign));
    Require(cMoveAssign.m_colorAndFlags == c1.m_colorAndFlags);

    WebCore::Color cAlpha = _ZNK7WebCore5Color14colorWithAlphaEf(&c1, 0.5f);
    Require((cAlpha.m_colorAndFlags & 0xFF) == 128);

    WebCore::Color cWhite;
    cWhite.m_colorAndFlags = 0xFFFFFFFFull;
    Require(AlmostEqual(_ZNK7WebCore5Color9lightnessEv(&cWhite), 1.0));
    Require(AlmostEqual(_ZNK7WebCore5Color9luminanceEv(&cWhite), 1.0));

    WebCore::Color cBlack;
    cBlack.m_colorAndFlags = 0x000000FFull;
    Require(AlmostEqual(_ZNK7WebCore5Color9lightnessEv(&cBlack), 0.0));
    Require(AlmostEqual(_ZNK7WebCore5Color9luminanceEv(&cBlack), 0.0));

    WebCore::Color cDarkened = _ZNK7WebCore5Color8darkenedEv(&cWhite);
    Require(((cDarkened.m_colorAndFlags >> 24) & 0xFF) < 255);

    WebCore::Color cLightened = _ZNK7WebCore5Color9lightenedEv(&cBlack);
    Require(((cLightened.m_colorAndFlags >> 24) & 0xFF) > 0);
}

static void TestAffineTransform() {
    WebCore::AffineTransform at;
    Require(_ZNK7WebCore15AffineTransform10isIdentityEv(&at));

    _ZN7WebCore15AffineTransform9translateEdd(&at, 10.0, 20.0);
    Require(!_ZNK7WebCore15AffineTransform10isIdentityEv(&at));

    WebCore::FloatPoint p{0.0f, 0.0f};
    WebCore::FloatPoint mp = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(&at, p);
    Require(AlmostEqual(mp.m_x, 10.0f));
    Require(AlmostEqual(mp.m_y, 20.0f));

    WebCore::IntPoint ip{5, 5};
    WebCore::IntPoint mip = _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(&at, ip);
    Require(mip.m_x == 15);
    Require(mip.m_y == 25);

    _ZN7WebCore15AffineTransform12makeIdentityEv(&at);
    Require(_ZNK7WebCore15AffineTransform10isIdentityEv(&at));

    _ZN7WebCore15AffineTransform5scaleEd(&at, 2.0);
    Require(AlmostEqual(_ZNK7WebCore15AffineTransform6xScaleEv(&at), 2.0));
    Require(AlmostEqual(_ZNK7WebCore15AffineTransform6yScaleEv(&at), 2.0));

    WebCore::FloatSize scaleSz{1.5f, 0.5f};
    _ZN7WebCore15AffineTransform5scaleERKNS_9FloatSizeE(&at, scaleSz);
    Require(AlmostEqual(_ZNK7WebCore15AffineTransform6xScaleEv(&at), 3.0));
    Require(AlmostEqual(_ZNK7WebCore15AffineTransform6yScaleEv(&at), 1.0));

    WebCore::FloatPoint transPt{2.0f, 3.0f};
    _ZN7WebCore15AffineTransform9translateERKNS_10FloatPointE(&at, transPt);

    WebCore::FloatSize transSz{1.0f, 2.0f};
    _ZN7WebCore15AffineTransform9translateERKNS_9FloatSizeE(&at, transSz);

    WebCore::AffineTransform skewT;
    _ZN7WebCore15AffineTransform4skewEdd(&skewT, 0.0, 45.0);
    Require(!_ZNK7WebCore15AffineTransform10isIdentityEv(&skewT));

    WebCore::AffineTransform shearT;
    _ZN7WebCore15AffineTransform5shearEdd(&shearT, 0.2, 0.3);
    Require(!_ZNK7WebCore15AffineTransform10isIdentityEv(&shearT));

    WebCore::AffineTransform multT;
    _ZN7WebCore15AffineTransform8multiplyERKS0_(&multT, skewT);

    _ZN7WebCore15AffineTransform12makeIdentityEv(&at);
    _ZN7WebCore15AffineTransform5flipXEv(&at);
    Require(AlmostEqual(at.m_transform[0], -1.0));

    _ZN7WebCore15AffineTransform5flipYEv(&at);
    Require(AlmostEqual(at.m_transform[3], -1.0));

    _ZN7WebCore15AffineTransform12makeIdentityEv(&at);
    _ZN7WebCore15AffineTransform6rotateEd(&at, 90.0);
    WebCore::FloatPoint rp = _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(
        &at, WebCore::FloatPoint{1.0f, 0.0f});
    Require(AlmostEqual(rp.m_x, 0.0f));
    Require(AlmostEqual(rp.m_y, 1.0f));

    _ZN7WebCore15AffineTransform12makeIdentityEv(&at);
    _ZN7WebCore15AffineTransform15scaleNonUniformEdd(&at, 2.0, 3.0);
    WebCore::FloatRect fr{WebCore::FloatPoint{10.0f, 10.0f}, WebCore::FloatSize{5.0f, 5.0f}};
    WebCore::FloatRect mfr = _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&at, fr);
    Require(AlmostEqual(mfr.m_location.m_x, 20.0f));
    Require(AlmostEqual(mfr.m_location.m_y, 30.0f));
    Require(AlmostEqual(mfr.m_size.m_width, 10.0f));
    Require(AlmostEqual(mfr.m_size.m_height, 15.0f));

    WebCore::IntRect ir{WebCore::IntPoint{10, 10}, WebCore::IntSize{5, 5}};
    WebCore::IntRect mir = _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(&at, ir);
    Require(mir.m_location.m_x == 20);
    Require(mir.m_size.m_width == 10);

    WebCore::FloatSize fsz{10.0f, 20.0f};
    WebCore::FloatSize mfsz = _ZNK7WebCore15AffineTransform7mapSizeERKNS_9FloatSizeE(&at, fsz);
    Require(AlmostEqual(mfsz.m_width, 20.0f));
    Require(AlmostEqual(mfsz.m_height, 60.0f));

    WebCore::IntSize isz{10, 20};
    WebCore::IntSize misz = _ZNK7WebCore15AffineTransform7mapSizeERKNS_7IntSizeE(&at, isz);
    Require(misz.m_width == 20);
    Require(misz.m_height == 60);

    WebCore::FloatQuad q{
        WebCore::FloatPoint{0.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 10.0f},
        WebCore::FloatPoint{0.0f, 10.0f}
    };
    WebCore::FloatQuad mq = _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&at, q);
    Require(AlmostEqual(mq.m_p2.m_x, 20.0f));

    WebCore::TransformationMatrix tm = _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(&at);
    Require(AlmostEqual(tm.m_matrix[0][0], 2.0));
    Require(AlmostEqual(tm.m_matrix[1][1], 3.0));
}

static bool MatrixNear(const WebCore::TransformationMatrix& a, const WebCore::TransformationMatrix& b, double eps = 1e-9) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!AlmostEqual(a.m_matrix[i][j], b.m_matrix[i][j], eps)) {
                return false;
            }
        }
    }
    return true;
}

static WebCore::TransformationMatrix Rotated(double rx, double ry, double rz) {
    WebCore::TransformationMatrix m;
    _ZN7WebCore20TransformationMatrix8rotate3dEddd(&m, rx, ry, rz);
    return m;
}

static WebCore::TransformationMatrix AxisRotated(double x, double y, double z, double angle) {
    WebCore::TransformationMatrix m;
    _ZN7WebCore20TransformationMatrix8rotate3dEdddd(&m, x, y, z, angle);
    return m;
}

static WebCore::TransformationMatrix Product(WebCore::TransformationMatrix a, const WebCore::TransformationMatrix& b) {
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&a, b);
    return a;
}

static void TestTransformationMatrixScale3dAndAxisRotation() {
    WebCore::TransformationMatrix m;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&m, 1.0, 2.0, 3.0);
    Require(&_ZN7WebCore20TransformationMatrix7scale3dEddd(&m, 2.0, 3.0, 4.0) == &m);
    Require(m.m_matrix[0][0] == 2.0 && m.m_matrix[1][1] == 3.0 && m.m_matrix[2][2] == 4.0 && m.m_matrix[3][3] == 1.0);
    Require(m.m_matrix[3][0] == 1.0 && m.m_matrix[3][1] == 2.0 && m.m_matrix[3][2] == 3.0);

    WebCore::TransformationMatrix unchanged = m;
    Require(&_ZN7WebCore20TransformationMatrix8rotate3dEdddd(&unchanged, 0.0, 0.0, 0.0, 45.0) == &unchanged);
    Require(std::memcmp(&unchanged, &m, sizeof(m)) == 0);

    Require(MatrixNear(AxisRotated(0.0, 0.0, 2.0, 90.0), Rotated(0.0, 0.0, 90.0), 1e-12));
    Require(MatrixNear(AxisRotated(3.0, 0.0, 0.0, 30.0), Rotated(30.0, 0.0, 0.0), 1e-12));
    Require(MatrixNear(AxisRotated(0.0, -1.0, 0.0, 60.0), Rotated(0.0, -60.0, 0.0), 1e-12));

    WebCore::TransformationMatrix cycle = AxisRotated(1.0, 1.0, 1.0, 120.0);
    WebCore::TransformationMatrix permutation;
    permutation.m_matrix[0][0] = 0.0;
    permutation.m_matrix[0][1] = 1.0;
    permutation.m_matrix[1][1] = 0.0;
    permutation.m_matrix[1][2] = 1.0;
    permutation.m_matrix[2][2] = 0.0;
    permutation.m_matrix[2][0] = 1.0;
    Require(MatrixNear(cycle, permutation, 1e-12));
    Require(MatrixNear(Product(AxisRotated(1.0, 2.0, 3.0, 40.0), AxisRotated(1.0, 2.0, 3.0, -40.0)), WebCore::TransformationMatrix{}, 1e-12));
}

static void TestTransformationMatrixInverse() {
    WebCore::TransformationMatrix identity;
    auto inverse = _ZNK7WebCore20TransformationMatrix7inverseEv(&identity);
    Require(inverse.m_engaged && std::memcmp(&inverse.m_value, &identity, sizeof(identity)) == 0);

    WebCore::TransformationMatrix translation;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&translation, 5.0, -7.0, 9.0);
    inverse = _ZNK7WebCore20TransformationMatrix7inverseEv(&translation);
    WebCore::TransformationMatrix expected;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&expected, -5.0, 7.0, -9.0);
    Require(inverse.m_engaged && std::memcmp(&inverse.m_value, &expected, sizeof(expected)) == 0);

    WebCore::TransformationMatrix affine;
    _ZN7WebCore20TransformationMatrix9translateEdd(&affine, 3.0, 2.0);
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&affine, 2.0, 4.0);
    inverse = _ZNK7WebCore20TransformationMatrix7inverseEv(&affine);
    expected = WebCore::TransformationMatrix{};
    expected.m_matrix[0][0] = 0.5;
    expected.m_matrix[1][1] = 0.25;
    expected.m_matrix[3][0] = -1.5;
    expected.m_matrix[3][1] = -0.5;
    Require(inverse.m_engaged && MatrixNear(inverse.m_value, expected, 0.0));

    WebCore::TransformationMatrix flat;
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&flat, 1.0, 0.0);
    Require(!_ZNK7WebCore20TransformationMatrix7inverseEv(&flat).m_engaged);

    WebCore::TransformationMatrix projective = AxisRotated(1.0, 2.0, 3.0, 40.0);
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&projective, 4.0, -3.0, 2.0);
    _ZN7WebCore20TransformationMatrix7scale3dEddd(&projective, 1.5, 0.5, 2.0);
    projective.m_matrix[2][3] = -0.01;
    inverse = _ZNK7WebCore20TransformationMatrix7inverseEv(&projective);
    Require(inverse.m_engaged);
    Require(MatrixNear(Product(projective, inverse.m_value), WebCore::TransformationMatrix{}, 1e-12));
    Require(MatrixNear(Product(inverse.m_value, projective), WebCore::TransformationMatrix{}, 1e-12));

    WebCore::TransformationMatrix singular = projective;
    _ZN7WebCore20TransformationMatrix7scale3dEddd(&singular, 1.0, 1.0, 0.0);
    Require(!_ZNK7WebCore20TransformationMatrix7inverseEv(&singular).m_engaged);
}

static void TestTransformationMatrixBlend() {
    WebCore::TransformationMatrix identity;
    WebCore::TransformationMatrix same;
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&same, identity, 0.3);
    Require(std::memcmp(&same, &identity, sizeof(identity)) == 0);

    WebCore::TransformationMatrix moved;
    _ZN7WebCore20TransformationMatrix9translateEdd(&moved, 10.0, 20.0);
    WebCore::TransformationMatrix half = moved;
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&half, identity, 0.5);
    WebCore::TransformationMatrix expected;
    _ZN7WebCore20TransformationMatrix9translateEdd(&expected, 5.0, 10.0);
    Require(MatrixNear(half, expected, 1e-12));

    for (double progress : {0.0, 0.25, 0.5, 1.0}) {
        WebCore::TransformationMatrix to = Rotated(0.0, 0.0, 50.0);
        _ZN7WebCore20TransformationMatrix6blend2ERKS0_d(&to, Rotated(0.0, 0.0, 10.0), progress);
        Require(MatrixNear(to, Rotated(0.0, 0.0, 10.0 + 40.0 * progress), 1e-12));
    }

    WebCore::TransformationMatrix shortWay = Rotated(0.0, 0.0, -170.0);
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&shortWay, Rotated(0.0, 0.0, 170.0), 0.5);
    Require(MatrixNear(shortWay, Rotated(0.0, 0.0, 180.0), 1e-12));

    WebCore::TransformationMatrix scaled;
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&scaled, 3.0, 5.0);
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&scaled, identity, 0.5);
    expected = WebCore::TransformationMatrix{};
    _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(&expected, 2.0, 3.0);
    Require(MatrixNear(scaled, expected, 1e-12));

    for (double progress : {0.0, 0.5, 1.0}) {
        WebCore::TransformationMatrix turned = AxisRotated(0.0, 1.0, 0.0, 90.0);
        _ZN7WebCore20TransformationMatrix5blendERKS0_d(&turned, AxisRotated(0.0, 1.0, 0.0, 10.0), progress);
        Require(MatrixNear(turned, AxisRotated(0.0, 1.0, 0.0, 10.0 + 80.0 * progress), 1e-12));
    }

    WebCore::TransformationMatrix deep;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&deep, 0.0, 0.0, 30.0);
    _ZN7WebCore20TransformationMatrix7scale3dEddd(&deep, 1.0, 1.0, 3.0);
    WebCore::TransformationMatrix closer;
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&closer, 0.0, 0.0, 10.0);
    _ZN7WebCore20TransformationMatrix6blend4ERKS0_d(&deep, closer, 0.5);
    expected = WebCore::TransformationMatrix{};
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&expected, 0.0, 0.0, 20.0);
    _ZN7WebCore20TransformationMatrix7scale3dEddd(&expected, 1.0, 1.0, 2.0);
    Require(MatrixNear(deep, expected, 1e-12));

    WebCore::TransformationMatrix perspective;
    perspective.m_matrix[2][3] = -0.01;
    _ZN7WebCore20TransformationMatrix5blendERKS0_d(&perspective, identity, 0.5);
    expected = WebCore::TransformationMatrix{};
    expected.m_matrix[2][3] = -0.005;
    Require(MatrixNear(perspective, expected, 1e-12));

    WebCore::TransformationMatrix degenerate;
    degenerate.m_matrix[3][3] = 0.0;
    degenerate.m_matrix[2][3] = 1.0;
    for (double progress : {0.25, 0.75}) {
        WebCore::TransformationMatrix target = moved;
        _ZN7WebCore20TransformationMatrix5blendERKS0_d(&target, degenerate, progress);
        const auto& wanted = progress < 0.5 ? degenerate : moved;
        Require(std::memcmp(&target, &wanted, sizeof(target)) == 0);
    }
}

static void TestTransformationMatrix() {
    WebCore::TransformationMatrix tm;
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&tm));

    _ZN7WebCore20TransformationMatrix11translate3dEddd(&tm, 5.0, 10.0, 15.0);
    Require(AlmostEqual(tm.m_matrix[3][0], 5.0));
    Require(AlmostEqual(tm.m_matrix[3][1], 10.0));
    Require(AlmostEqual(tm.m_matrix[3][2], 15.0));

    _ZN7WebCore20TransformationMatrix9translateEdd(&tm, 5.0, 5.0);
    Require(AlmostEqual(tm.m_matrix[3][0], 10.0));
    Require(AlmostEqual(tm.m_matrix[3][1], 15.0));

    WebCore::FloatPoint p{0.0f, 0.0f};
    WebCore::FloatPoint mp = _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(&tm, p);
    Require(AlmostEqual(mp.m_x, 10.0f));
    Require(AlmostEqual(mp.m_y, 15.0f));

    WebCore::TransformationMatrix scaleMat;
    _ZN7WebCore20TransformationMatrix5scaleEd(&scaleMat, 3.0);
    _ZN7WebCore20TransformationMatrix5flipXEv(&scaleMat);
    Require(AlmostEqual(scaleMat.m_matrix[0][0], -3.0));
    _ZN7WebCore20TransformationMatrix5flipYEv(&scaleMat);
    Require(AlmostEqual(scaleMat.m_matrix[1][1], -3.0));
    _ZN7WebCore20TransformationMatrix8multiplyERKS0_(&tm, scaleMat);

    WebCore::TransformationMatrix rotMat;
    _ZN7WebCore20TransformationMatrix6rotateEd(&rotMat, 90.0);
    Require(AlmostEqual(rotMat.m_matrix[0][1], 1.0));

    WebCore::TransformationMatrix rot3dMat;
    _ZN7WebCore20TransformationMatrix8rotate3dEddd(&rot3dMat, 0.0, 90.0, 0.0);
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&rot3dMat));

    WebCore::TransformationMatrix skewMat;
    _ZN7WebCore20TransformationMatrix4skewEdd(&skewMat, 30.0, 45.0);
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&skewMat));

    _ZN7WebCore20TransformationMatrix10makeAffineEv(&tm);
    Require(AlmostEqual(tm.m_matrix[0][2], 0.0));
    Require(AlmostEqual(tm.m_matrix[3][2], 0.0));

    WebCore::AffineTransform at = _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(&tm);
    Require(AlmostEqual(at.m_transform[0], tm.m_matrix[0][0]));

    WebCore::FloatRect srcR{WebCore::FloatPoint{0.0f, 0.0f}, WebCore::FloatSize{100.0f, 100.0f}};
    WebCore::FloatRect dstR{WebCore::FloatPoint{50.0f, 50.0f}, WebCore::FloatSize{200.0f, 200.0f}};
    WebCore::TransformationMatrix r2r = _ZN7WebCore20TransformationMatrix10rectToRectERKNS_9FloatRectES3_(srcR, dstR);
    Require(AlmostEqual(r2r.m_matrix[0][0], 2.0));
    Require(AlmostEqual(r2r.m_matrix[1][1], 2.0));
    Require(AlmostEqual(r2r.m_matrix[3][0], 50.0));
    Require(AlmostEqual(r2r.m_matrix[3][1], 50.0));

    WebCore::FloatQuad q{
        WebCore::FloatPoint{0.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 0.0f},
        WebCore::FloatPoint{10.0f, 10.0f},
        WebCore::FloatPoint{0.0f, 10.0f}
    };
    WebCore::FloatQuad mq = _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&r2r, q);
    Require(AlmostEqual(mq.m_p1.m_x, 50.0f));
    Require(AlmostEqual(mq.m_p2.m_x, 70.0f));

    WebCore::FloatRect mr = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&r2r, srcR);
    Require(AlmostEqual(mr.m_location.m_x, 50.0f));
    Require(AlmostEqual(mr.m_size.m_width, 200.0f));

    WebCore::TransformationMatrix fromAt1;
    _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(&fromAt1, at);
    Require(AlmostEqual(fromAt1.m_matrix[0][0], at.m_transform[0]));

    WebCore::TransformationMatrix fromAt2;
    _ZN7WebCore20TransformationMatrixC2ERKNS_15AffineTransformE(&fromAt2, at);
    Require(AlmostEqual(fromAt2.m_matrix[0][0], at.m_transform[0]));
}

static WebCore::TransformationMatrix ReferencePerspective(const WebCore::TransformationMatrix& input, double perspective) {
    const std::array<double, 16> transform{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, perspective != 0.0 ? -1.0 / perspective : 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    WebCore::TransformationMatrix result;
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned column = 0; column < 4; column += 2) {
            auto value = _mm_mul_pd(_mm_loadu_pd(&input.m_matrix[0][column]), _mm_set1_pd(transform[row * 4]));
            for (unsigned term = 1; term < 4; ++term) {
                value = _mm_add_pd(value, _mm_mul_pd(_mm_loadu_pd(&input.m_matrix[term][column]),
                                                  _mm_set1_pd(transform[row * 4 + term])));
            }
            _mm_storeu_pd(&result.m_matrix[row][column], value);
        }
    }
    return result;
}

static void RequirePerspectiveResult(const WebCore::TransformationMatrix& actual, const WebCore::TransformationMatrix& expected) {
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            const double value = actual.m_matrix[row][column];
            const double reference = expected.m_matrix[row][column];
            if (std::isnan(reference)) {
                Require(std::isnan(value));
            } else if (reference == 0.0 || std::isinf(reference)) {
                Require(std::bit_cast<std::uint64_t>(value) == std::bit_cast<std::uint64_t>(reference));
            } else {
                Require(std::isfinite(value));
                Require(std::abs(value - reference) <= 8.0 * std::numeric_limits<double>::epsilon()
                    * std::max(std::abs(value), std::abs(reference)));
            }
        }
    }
}

static void TestApplyPerspective() {
    static_assert(sizeof(WebCore::TransformationMatrix) == 128);
    static_assert(alignof(WebCore::TransformationMatrix) == 16);
    struct GuardedMatrix {
        std::array<std::uint64_t, 2> before;
        WebCore::TransformationMatrix value;
        std::array<std::uint64_t, 2> after;
    };
    static_assert(offsetof(GuardedMatrix, value) == 16);
    static_assert(offsetof(GuardedMatrix, after) == 144);
    const std::array<std::uint64_t, 2> canary{0x123456789abcdef0ULL, 0xfedcba9876543210ULL};
    std::array<WebCore::TransformationMatrix, 7> inputs;
    inputs[1].m_matrix[3][0] = 5.0;
    inputs[1].m_matrix[3][1] = -7.0;
    inputs[1].m_matrix[3][2] = 12.0;
    inputs[2].m_matrix[0][0] = 2.0;
    inputs[2].m_matrix[1][1] = 3.0;
    inputs[2].m_matrix[2][2] = -4.0;
    inputs[3].m_matrix[2][3] = -0.125;
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            const double magnitude = 1.0 + row * 4 + column;
            inputs[4].m_matrix[row][column] = column % 2 ? -magnitude : magnitude;
            inputs[5].m_matrix[row][column] = -0.0;
        }
    }
    inputs[6].m_matrix[0][0] = std::numeric_limits<double>::infinity();
    inputs[6].m_matrix[1][1] = std::numeric_limits<double>::quiet_NaN();
    inputs[6].m_matrix[3][2] = -std::numeric_limits<double>::infinity();
    const std::array<double, 12> perspectives{
        4.0, -8.0, 0.0, -0.0,
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::min(), -std::numeric_limits<double>::min(),
        std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min(), 3.25
    };
    auto* applyPerspective = &_ZN7WebCore20TransformationMatrix16applyPerspectiveEd;
    for (const auto& input : inputs) {
        const auto original = std::bit_cast<std::array<std::uint64_t, 16>>(input);
        for (double perspective : perspectives) {
            GuardedMatrix guarded{canary, input, canary};
            const auto expected = ReferencePerspective(input, perspective);
            Require(&applyPerspective(&guarded.value, perspective) == &guarded.value);
            RequirePerspectiveResult(guarded.value, expected);
            Require(guarded.before == canary && guarded.after == canary);
        }
        GuardedMatrix guarded{canary, input, canary};
        auto expected = input;
        for (double perspective : {4.0, -8.0, 16.0, 0.0}) {
            expected = ReferencePerspective(expected, perspective);
            Require(&applyPerspective(&guarded.value, perspective) == &guarded.value);
            RequirePerspectiveResult(guarded.value, expected);
            Require(guarded.before == canary && guarded.after == canary);
        }
        Require((std::bit_cast<std::array<std::uint64_t, 16>>(input) == original));
    }
    WebCore::TransformationMatrix identity;
    applyPerspective(&identity, 4.0);
    Require(identity.m_matrix[2][3] == -0.25);
    auto translated = inputs[1];
    applyPerspective(&translated, 4.0);
    Require(translated.m_matrix[2][0] == -1.25);
    Require(translated.m_matrix[2][1] == 1.75);
    Require(translated.m_matrix[2][2] == -2.0);
}

static void TestMatrixTransformOperation() {
    WebCore::MatrixTransformOperation op{};
    op.m_a = 2.0;
    op.m_b = 0.5;
    op.m_c = -0.5;
    op.m_d = 3.0;
    op.m_e = 100.0;
    op.m_f = 200.0;

    WebCore::TransformationMatrix tm = _ZNK7WebCore24MatrixTransformOperation6matrixEv(&op);
    Require(AlmostEqual(tm.m_matrix[0][0], 2.0));
    Require(AlmostEqual(tm.m_matrix[0][1], 0.5));
    Require(AlmostEqual(tm.m_matrix[1][0], -0.5));
    Require(AlmostEqual(tm.m_matrix[1][1], 3.0));
    Require(AlmostEqual(tm.m_matrix[3][0], 100.0));
    Require(AlmostEqual(tm.m_matrix[3][1], 200.0));
    Require(AlmostEqual(tm.m_matrix[2][2], 1.0));
    Require(AlmostEqual(tm.m_matrix[3][3], 1.0));
}

static void TestIntRectScaleOverflowGivesIntMin() {
    WebCore::IntRect ir{WebCore::IntPoint{1000000000, -1000000000}, WebCore::IntSize{1000000000, 3}};
    _ZN7WebCore7IntRect5scaleEf(&ir, 10.0f);
    Require(ir.m_location.m_x == std::numeric_limits<int>::min());
    Require(ir.m_location.m_y == std::numeric_limits<int>::min());
    Require(ir.m_size.m_width == std::numeric_limits<int>::min());
    Require(ir.m_size.m_height == 30);
}

int main() {
    TestFloatPoint();
    TestFloatSize();
    TestFloatSizeFromIntSize();
    TestFloatRect();
    TestFloatQuad();
    TestFloatRoundedRect();
    TestIntPointIntSizeIntRect();
    TestLayoutRect();
    TestRoundedRect();
    TestColor();
    TestAffineTransform();
    TestTransformationMatrix();
    TestTransformationMatrixScale3dAndAxisRotation();
    TestTransformationMatrixInverse();
    TestTransformationMatrixBlend();
    TestApplyPerspective();
    TestMatrixTransformOperation();
    TestIntRectScaleOverflowGivesIntMin();
    return 0;
}
