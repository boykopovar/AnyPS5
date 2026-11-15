#include "prx/libcairo/include/GlyphDrawing.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <cstddef>
#include <limits>
#include <vector>

static_assert(sizeof(CairoGlyph) == 24);
static_assert(offsetof(CairoGlyph, x) == 8);
static_assert(offsetof(CairoGlyph, y) == 16);

namespace {

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

template<typename Draw>
void WithGlyphs(const CairoGlyph* glyphs, int numGlyphs, const char* function, Draw draw) {
    if constexpr (sizeof(unsigned long) == sizeof(std::uint64_t)) {
        static_assert(sizeof(cairo_glyph_t) == sizeof(CairoGlyph));
        static_assert(offsetof(cairo_glyph_t, x) == offsetof(CairoGlyph, x));
        static_assert(offsetof(cairo_glyph_t, y) == offsetof(CairoGlyph, y));
        draw(reinterpret_cast<const cairo_glyph_t*>(glyphs), numGlyphs);
    } else {
        if (!glyphs || numGlyphs <= 0) {
            draw(nullptr, numGlyphs);
            return;
        }
        std::vector<cairo_glyph_t> host(static_cast<std::size_t>(numGlyphs));
        for (std::size_t i = 0; i < host.size(); ++i) {
            if (glyphs[i].index > std::numeric_limits<unsigned long>::max()) NotImplemented_nid_no_patch(function);
            host[i] = {static_cast<unsigned long>(glyphs[i].index), glyphs[i].x, glyphs[i].y};
        }
        draw(host.data(), numGlyphs);
    }
}

}

extern "C" {

void APS5_VABI cairo_show_glyphs(CairoContext* context, const CairoGlyph* glyphs, int numGlyphs) {
    WithGlyphs(glyphs, numGlyphs, __func__, [context](const cairo_glyph_t* host, int count) {
        aps5_cairo_show_glyphs(Backend(context), host, count);
    });
}

void APS5_VABI cairo_glyph_path(CairoContext* context, const CairoGlyph* glyphs, int numGlyphs) {
    WithGlyphs(glyphs, numGlyphs, __func__, [context](const cairo_glyph_t* host, int count) {
        aps5_cairo_glyph_path(Backend(context), host, count);
    });
}

}
