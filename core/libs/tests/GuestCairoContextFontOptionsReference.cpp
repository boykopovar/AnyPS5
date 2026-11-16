#include <CairoBackend.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
extern "C" void _cairo_mutex_initialize();
#endif

namespace CairoFontOptionsReference {

std::array<int, 4> Read(void* context) {
    cairo_font_options_t* options = aps5_cairo_font_options_create();
    aps5_cairo_get_font_options(static_cast<cairo_t*>(context), options);
    const std::array<int, 4> values{aps5_cairo_font_options_get_antialias(options),
        aps5_cairo_font_options_get_subpixel_order(options), aps5_cairo_font_options_get_hint_style(options),
        aps5_cairo_font_options_get_hint_metrics(options)};
    aps5_cairo_font_options_destroy(options);
    return values;
}

int Status(void* context) {
    return aps5_cairo_status(static_cast<cairo_t*>(context));
}

std::vector<unsigned char> Render(const std::array<int, 4>* values, const std::uint64_t* indices,
    const double* positions, int count, int width, int height) {
#ifdef _WIN32
    _cairo_mutex_initialize();
#endif
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* context = aps5_cairo_create(surface);
    if (values) {
        cairo_font_options_t* options = aps5_cairo_font_options_create();
        aps5_cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>((*values)[0]));
        aps5_cairo_font_options_set_subpixel_order(options, static_cast<cairo_subpixel_order_t>((*values)[1]));
        aps5_cairo_font_options_set_hint_style(options, static_cast<cairo_hint_style_t>((*values)[2]));
        aps5_cairo_font_options_set_hint_metrics(options, static_cast<cairo_hint_metrics_t>((*values)[3]));
        aps5_cairo_set_font_options(context, options);
        aps5_cairo_font_options_destroy(options);
    }
    std::vector<cairo_glyph_t> glyphs(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        glyphs[i] = {static_cast<unsigned long>(indices[i]), positions[i * 2], positions[i * 2 + 1]};
    }
    aps5_cairo_show_glyphs(context, glyphs.data(), count);
    aps5_cairo_destroy(context);
    aps5_cairo_surface_flush(surface);
    const std::size_t size = static_cast<std::size_t>(aps5_cairo_image_surface_get_stride(surface)) * height;
    std::vector<unsigned char> pixels(size);
    std::memcpy(pixels.data(), aps5_cairo_image_surface_get_data(surface), size);
    aps5_cairo_surface_destroy(surface);
    return pixels;
}

}
