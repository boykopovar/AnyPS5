#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <cstdlib>
#include <exception>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    FreeType::GuestBBox empty{1, 2, 3, 4};
    sceFreeTypeOtGlyphGetCBox_nid_no_patch(nullptr, FT_GLYPH_BBOX_SUBPIXELS, &empty);
    Require(empty.xMin == 0 && empty.yMin == 0 && empty.xMax == 0 && empty.yMax == 0);

    const auto font = SquareGlyphFont('A', 700);
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FT_Library hostLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0);
    Require(FT_Init_FreeType(&hostLibrary) == 0);
    FreeType::GuestFaceRec* guestFace = nullptr;
    FT_Face hostFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(guestLibrary, font.data(), font.size(), 0, &guestFace) == 0);
    Require(FT_New_Memory_Face(hostLibrary, font.data(), font.size(), 0, &hostFace) == 0);
    Require(FT_Set_Pixel_Sizes_nid_postfix(guestFace, 0, 48) == 0);
    Require(FT_Set_Pixel_Sizes(hostFace, 0, 48) == 0);
    Require(FT_Load_Glyph_nid_postfix(guestFace, 1, FT_LOAD_NO_HINTING) == 0);
    Require(FT_Load_Glyph(hostFace, 1, FT_LOAD_NO_HINTING) == 0);
    FreeType::GuestGlyphRec* guestGlyph = nullptr;
    FT_Glyph hostGlyph = nullptr;
    Require(FT_Get_Glyph_nid_postfix(guestFace->glyph, &guestGlyph) == 0);
    Require(FT_Get_Glyph(hostFace->glyph, &hostGlyph) == 0);
    for (FT_UInt mode : {FT_GLYPH_BBOX_SUBPIXELS, FT_GLYPH_BBOX_GRIDFIT, FT_GLYPH_BBOX_TRUNCATE, FT_GLYPH_BBOX_PIXELS}) {
        FreeType::GuestBBox guestBox{};
        FT_BBox hostBox{};
        sceFreeTypeOtGlyphGetCBox_nid_no_patch(guestGlyph, mode, &guestBox);
        FT_Glyph_Get_CBox(hostGlyph, mode, &hostBox);
        Require(guestBox.xMin == hostBox.xMin && guestBox.yMin == hostBox.yMin &&
                guestBox.xMax == hostBox.xMax && guestBox.yMax == hostBox.yMax);
    }
    bool rejected = false;
    try {
        FreeType::GuestBBox box{};
        sceFreeTypeOtGlyphGetCBox_nid_no_patch(reinterpret_cast<FreeType::GuestGlyphRec*>(hostGlyph), FT_GLYPH_BBOX_SUBPIXELS, &box);
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
    FT_Done_Glyph_nid_postfix(guestGlyph);
    FT_Done_Glyph(hostGlyph);
    Require(FT_Done_Face_nid_postfix(guestFace) == 0);
    Require(FT_Done_Face(hostFace) == 0);
    Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0);
    Require(FT_Done_FreeType(hostLibrary) == 0);
}
