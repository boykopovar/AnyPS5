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

static void TestDefaultFilterOperation() {
    auto* op1Raw = static_cast<WebCore::DefaultFilterOperation*>(std::malloc(sizeof(WebCore::DefaultFilterOperation)));
    op1Raw->m_vptr = nullptr;
    op1Raw->m_refCount = 1;
    op1Raw->m_type = WebCore::FilterOperationType::Default;
    op1Raw->m_representedType = WebCore::FilterOperationType::Blur;
    auto op1 = WTF::adoptRef(*op1Raw);
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(op1.ptr()) == WebCore::FilterOperationType::Blur);
    Require(_ZNK7WebCore22DefaultFilterOperation15representedTypeEv(op1.ptr()) != WebCore::FilterOperationType::DropShadow);

    auto* op2Raw = static_cast<WebCore::DefaultFilterOperation*>(std::malloc(sizeof(WebCore::DefaultFilterOperation)));
    op2Raw->m_vptr = nullptr;
    op2Raw->m_refCount = 1;
    op2Raw->m_type = WebCore::FilterOperationType::Default;
    op2Raw->m_representedType = WebCore::FilterOperationType::DropShadow;
    auto op2 = WTF::adoptRef(*op2Raw);
    Require(!_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(op1.ptr(), *op2.ptr()));

    auto* op3Raw = static_cast<WebCore::DefaultFilterOperation*>(std::malloc(sizeof(WebCore::DefaultFilterOperation)));
    op3Raw->m_vptr = nullptr;
    op3Raw->m_refCount = 1;
    op3Raw->m_type = WebCore::FilterOperationType::Default;
    op3Raw->m_representedType = WebCore::FilterOperationType::Blur;
    auto op3 = WTF::adoptRef(*op3Raw);
    Require(_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(op1.ptr(), *op3.ptr()));

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore22DefaultFilterOperation5cloneEv(op1.ptr());
    Require(cloned.ptr() != nullptr);
    Require(_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(op1.ptr(), *cloned.ptr()));

    WebCore::BlurFilterOperation diffTypeOp;
    diffTypeOp.m_vptr = nullptr;
    diffTypeOp.m_refCount = 1;
    diffTypeOp.m_type = WebCore::FilterOperationType::Blur;
    Require(!_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(op1.ptr(), diffTypeOp));

    alignas(alignof(WebCore::DefaultFilterOperation)) char storage[sizeof(WebCore::DefaultFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::DefaultFilterOperation*>(storage);
    placed->m_representedType = WebCore::FilterOperationType::Sepia;

}

static void TestBlurFilterOperation() {
    WebCore::Length len{12, WebCore::LengthType::Fixed};
    WTF::Ref<WebCore::BlurFilterOperation> blur = _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(len);
    Require(blur.ptr() != nullptr);

    const WebCore::Length& dev = _ZNK7WebCore19BlurFilterOperation12stdDeviationEv(blur.ptr());
    Require(dev.type() == WebCore::LengthType::Fixed);
    Require(dev.value() == 12);

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore19BlurFilterOperation5cloneEv(blur.ptr());
    Require(_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(blur.ptr(), *cloned.ptr()));

    WebCore::Length diffLen{20, WebCore::LengthType::Fixed};
    auto diffBlur = _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(diffLen);
    Require(!_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(blur.ptr(), *diffBlur.ptr()));

    WebCore::DefaultFilterOperation defaultOp;
    defaultOp.m_vptr = nullptr;
    defaultOp.m_refCount = 1;
    defaultOp.m_type = WebCore::FilterOperationType::Default;
    Require(!_ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(blur.ptr(), defaultOp));

    alignas(alignof(WebCore::BlurFilterOperation)) char storage[sizeof(WebCore::BlurFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::BlurFilterOperation*>(storage);
    _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(placed, len);
    Require(_ZNK7WebCore19BlurFilterOperation12stdDeviationEv(placed).value() == 12);
    _ZN7WebCore19BlurFilterOperationD1Ev(placed);

    _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(placed, diffLen);
    Require(_ZNK7WebCore19BlurFilterOperation12stdDeviationEv(placed).value() == 20);
    _ZN7WebCore19BlurFilterOperationD2Ev(placed);

    auto* heapOp = static_cast<WebCore::BlurFilterOperation*>(std::malloc(sizeof(WebCore::BlurFilterOperation)));
    _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(heapOp, len);
    _ZN7WebCore19BlurFilterOperationD0Ev(heapOp);
}

static void TestDropShadowFilterOperation() {
    WebCore::IntPoint pt{15, 25};
    WebCore::Color color{0xFF11223344556677ULL};
    WTF::Ref<WebCore::DropShadowFilterOperation> ds = _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(pt, 8, color);
    Require(ds.ptr() != nullptr);
    Require(_ZNK7WebCore25DropShadowFilterOperation1xEv(ds.ptr()) == 15);
    Require(_ZNK7WebCore25DropShadowFilterOperation1yEv(ds.ptr()) == 25);
    Require(_ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(ds.ptr()) == 8);

    WebCore::IntPoint loc = _ZNK7WebCore25DropShadowFilterOperation8locationEv(ds.ptr());
    Require(loc.m_x == 15 && loc.m_y == 25);

    const WebCore::Color& c = _ZNK7WebCore25DropShadowFilterOperation5colorEv(ds.ptr());
    Require(c.m_colorAndFlags == 0xFF11223344556677ULL);

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore25DropShadowFilterOperation5cloneEv(ds.ptr());
    Require(_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(ds.ptr(), *cloned.ptr()));

    WebCore::Color color2{0xFF99999999999999ULL};
    auto ds2 = _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(pt, 8, color2);
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(ds.ptr(), *ds2.ptr()));

    WebCore::IntPoint pt2{99, 99};
    auto ds3 = _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(pt2, 8, color);
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(ds.ptr(), *ds3.ptr()));

    auto ds4 = _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(pt, 1, color);
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(ds.ptr(), *ds4.ptr()));

    WebCore::DefaultFilterOperation defaultOp;
    defaultOp.m_vptr = nullptr;
    defaultOp.m_refCount = 1;
    defaultOp.m_type = WebCore::FilterOperationType::Default;
    Require(!_ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(ds.ptr(), defaultOp));

    alignas(alignof(WebCore::DropShadowFilterOperation)) char storage[sizeof(WebCore::DropShadowFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::DropShadowFilterOperation*>(storage);
    _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(placed, pt, 8, color);
    Require(_ZNK7WebCore25DropShadowFilterOperation1xEv(placed) == 15);
    _ZN7WebCore25DropShadowFilterOperationD1Ev(placed);

    _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(placed, pt, 4, color2);
    Require(_ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(placed) == 4);
    _ZN7WebCore25DropShadowFilterOperationD2Ev(placed);

    auto* heapOp = static_cast<WebCore::DropShadowFilterOperation*>(std::malloc(sizeof(WebCore::DropShadowFilterOperation)));
    _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(heapOp, pt, 8, color);
    _ZN7WebCore25DropShadowFilterOperationD0Ev(heapOp);
}

static void TestInvertLightnessFilterOperation() {
    WTF::Ref<WebCore::InvertLightnessFilterOperation> inv = _ZN7WebCore30InvertLightnessFilterOperation6createEv();
    Require(inv.ptr() != nullptr);
    Require(inv->type() == WebCore::FilterOperationType::AppleInvertLightness);

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(inv.ptr());
    Require(_ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(inv.ptr(), *cloned.ptr()));

    WebCore::DefaultFilterOperation defaultOp;
    defaultOp.m_vptr = nullptr;
    defaultOp.m_refCount = 1;
    defaultOp.m_type = WebCore::FilterOperationType::Default;
    Require(!_ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(inv.ptr(), defaultOp));

    alignas(alignof(WebCore::InvertLightnessFilterOperation)) char storage[sizeof(WebCore::InvertLightnessFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::InvertLightnessFilterOperation*>(storage);
    _ZN7WebCore30InvertLightnessFilterOperationC1Ev(placed);
    Require(placed->type() == WebCore::FilterOperationType::AppleInvertLightness);

    _ZN7WebCore30InvertLightnessFilterOperationC2Ev(placed);

    auto* heapOp = static_cast<WebCore::InvertLightnessFilterOperation*>(std::malloc(sizeof(WebCore::InvertLightnessFilterOperation)));
    _ZN7WebCore30InvertLightnessFilterOperationC1Ev(heapOp);
    std::free(heapOp);
}

static void TestBasicColorMatrixFilterOperation() {
    auto* grayRaw = static_cast<WebCore::BasicColorMatrixFilterOperation*>(std::malloc(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    grayRaw->m_vptr = nullptr;
    grayRaw->m_refCount = 1;
    grayRaw->m_type = WebCore::FilterOperationType::Grayscale;
    grayRaw->m_amount = 0.65;
    auto gray = WTF::adoptRef(*grayRaw);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv(gray.ptr()) == 0.65);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(gray.ptr()) == 0.0);

    auto* sepiaRaw = static_cast<WebCore::BasicColorMatrixFilterOperation*>(std::malloc(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    sepiaRaw->m_vptr = nullptr;
    sepiaRaw->m_refCount = 1;
    sepiaRaw->m_type = WebCore::FilterOperationType::Sepia;
    sepiaRaw->m_amount = 0.8;
    auto sepia = WTF::adoptRef(*sepiaRaw);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(sepia.ptr()) == 0.0);

    auto* satRaw = static_cast<WebCore::BasicColorMatrixFilterOperation*>(std::malloc(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    satRaw->m_vptr = nullptr;
    satRaw->m_refCount = 1;
    satRaw->m_type = WebCore::FilterOperationType::Saturate;
    satRaw->m_amount = 1.5;
    auto sat = WTF::adoptRef(*satRaw);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(sat.ptr()) == 1.0);

    auto* hueRaw = static_cast<WebCore::BasicColorMatrixFilterOperation*>(std::malloc(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    hueRaw->m_vptr = nullptr;
    hueRaw->m_refCount = 1;
    hueRaw->m_type = WebCore::FilterOperationType::HueRotate;
    hueRaw->m_amount = 180.0;
    auto hue = WTF::adoptRef(*hueRaw);
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(hue.ptr()) == 0.0);

    auto* invalidRaw = static_cast<WebCore::BasicColorMatrixFilterOperation*>(std::malloc(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    invalidRaw->m_vptr = nullptr;
    invalidRaw->m_refCount = 1;
    invalidRaw->m_type = WebCore::FilterOperationType::None;
    invalidRaw->m_amount = 0.0;
    auto invalidOp = WTF::adoptRef(*invalidRaw);
    bool threw = false;
    try {
        _ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(invalidOp.ptr());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(gray.ptr());
    Require(_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(gray.ptr(), *cloned.ptr()));
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(gray.ptr(), *sepia.ptr()));

    WebCore::DefaultFilterOperation defaultOp;
    defaultOp.m_vptr = nullptr;
    defaultOp.m_refCount = 1;
    defaultOp.m_type = WebCore::FilterOperationType::Default;
    Require(!_ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(gray.ptr(), defaultOp));

    alignas(alignof(WebCore::BasicColorMatrixFilterOperation)) char storage[sizeof(WebCore::BasicColorMatrixFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::BasicColorMatrixFilterOperation*>(storage);
    placed->m_amount = 0.5;

}

static void TestBasicComponentTransferFilterOperation() {
    auto* invRaw = static_cast<WebCore::BasicComponentTransferFilterOperation*>(std::malloc(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    invRaw->m_vptr = nullptr;
    invRaw->m_refCount = 1;
    invRaw->m_type = WebCore::FilterOperationType::Invert;
    invRaw->m_amount = 0.4;
    auto inv = WTF::adoptRef(*invRaw);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv(inv.ptr()) == 0.4);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(inv.ptr()) == 0.0);
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(inv.ptr()));

    auto* opacRaw = static_cast<WebCore::BasicComponentTransferFilterOperation*>(std::malloc(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    opacRaw->m_vptr = nullptr;
    opacRaw->m_refCount = 1;
    opacRaw->m_type = WebCore::FilterOperationType::Opacity;
    opacRaw->m_amount = 0.7;
    auto opac = WTF::adoptRef(*opacRaw);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(opac.ptr()) == 1.0);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(opac.ptr()));

    auto* brightRaw = static_cast<WebCore::BasicComponentTransferFilterOperation*>(std::malloc(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    brightRaw->m_vptr = nullptr;
    brightRaw->m_refCount = 1;
    brightRaw->m_type = WebCore::FilterOperationType::Brightness;
    brightRaw->m_amount = 1.2;
    auto bright = WTF::adoptRef(*brightRaw);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(bright.ptr()) == 1.0);
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(bright.ptr()));

    auto* contrastRaw = static_cast<WebCore::BasicComponentTransferFilterOperation*>(std::malloc(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    contrastRaw->m_vptr = nullptr;
    contrastRaw->m_refCount = 1;
    contrastRaw->m_type = WebCore::FilterOperationType::Contrast;
    contrastRaw->m_amount = 1.1;
    auto contrast = WTF::adoptRef(*contrastRaw);
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(contrast.ptr()) == 1.0);
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(contrast.ptr()));

    auto* invalidRaw = static_cast<WebCore::BasicComponentTransferFilterOperation*>(std::malloc(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    invalidRaw->m_vptr = nullptr;
    invalidRaw->m_refCount = 1;
    invalidRaw->m_type = WebCore::FilterOperationType::None;
    invalidRaw->m_amount = 0.0;
    auto invalidOp = WTF::adoptRef(*invalidRaw);
    bool threw = false;
    try {
        _ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(invalidOp.ptr());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    WTF::Ref<WebCore::FilterOperation> cloned = _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(inv.ptr());
    Require(_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(inv.ptr(), *cloned.ptr()));
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(inv.ptr(), *opac.ptr()));

    WebCore::DefaultFilterOperation defaultOp;
    defaultOp.m_vptr = nullptr;
    defaultOp.m_refCount = 1;
    defaultOp.m_type = WebCore::FilterOperationType::Default;
    Require(!_ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(inv.ptr(), defaultOp));

    alignas(alignof(WebCore::BasicComponentTransferFilterOperation)) char storage[sizeof(WebCore::BasicComponentTransferFilterOperation)];
    auto* placed = reinterpret_cast<WebCore::BasicComponentTransferFilterOperation*>(storage);
    placed->m_amount = 0.5;

}

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
    TestDefaultFilterOperation();
    TestBlurFilterOperation();
    TestDropShadowFilterOperation();
    TestInvertLightnessFilterOperation();
    TestBasicColorMatrixFilterOperation();
    TestBasicComponentTransferFilterOperation();
    TestRegionBasicsAndShape();
    TestRegionGeometryOps();
    return 0;
}
