#include "prx/libcairo/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

static_assert(sizeof(cairo_glyph_t) == 24);
static_assert(sizeof(cairo_glyph_t::index) == 8);
static_assert(offsetof(cairo_glyph_t, index) == 0);
static_assert(offsetof(cairo_glyph_t, x) == 8);
static_assert(offsetof(cairo_glyph_t, y) == 16);
static_assert(std::is_same_v<decltype(cairo_glyph_free(nullptr)), void>);

int main() {
    cairo_glyph_free(nullptr);
    for (int i = 1; i <= 64; ++i) {
        auto* glyphs = static_cast<cairo_glyph_t*>(std::calloc(i, sizeof(cairo_glyph_t)));
        if (!glyphs) return 1;
        glyphs[0] = {42, -1, 3.5};
        cairo_glyph_free(glyphs);
    }
}
