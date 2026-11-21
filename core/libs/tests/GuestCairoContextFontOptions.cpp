#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/GlyphDrawing.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <vector>

namespace CairoFontOptionsReference {

std::array<int, 4> Read(void* context);
int Status(void* context);
std::vector<unsigned char> Render(const std::array<int, 4>* values, const std::uint64_t* indices,
    const double* positions, int count, int width, int height);

}

static_assert(std::is_same_v<decltype(cairo_set_font_options(nullptr, nullptr)), void>);

namespace {

using Values = std::array<int, 4>;

constexpr int FormatArgb32 = 0;
constexpr int StatusSuccess = 0;
constexpr int StatusNoMemory = 1;
constexpr int StatusNullPointer = 7;
constexpr int Width = 48;
constexpr int Height = 16;

constexpr std::array<CairoGlyph, 4> glyphs{{{'A', 2, 12}, {'g', 12, 12}, {'#', 22, 12}, {'W', 32, 12}}};

constexpr Values defaults{CAIRO_ANTIALIAS_DEFAULT, CAIRO_SUBPIXEL_ORDER_DEFAULT, CAIRO_HINT_STYLE_DEFAULT,
    CAIRO_HINT_METRICS_DEFAULT};

cairo_font_options_t* Options(const Values& values) {
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>(values[0]));
    cairo_font_options_set_subpixel_order(options, static_cast<cairo_subpixel_order_t>(values[1]));
    cairo_font_options_set_hint_style(options, static_cast<cairo_hint_style_t>(values[2]));
    cairo_font_options_set_hint_metrics(options, static_cast<cairo_hint_metrics_t>(values[3]));
    return options;
}

void Set(CairoContext* context, const Values& values) {
    cairo_font_options_t* options = Options(values);
    cairo_set_font_options(context, options);
    cairo_font_options_destroy(options);
}

Values Read(CairoContext* context) {
    return CairoFontOptionsReference::Read(context);
}

int Status(CairoContext* context) {
    return CairoFontOptionsReference::Status(context);
}

std::vector<unsigned char> Draw(const Values* values) {
    CairoSurface* surface = cairo_image_surface_create(FormatArgb32, Width, Height);
    CairoContext* context = cairo_create(surface);
    if (values) Set(context, *values);
    cairo_show_glyphs(context, glyphs.data(), static_cast<int>(glyphs.size()));
    assert(Status(context) == StatusSuccess);
    cairo_destroy(context);
    cairo_surface_flush(surface);
    const auto size = static_cast<std::size_t>(cairo_image_surface_get_stride(surface)) * Height;
    std::vector<unsigned char> pixels(size);
    std::memcpy(pixels.data(), cairo_image_surface_get_data(surface), size);
    cairo_surface_destroy(surface);
    return pixels;
}

std::vector<unsigned char> Reference(const Values* values) {
    std::array<std::uint64_t, glyphs.size()> indices{};
    std::array<double, glyphs.size() * 2> positions{};
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        indices[i] = glyphs[i].index;
        positions[i * 2] = glyphs[i].x;
        positions[i * 2 + 1] = glyphs[i].y;
    }
    return CairoFontOptionsReference::Render(values, indices.data(), positions.data(),
        static_cast<int>(glyphs.size()), Width, Height);
}

std::array<std::size_t, 3> Alphas(const std::vector<unsigned char>& pixels) {
    std::array<std::size_t, 3> counts{};
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        std::uint32_t pixel = 0;
        std::memcpy(&pixel, pixels.data() + offset, sizeof(pixel));
        const std::uint32_t alpha = pixel >> 24;
        ++counts[alpha == 0 ? 0 : alpha == 0xff ? 2 : 1];
    }
    return counts;
}

void ValuesTest() {
    CairoSurface* surface = cairo_image_surface_create(FormatArgb32, Width, Height);
    CairoContext* context = cairo_create(surface);
    assert(Read(context) == defaults);

    const Values first{CAIRO_ANTIALIAS_NONE, CAIRO_SUBPIXEL_ORDER_BGR, CAIRO_HINT_STYLE_FULL, CAIRO_HINT_METRICS_OFF};
    cairo_font_options_t* options = Options(first);
    cairo_set_font_options(context, options);
    assert(Read(context) == first);
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    assert(Read(context) == first);
    cairo_font_options_destroy(options);
    assert(Read(context) == first);

    const Values second{CAIRO_ANTIALIAS_SUBPIXEL, CAIRO_SUBPIXEL_ORDER_VBGR, CAIRO_HINT_STYLE_SLIGHT,
        CAIRO_HINT_METRICS_ON};
    cairo_save(context);
    Set(context, second);
    assert(Read(context) == second);
    cairo_restore(context);
    assert(Read(context) == first);

    Set(context, defaults);
    assert(Read(context) == defaults);
    assert(Status(context) == StatusSuccess);
    cairo_destroy(context);
    cairo_surface_destroy(surface);
}

void RenderTest() {
    const auto plain = Draw(nullptr);
    assert(plain == Reference(nullptr));
    assert(Alphas(plain)[1] > 0 && Alphas(plain)[2] + Alphas(plain)[1] > 40);

    const Values aliased{CAIRO_ANTIALIAS_NONE, CAIRO_SUBPIXEL_ORDER_DEFAULT, CAIRO_HINT_STYLE_DEFAULT,
        CAIRO_HINT_METRICS_DEFAULT};
    const auto sharp = Draw(&aliased);
    assert(sharp == Reference(&aliased));
    assert(Alphas(sharp)[1] == 0 && Alphas(sharp)[2] > 40);

    const Values hinted{CAIRO_ANTIALIAS_DEFAULT, CAIRO_SUBPIXEL_ORDER_DEFAULT, CAIRO_HINT_STYLE_FULL,
        CAIRO_HINT_METRICS_DEFAULT};
    const auto snapped = Draw(&hinted);
    assert(snapped == Reference(&hinted));
    assert(snapped != plain);

    for (const Values& values : {Values{CAIRO_ANTIALIAS_NONE, CAIRO_SUBPIXEL_ORDER_RGB, CAIRO_HINT_STYLE_MEDIUM,
             CAIRO_HINT_METRICS_OFF},
             Values{CAIRO_ANTIALIAS_SUBPIXEL, CAIRO_SUBPIXEL_ORDER_BGR, CAIRO_HINT_STYLE_SLIGHT,
                 CAIRO_HINT_METRICS_ON},
             defaults}) {
        assert(Draw(&values) == Reference(&values));
    }
}

void ErrorsTest() {
    CairoSurface* surface = cairo_image_surface_create(FormatArgb32, Width, Height);
    CairoContext* context = cairo_create(surface);
    cairo_set_font_options(context, nullptr);
    assert(Status(context) == StatusNullPointer);
    Set(context, {CAIRO_ANTIALIAS_NONE, CAIRO_SUBPIXEL_ORDER_RGB, CAIRO_HINT_STYLE_FULL, CAIRO_HINT_METRICS_ON});
    assert(Status(context) == StatusNullPointer);
    assert(Read(context) == defaults);
    cairo_destroy(context);

    context = cairo_create(surface);
    cairo_font_options_t* nil = cairo_font_options_copy(nullptr);
    cairo_set_font_options(context, nil);
    assert(Status(context) == StatusNoMemory);
    assert(Read(context) == defaults);
    cairo_destroy(context);
    cairo_surface_destroy(surface);
}

}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string_view mode = argv[1];
    if (mode == "values") {
        ValuesTest();
    } else if (mode == "render") {
        RenderTest();
    } else if (mode == "errors") {
        ErrorsTest();
    } else {
        return 1;
    }
    return 0;
}
