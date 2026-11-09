#include <ft2build.h>
#include FT_GLYPH_H
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>

extern "C" void APS5_VABI sceFreeTypeOtGlyphGetCBox_nid_no_patch(FT_Glyph glyph, FT_UInt mode, FT_BBox* box);

int main() {
    FT_BBox box{1, 2, 3, 4};
    sceFreeTypeOtGlyphGetCBox_nid_no_patch(nullptr, FT_GLYPH_BBOX_SUBPIXELS, &box);
    if (box.xMin == 0 && box.yMin == 0 && box.xMax == 0 && box.yMax == 0) {
        std::puts("FreeType Ot glyph control box test passed");
        return 0;
    }
    std::fprintf(stderr, "FreeType Ot glyph control box: null glyph did not clear the box\n");
    return 1;
}
