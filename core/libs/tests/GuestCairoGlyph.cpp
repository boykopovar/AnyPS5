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
    const std::size_t counts[] = {1, 3, 1024};
    for (std::size_t count : counts) {
        auto* glyphs = static_cast<cairo_glyph_t*>(std::calloc(count, sizeof(cairo_glyph_t)));
        if (glyphs == nullptr) {
            std::fprintf(stderr, "cairo glyph: allocation of %zu glyphs failed\n", count);
            return 1;
        }
        for (std::size_t i = 0; i < count; ++i) glyphs[i] = {static_cast<std::uint64_t>(i), 1.5 * static_cast<double>(i), 2.0};
        cairo_glyph_free(glyphs);
    }
    std::printf("cairo glyph: freed null and allocated glyph arrays\n");
    return 0;
}
