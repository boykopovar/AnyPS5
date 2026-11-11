#include "prx/libcairo/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <type_traits>

static_assert(sizeof(cairo_glyph_t) == 24);
static_assert(sizeof(cairo_glyph_t::index) == 8);
static_assert(offsetof(cairo_glyph_t, index) == 0);
static_assert(offsetof(cairo_glyph_t, x) == 8);
static_assert(offsetof(cairo_glyph_t, y) == 16);
static_assert(std::is_same_v<decltype(cairo_glyph_free(nullptr)), void>);

int main() {
    cairo_glyph_free(nullptr);
    cairo_glyph_t glyphs[3] = {{0, 0.0, 2.0}, {1, 1.5, 2.0}, {2, 3.0, 2.0}};
    try {
        cairo_glyph_free(glyphs);
    } catch (const std::runtime_error&) {
        std::printf("cairo glyph: null is a no-op and a foreign array throws\n");
        return 0;
    }
    std::fprintf(stderr, "cairo glyph: a non-null array was accepted without an allocating export\n");
    return 1;
}
