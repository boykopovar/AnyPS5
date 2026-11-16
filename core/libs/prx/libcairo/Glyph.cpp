#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

extern "C" void APS5_VABI cairo_glyph_free(cairo_glyph_t* glyphs) {
    if (glyphs) NotImplemented_nid_no_patch(__func__);
}
