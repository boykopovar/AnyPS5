#include "prx/libcairo/include/InventoryExports.hpp"
#include <cstdlib>

extern "C" void APS5_VABI cairo_glyph_free(cairo_glyph_t* glyphs) {
    std::free(glyphs);
}
