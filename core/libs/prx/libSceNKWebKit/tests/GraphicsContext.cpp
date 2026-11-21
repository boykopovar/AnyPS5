#include "prx/libSceNKWebKit/include/GraphicsContextExports.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::GraphicsContextAbi::Context;
using NKWebKit::GraphicsContextAbi::PlatformContext;
using NKWebKit::GraphicsContextAbi::State;

void Require(const bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GraphicsContext check failed at line %u\n", location.line());
        std::abort();
    }
}

constexpr WebCore::Color Red{0xFF0000FFULL};
constexpr WebCore::Color Green{0x00FF00FFULL};
constexpr WebCore::Color Blue{0x0000FFFFULL};

struct TestPath {
    void* m_path;
    unsigned char rest[24] = {};
};

struct Surface {
    CairoSurface* surface;
    CairoContext* context;
    PlatformContext platform;

    Surface() {
        surface = cairo_image_surface_create(0, 8, 8);
        context = cairo_create(surface);
        platform.cr = context;
    }

    ~Surface() {
        cairo_destroy(context);
        cairo_surface_destroy(surface);
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    std::uint32_t Pixel(const int x, const int y) const {
        cairo_surface_flush(surface);
        const auto* data = cairo_image_surface_get_data(surface);
        const int stride = cairo_image_surface_get_stride(surface);
        std::uint32_t value = 0;
        std::memcpy(&value, data + y * stride + x * 4, sizeof(value));
        return value;
    }
};

struct OwnedContext {
    alignas(8) unsigned char storage[sizeof(Context)];
    Context* context = reinterpret_cast<Context*>(storage);

    explicit OwnedContext(PlatformContext* platform) {
        _ZN7WebCore15GraphicsContextC1EPNS_20PlatformContextCairoE(context, platform);
    }

    ~OwnedContext() {
        _ZN7WebCore15GraphicsContextD1Ev(context);
    }

    OwnedContext(const OwnedContext&) = delete;
    OwnedContext& operator=(const OwnedContext&) = delete;
};

void FillRect(Context& context, float x, float y, float width, float height) {
    const WebCore::FloatRect rect{{x, y}, {width, height}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectE(&context, &rect);
}

void ClearRect(Context& context, float x, float y, float width, float height) {
    const WebCore::FloatRect rect{{x, y}, {width, height}};
    _ZN7WebCore15GraphicsContext9clearRectERKNS_9FloatRectE(&context, &rect);
}

void TestState() {
    alignas(8) unsigned char storage[sizeof(State)] = {};
    auto* state = reinterpret_cast<State*>(storage);
    _ZN7WebCore20GraphicsContextStateC1Ev(state);
    Require(state->strokeColor.m_colorAndFlags == 0x000000FFULL);
    Require(state->fillColor.m_colorAndFlags == 0x000000FFULL);
    Require(state->shadowColor.m_colorAndFlags == 0);
    Require(state->alpha == 1.0f);
    Require(state->strokeThickness == 0.0f);
    Require(state->textDrawingMode == 1);
    Require(state->strokeStyle == 1);
    Require(state->fillRule == 0);
    Require(state->compositeOperator == 2);
    Require(state->blendMode == 1);
    Require((state->flags & 1U) != 0);
    Require((state->flags & 2U) != 0);
    Require((state->flags & 4U) != 0);
    _ZN7WebCore20GraphicsContextStateD1Ev(state);
}

void TestFillAndClear() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::Color red = Red;
    _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(&context, &red);
    FillRect(context, 0, 0, 4, 4);
    Require(surface.Pixel(1, 1) == 0xFFFF0000U);
    Require(surface.Pixel(5, 5) == 0);
    const WebCore::Color blue = Blue;
    const WebCore::FloatRect rect{{4, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &rect, &blue);
    Require(surface.Pixel(5, 1) == 0xFF0000FFU);
    ClearRect(context, 0, 0, 4, 4);
    Require(surface.Pixel(1, 1) == 0);
    Require(surface.Pixel(5, 1) == 0xFF0000FFU);
}

void TestSaveRestoreAndTransforms() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::Color red = Red;
    const WebCore::Color green = Green;
    _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(&context, &red);
    _ZN7WebCore15GraphicsContext4saveEv(&context);
    _ZN7WebCore15GraphicsContext9translateEff(&context, 4.0f, 0.0f);
    _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(&context, &green);
    FillRect(context, 0, 0, 4, 4);
    _ZN7WebCore15GraphicsContext7restoreEv(&context);
    FillRect(context, 0, 4, 4, 4);
    Require(surface.Pixel(5, 1) == 0xFF00FF00U);
    Require(surface.Pixel(1, 5) == 0xFFFF0000U);

    _ZN7WebCore15GraphicsContext4saveEv(&context);
    const WebCore::FloatSize scale{2.0f, 2.0f};
    _ZN7WebCore15GraphicsContext5scaleERKNS_9FloatSizeE(&context, &scale);
    const WebCore::FloatRect unit{{4, 4}, {1, 1}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &unit, &red);
    Require(surface.Pixel(7, 7) == 0);
    _ZN7WebCore15GraphicsContext7restoreEv(&context);

    _ZN7WebCore15GraphicsContext9translateEff(&context, 1.0f, 2.0f);
    const WebCore::FloatSize half{0.5f, 0.5f};
    _ZN7WebCore15GraphicsContext5scaleERKNS_9FloatSizeE(&context, &half);
    const auto ctm = _ZNK7WebCore15GraphicsContext6getCTMENS0_18IncludeDeviceScaleE(&context, 0);
    Require(ctm.m_transform[0] == 0.5 && ctm.m_transform[3] == 0.5);
    Require(ctm.m_transform[4] == 1.0 && ctm.m_transform[5] == 2.0);
}

void TestClip() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::FloatRect clip{{0, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext4clipERKNS_9FloatRectE(&context, &clip);
    const WebCore::Color green = Green;
    const WebCore::FloatRect full{{0, 0}, {8, 8}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &green);
    Require(surface.Pixel(1, 1) == 0xFF00FF00U);
    Require(surface.Pixel(5, 5) == 0);
}

void TestPaths() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;

    CairoSurface* pathSurface = cairo_image_surface_create(0, 1, 1);
    CairoContext* pathContext = cairo_create(pathSurface);
    cairo_move_to(pathContext, 1.0, 1.0);
    cairo_line_to(pathContext, 6.0, 1.0);
    cairo_line_to(pathContext, 6.0, 6.0);
    cairo_line_to(pathContext, 1.0, 6.0);
    cairo_close_path(pathContext);
    TestPath path{pathContext};
    const WebCore::Color blue = Blue;
    _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(&context, &blue);
    _ZN7WebCore15GraphicsContext8fillPathERKNS_4PathE(&context, &path);
    Require(surface.Pixel(3, 3) == 0xFF0000FFU);

    cairo_new_path(pathContext);
    cairo_move_to(pathContext, 1.0, 1.0);
    cairo_line_to(pathContext, 6.0, 1.0);
    const WebCore::Color red = Red;
    _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(&context, &red);
    _ZN7WebCore15GraphicsContext18setStrokeThicknessEf(&context, 2.0f);
    _ZN7WebCore15GraphicsContext10strokePathERKNS_4PathE(&context, &path);
    Require(surface.Pixel(3, 1) != 0);

    cairo_destroy(pathContext);
    cairo_surface_destroy(pathSurface);
}

void TestPlatformContext() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    Require(_ZNK7WebCore15GraphicsContext18hasPlatformContextEv(owned.context));
    Require(_ZNK7WebCore15GraphicsContext15platformContextEv(owned.context) == &surface.platform);
    Require(surface.Pixel(0, 0) == 0);

    OwnedContext nullContext(nullptr);
    Require(!_ZNK7WebCore15GraphicsContext18hasPlatformContextEv(nullContext.context));
    Require(_ZNK7WebCore15GraphicsContext15platformContextEv(nullContext.context) == nullptr);
    const WebCore::FloatRect rect{{0, 0}, {1, 1}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectE(nullContext.context, &rect);
}

void TestStrokeRectAndImage() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::Color red = Red;
    _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(&context, &red);
    const WebCore::FloatRect rect{{1, 1}, {6, 6}};
    _ZN7WebCore15GraphicsContext10strokeRectERKNS_9FloatRectEf(&context, &rect, 2.0f);
    Require(surface.Pixel(3, 1) != 0);
    Require(surface.Pixel(3, 3) == 0);

    CairoSurface* imageSurface = cairo_image_surface_create(0, 2, 2);
    CairoContext* imageContext = cairo_create(imageSurface);
    cairo_set_source_rgb(imageContext, 1.0, 0.0, 0.0);
    cairo_paint(imageContext);
    cairo_destroy(imageContext);
    cairo_surface_flush(imageSurface);

    const WebCore::FloatSize imageSize{2.0f, 2.0f};
    const WebCore::FloatRect destination{{4, 4}, {4, 4}};
    const WebCore::FloatRect source{{0, 0}, {2, 2}};
    const NKWebKit::GraphicsContextAbi::ImagePaintingOptions options{2, 1, 0, 0, 1, 0, {0, 0, 0}};
    _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(&context, &imageSurface, &imageSize, &destination, &source, &options);
    Require(surface.Pixel(5, 5) == 0xFFFF0000U);
    Require(surface.Pixel(7, 7) == 0xFFFF0000U);

    const NKWebKit::GraphicsContextAbi::ImagePaintingOptions multiplied{2, 2, 0, 0, 1, 0, {0, 0, 0}};
    bool threw = false;
    try {
        _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(&context, &imageSurface, &imageSize, &destination, &source, &multiplied);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    const WebCore::Color green = Green;
    const WebCore::FloatRect leftRegion{{0, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &leftRegion, &green);
    const NKWebKit::GraphicsContextAbi::ImagePaintingOptions destinationOver{6, 1, 0, 0, 1, 0, {0, 0, 0}};
    const WebCore::FloatRect leftDestination{{0, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(&context, &imageSurface, &imageSize, &leftDestination, &source, &destinationOver);
    Require(surface.Pixel(1, 1) == 0xFF00FF00U);

    const WebCore::FloatRect rightRegion{{4, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &rightRegion, &green);
    Require(surface.Pixel(5, 1) == 0xFF00FF00U);
    const NKWebKit::GraphicsContextAbi::ImagePaintingOptions xorOptions{10, 1, 0, 0, 1, 0, {0, 0, 0}};
    const WebCore::FloatRect rightDestination{{4, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(&context, &imageSurface, &imageSize, &rightDestination, &source, &xorOptions);
    Require(surface.Pixel(5, 1) == 0);
    cairo_surface_destroy(imageSurface);
}

void TestCompositeAndTransparency() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::Color green = Green;
    const WebCore::Color red = Red;
    const WebCore::FloatRect full{{0, 0}, {8, 8}};
    const WebCore::FloatRect quarter{{0, 0}, {4, 4}};
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &green);
    _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(&context, 6, 1);
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &red);
    Require(surface.Pixel(1, 1) == 0xFF00FF00U);
    _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(&context, 2, 1);
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &red);
    Require(surface.Pixel(1, 1) == 0xFFFF0000U);

    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &green);
    _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(&context, 10, 1);
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &full, &red);
    Require(surface.Pixel(1, 1) == 0);

    bool threw = false;
    try {
        _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(&context, 2, 2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(&context, 2, 1);
    _ZN7WebCore15GraphicsContext9clearRectERKNS_9FloatRectE(&context, &full);
    _ZN7WebCore15GraphicsContext22beginTransparencyLayerEf(&context, 0.5f);
    _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(&context, &quarter, &red);
    _ZN7WebCore15GraphicsContext20endTransparencyLayerEv(&context);
    const std::uint32_t pixel = surface.Pixel(1, 1);
    Require((pixel >> 24) == 0x80);
    Require(((pixel >> 16) & 0xFF) >= 127 && ((pixel >> 16) & 0xFF) <= 129);
    Require(surface.Pixel(6, 6) == 0);
}

void TestDrawLineForText() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::Color red = Red;
    _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(&context, &red);
    const WebCore::FloatRect line{{1, 1}, {6, 2}};
    _ZN7WebCore15GraphicsContext15drawLineForTextERKNS_9FloatRectEbbNS_11StrokeStyleE(&context, &line, false, false, 1);
    Require(surface.Pixel(3, 1) == 0xFFFF0000U);
    Require(surface.Pixel(3, 2) == 0xFFFF0000U);
    Require(surface.Pixel(3, 3) == 0);

    Surface doubled;
    OwnedContext doubledContext(&doubled.platform);
    _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(doubledContext.context, &red);
    _ZN7WebCore15GraphicsContext15drawLineForTextERKNS_9FloatRectEbbNS_11StrokeStyleE(doubledContext.context, &line, true, true, 1);
    Require(doubled.Pixel(3, 1) == 0xFFFF0000U);
    Require(doubled.Pixel(3, 3) == 0);
    Require(doubled.Pixel(3, 5) == 0xFFFF0000U);
    Require(doubled.Pixel(3, 7) == 0);
}

void TestShadowState() {
    Surface surface;
    OwnedContext owned(&surface.platform);
    auto& context = *owned.context;
    const WebCore::FloatSize offset{2.0f, 3.0f};
    const WebCore::Color blue = Blue;
    _ZN7WebCore15GraphicsContext9setShadowERKNS_9FloatSizeEfRKNS_5ColorE(&context, &offset, 4.0f, &blue);
    Require(context.state.shadowOffset.m_width == 2.0f);
    Require(context.state.shadowOffset.m_height == 3.0f);
    Require(context.state.shadowBlur == 4.0f);
    Require(context.state.shadowColor.m_colorAndFlags == Blue.m_colorAndFlags);
    _ZN7WebCore15GraphicsContext11clearShadowEv(&context);
    Require(context.state.shadowBlur == 0.0f);
    Require(context.state.shadowColor.m_colorAndFlags == 0);
}

}

int main() {
    try {
        TestState();
        TestFillAndClear();
        TestSaveRestoreAndTransforms();
        TestClip();
        TestPaths();
        TestPlatformContext();
        TestStrokeRectAndImage();
        TestCompositeAndTransparency();
        TestDrawLineForText();
        TestShadowState();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GraphicsContext test failed: %s\n", error.what());
        return 1;
    }
    std::puts("GraphicsContext tests passed.");
    return 0;
}
