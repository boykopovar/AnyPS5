#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore viewport check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool AlmostEqual(float a, float b, float eps = 1e-4f) {
    return std::abs(a - b) <= eps;
}

static WebCore::ViewportAttributes Compute(const WebCore::ViewportArguments& args, int desktopWidth = 980, int deviceWidth = 320,
                                           int deviceHeight = 480, float dpr = 1.0f, WebCore::IntSize visible = {320, 480}) {
    return _ZN7WebCore25computeViewportAttributesENS_17ViewportArgumentsEiiifNS_7IntSizeE(args, desktopWidth, deviceWidth, deviceHeight, dpr, visible);
}

static WebCore::ViewportArguments Args(WebCore::ViewportArguments::Type type) {
    WebCore::ViewportArguments args;
    args.type = type;
    return args;
}

static void TestImplicit() {
    WebCore::ViewportAttributes r = Compute(Args(WebCore::ViewportArguments::Implicit));
    Require(AlmostEqual(r.minimumScale, 0.25f));
    Require(AlmostEqual(r.maximumScale, 5.0f));
    Require(AlmostEqual(r.initialScale, 320.0f / 980.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 980.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 1470.0f));
    Require(r.userScalable == -1.0f);
    Require(r.orientation == -1.0f);
    Require(r.shrinkToFit == -1.0f);
    Require(r.viewportFit == WebCore::ViewportFit::Auto);

    WebCore::ViewportArguments wide = Args(WebCore::ViewportArguments::Implicit);
    wide.width = 20000.0f;
    r = Compute(wide);
    Require(AlmostEqual(r.initialScale, 0.25f));
    Require(AlmostEqual(r.layoutSize.m_width, 20000.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 30000.0f));
}

static void TestViewportMeta() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = WebCore::ViewportArguments::ValueDeviceWidth;
    WebCore::ViewportAttributes r = Compute(args);
    Require(AlmostEqual(r.initialScale, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = WebCore::ViewportArguments::ValueDeviceHeight;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 320.0f / 480.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 480.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 720.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = 20000.0f;
    args.minZoom = 0.01f;
    args.zoom = 0.01f;
    args.maxZoom = 50.0f;
    r = Compute(args);
    Require(AlmostEqual(r.minimumScale, 0.1f));
    Require(AlmostEqual(r.maximumScale, 10.0f));
    Require(AlmostEqual(r.initialScale, 0.1f));
    Require(AlmostEqual(r.layoutSize.m_width, 10000.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 15000.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.height = 960.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 0.5f));
    Require(AlmostEqual(r.layoutSize.m_width, 980.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 960.0f));

    args.zoom = 1.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 640.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 960.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.zoom = 2.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 2.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 160.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 240.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = 100.0f;
    args.zoom = 1.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));
}

static void TestCssDeviceAdaptation() {
    WebCore::ViewportAttributes r = Compute(Args(WebCore::ViewportArguments::CSSDeviceAdaptation));
    Require(AlmostEqual(r.initialScale, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));

    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minWidth = WebCore::ViewportArguments::ValueDeviceHeight;
    args.maxWidth = 1000.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 320.0f / 480.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 480.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 720.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minZoom = 3.0f;
    args.maxZoom = 2.0f;
    args.zoom = 1.0f;
    r = Compute(args);
    Require(AlmostEqual(r.minimumScale, 3.0f));
    Require(AlmostEqual(r.maximumScale, 3.0f));
    Require(AlmostEqual(r.initialScale, 3.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f / 3.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 160.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minHeight = WebCore::ViewportArguments::ValueDeviceWidth;
    args.maxHeight = 100.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_height, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
}

static void TestScaleAndBoundaryBranches() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.minZoom = 3.0f;
    args.maxZoom = 2.0f;
    WebCore::ViewportAttributes r = Compute(args);
    Require(AlmostEqual(r.minimumScale, 3.0f));
    Require(AlmostEqual(r.maximumScale, 3.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.minZoom = 8.0f;
    r = Compute(args);
    Require(AlmostEqual(r.minimumScale, 5.0f));
    Require(AlmostEqual(r.maximumScale, 5.0f));

    args = Args(WebCore::ViewportArguments::Implicit);
    args.width = 10.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 5.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 10.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 15.0f));
}

static void TestCssUnclampedAndDerived() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.width = 20000.0f;
    WebCore::ViewportAttributes r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 20000.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 30000.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.zoom = 1.0f;
    args.height = 960.0f;
    r = Compute(args);
    Require(AlmostEqual(r.initialScale, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 640.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 960.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.maxZoom = 0.5f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 640.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 960.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minWidth = 0.5f;
    args.maxWidth = 0.5f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 1.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minHeight = 0.5f;
    args.maxHeight = 0.5f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 1.0f));
}

static void TestZoomAndDeviceKeywordResolution() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::Implicit);
    args.zoom = 2.0f;
    WebCore::ViewportAttributes r = Compute(args);
    Require(AlmostEqual(r.initialScale, 2.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 160.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 240.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minZoom = 2.0f;
    r = Compute(args);
    Require(AlmostEqual(r.minimumScale, 2.0f));
    Require(AlmostEqual(r.maximumScale, 5.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.width = 100.0f;
    args.zoom = 2.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 160.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 240.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.maxWidth = WebCore::ViewportArguments::ValueDeviceWidth;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.maxHeight = WebCore::ViewportArguments::ValueDeviceHeight;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));

    args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.height = 20000.0f;
    args.zoom = 1.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 10000.0f * 320.0f / 480.0f, 1e-2f));
    Require(AlmostEqual(r.layoutSize.m_height, 10000.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minWidth = 500.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 500.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 750.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.maxWidth = 200.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 200.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 300.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minHeight = 700.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 700.0f));

    args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.maxHeight = 100.0f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 100.0f));
}

static void TestDeviceKeywordTruncatesLikeInt() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = -2.9f;
    WebCore::ViewportAttributes r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));

    args.width = -3.5f;
    r = Compute(args);
    Require(AlmostEqual(r.layoutSize.m_width, 480.0f));
}

static void TestNonFiniteAndHugeWidthDoNotResolveKeyword() {
    const float inf = std::numeric_limits<float>::infinity();
    const float values[] = {inf, -inf, 3.0e9f, -3.0e9f, 2147483648.0f, -2147483904.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
    for (float value : values) {
        WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::Implicit);
        args.width = value;
        WebCore::ViewportAttributes r = Compute(args);
        Require(r.layoutSize.m_width == value);
        Require(AlmostEqual(r.initialScale, 0.25f));

        args = Args(WebCore::ViewportArguments::Implicit);
        args.height = value;
        r = Compute(args);
        Require(r.layoutSize.m_height == value);
    }

    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::CSSDeviceAdaptation);
    args.minWidth = inf;
    args.maxHeight = -inf;
    WebCore::ViewportAttributes r = Compute(args);
    Require(r.layoutSize.m_width == inf);
    Require(r.layoutSize.m_height == 1.0f);
}

static void TestNanWidthDoesNotResolveKeyword() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::Implicit);
    args.width = nan;
    args.height = nan;
    WebCore::ViewportAttributes r = Compute(args);
    Require(std::isnan(r.layoutSize.m_width));
    Require(std::isnan(r.layoutSize.m_height));
    Require(AlmostEqual(r.initialScale, 0.25f));

}

static void TestLargestIntRangeValuesAreNotKeywords() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::Implicit);
    args.width = -2147483648.0f;
    WebCore::ViewportAttributes r = Compute(args);
    Require(r.layoutSize.m_width == -2147483648.0f);

    args.width = 2147483520.0f;
    r = Compute(args);
    Require(r.layoutSize.m_width == 2147483520.0f);
}

static void TestDevicePixelRatio() {
    WebCore::ViewportAttributes r = Compute(Args(WebCore::ViewportArguments::Implicit), 980, 640, 960, 2.0f, {640, 960});
    Require(AlmostEqual(r.initialScale, 320.0f / 980.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 980.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 1470.0f));

    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.width = WebCore::ViewportArguments::ValueDeviceWidth;
    r = Compute(args, 980, 640, 960, 2.0f, {640, 960});
    Require(AlmostEqual(r.initialScale, 1.0f));
    Require(AlmostEqual(r.layoutSize.m_width, 320.0f));
    Require(AlmostEqual(r.layoutSize.m_height, 480.0f));
}

static void TestPassThrough() {
    WebCore::ViewportArguments args = Args(WebCore::ViewportArguments::ViewportMeta);
    args.userZoom = 0.0f;
    args.orientation = 2.0f;
    args.shrinkToFit = 3.0f;
    args.viewportFit = WebCore::ViewportFit::Cover;
    WebCore::ViewportAttributes r = Compute(args);
    Require(r.userScalable == 0.0f);
    Require(r.orientation == 2.0f);
    Require(r.shrinkToFit == 3.0f);
    Require(r.viewportFit == WebCore::ViewportFit::Cover);
}

static void TestRestrictMinimumScale() {
    WebCore::ViewportAttributes r{};
    r.minimumScale = 0.25f;
    r.layoutSize = {1000.0f, 1000.0f};
    _ZN7WebCore40restrictMinimumScaleFactorToViewportSizeERNS_18ViewportAttributesENS_7IntSizeEf(r, {500, 300}, 1.0f);
    Require(AlmostEqual(r.minimumScale, 0.5f));

    r.minimumScale = 0.25f;
    _ZN7WebCore40restrictMinimumScaleFactorToViewportSizeERNS_18ViewportAttributesENS_7IntSizeEf(r, {500, 300}, 2.0f);
    Require(AlmostEqual(r.minimumScale, 0.25f));

    r.minimumScale = 0.25f;
    r.layoutSize = {4000.0f, 4000.0f};
    _ZN7WebCore40restrictMinimumScaleFactorToViewportSizeERNS_18ViewportAttributesENS_7IntSizeEf(r, {500, 300}, 1.0f);
    Require(AlmostEqual(r.minimumScale, 0.25f));

    r.layoutSize = {1000.0f, 100.0f};
    _ZN7WebCore40restrictMinimumScaleFactorToViewportSizeERNS_18ViewportAttributesENS_7IntSizeEf(r, {500, 300}, 1.0f);
    Require(AlmostEqual(r.minimumScale, 3.0f));
}

static void TestContentContained() {
    WebCore::ViewportAttributes r{};
    r.minimumScale = 0.4f;
    WebCore::IntSize viewport{400, 300};
    Require(AlmostEqual(_ZN7WebCore44computeMinimumScaleFactorForContentContainedERKNS_18ViewportAttributesERKNS_7IntSizeES5_(r, viewport, WebCore::IntSize{800, 1200}), 0.5f));
    Require(AlmostEqual(_ZN7WebCore44computeMinimumScaleFactorForContentContainedERKNS_18ViewportAttributesERKNS_7IntSizeES5_(r, viewport, WebCore::IntSize{1600, 300}), 1.0f));
    Require(AlmostEqual(_ZN7WebCore44computeMinimumScaleFactorForContentContainedERKNS_18ViewportAttributesERKNS_7IntSizeES5_(r, viewport, WebCore::IntSize{1600, 1200}), 0.4f));
}

static void TestRestrictToInitialScale() {
    WebCore::ViewportAttributes r{};
    r.initialScale = 1.5f;
    r.minimumScale = 0.25f;
    r.maximumScale = 5.0f;
    r.userScalable = 0.0f;
    _ZN7WebCore50restrictScaleFactorToInitialScaleIfNotUserScalableERNS_18ViewportAttributesE(r);
    Require(r.minimumScale == 1.5f);
    Require(r.maximumScale == 1.5f);

    r.minimumScale = 0.25f;
    r.maximumScale = 5.0f;
    r.userScalable = 1.0f;
    _ZN7WebCore50restrictScaleFactorToInitialScaleIfNotUserScalableERNS_18ViewportAttributesE(r);
    Require(r.minimumScale == 0.25f);
    Require(r.maximumScale == 5.0f);

    r.userScalable = -1.0f;
    _ZN7WebCore50restrictScaleFactorToInitialScaleIfNotUserScalableERNS_18ViewportAttributesE(r);
    Require(r.minimumScale == 0.25f);
    Require(r.maximumScale == 5.0f);
}

int main() {
    TestImplicit();
    TestViewportMeta();
    TestCssDeviceAdaptation();
    TestScaleAndBoundaryBranches();
    TestCssUnclampedAndDerived();
    TestZoomAndDeviceKeywordResolution();
    TestDeviceKeywordTruncatesLikeInt();
    TestNonFiniteAndHugeWidthDoNotResolveKeyword();
    TestNanWidthDoesNotResolveKeyword();
    TestLargestIntRangeValuesAreNotKeywords();
    TestDevicePixelRatio();
    TestPassThrough();
    TestRestrictMinimumScale();
    TestContentContained();
    TestRestrictToInitialScale();
    return 0;
}
