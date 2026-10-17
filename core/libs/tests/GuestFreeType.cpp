#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_New_Face_nid_postfix(void*, const char*, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t);
std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(GuestFaceRec*, std::uint64_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Render_Glyph_nid_postfix(GuestGlyphSlotRec*, std::int32_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TCall> static bool ThrowsWith(TCall call, const std::string& prefix) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).rfind(prefix, 0) == 0;
    }
    return false;
}

static void Put16(std::vector<unsigned char>& out, std::uint32_t value) {
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value));
}

static void Put32(std::vector<unsigned char>& out, std::uint32_t value) {
    Put16(out, value >> 16);
    Put16(out, value & 0xFFFFu);
}

static void PutAll16(std::vector<unsigned char>& out, std::initializer_list<int> values) {
    for (const int value : values) Put16(out, static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)));
}

static std::vector<unsigned char> SquareGlyphFont(std::uint32_t codepoint, int width) {
    std::vector<unsigned char> head;
    Put32(head, 0x00010000u);
    Put32(head, 0x00010000u);
    Put32(head, 0);
    Put32(head, 0x5F0F3CF5u);
    PutAll16(head, {0x000B, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, width, 800, 0, 8, 2, 0, 0});
    std::vector<unsigned char> hhea;
    Put32(hhea, 0x00010000u);
    PutAll16(hhea, {800, -200, 0, width, 0, 0, width, 1, 0, 0, 0, 0, 0, 0, 0, 2});
    std::vector<unsigned char> maxp;
    Put32(maxp, 0x00010000u);
    PutAll16(maxp, {2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    std::vector<unsigned char> hmtx;
    PutAll16(hmtx, {width, 0, width, 100});
    std::vector<unsigned char> glyf;
    PutAll16(glyf, {1, 100, 0, width - 100, 700, 3, 0});
    glyf.insert(glyf.end(), {1, 1, 1, 1});
    PutAll16(glyf, {100, 0, width - 200, 0, 0, 700, 0, -700});
    std::vector<unsigned char> loca;
    PutAll16(loca, {0, 0, static_cast<int>(glyf.size() / 2)});
    std::vector<unsigned char> cmap;
    PutAll16(cmap, {0, 1, 3, 1});
    Put32(cmap, 12);
    PutAll16(cmap, {4, 32, 0, 4, 4, 1, 0, static_cast<int>(codepoint), 0xFFFF, 0, static_cast<int>(codepoint), 0xFFFF, 1 - static_cast<int>(codepoint), 1, 0, 0});
    const std::vector<std::pair<const char*, const std::vector<unsigned char>*>> tables = {
        {"cmap", &cmap}, {"glyf", &glyf}, {"head", &head}, {"hhea", &hhea}, {"hmtx", &hmtx}, {"loca", &loca}, {"maxp", &maxp}};
    std::vector<unsigned char> font;
    Put32(font, 0x00010000u);
    PutAll16(font, {static_cast<int>(tables.size()), 64, 2, static_cast<int>(tables.size()) * 16 - 64});
    std::uint32_t offset = 12 + static_cast<std::uint32_t>(tables.size()) * 16;
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), tag, tag + 4);
        Put32(font, 0);
        Put32(font, offset);
        Put32(font, static_cast<std::uint32_t>(data->size()));
        offset += (static_cast<std::uint32_t>(data->size()) + 3u) & ~3u;
    }
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), data->begin(), data->end());
        font.resize((font.size() + 3u) & ~std::size_t{3});
    }
    return font;
}

static void FaceMatches(const GuestFaceRec* face, FT_Face host, const void* library) {
    Require(face->num_faces == host->num_faces && face->face_index == host->face_index && face->face_flags == host->face_flags);
    Require(face->style_flags == host->style_flags && face->num_glyphs == host->num_glyphs && face->num_glyphs == 2);
    Require((face->family_name == nullptr) == (host->family_name == nullptr) && (face->style_name == nullptr) == (host->style_name == nullptr));
    Require(face->num_fixed_sizes == host->num_fixed_sizes && face->available_sizes == nullptr);
    Require(face->num_charmaps == 1 && host->num_charmaps == 1 && face->charmap == face->charmaps[0]);
    const GuestCharMapRec* charmap = face->charmaps[0];
    Require(charmap->face == face && charmap->encoding == static_cast<std::int32_t>(host->charmaps[0]->encoding) && charmap->platform_id == 3 && charmap->encoding_id == 1);
    Require(face->bbox.xMin == host->bbox.xMin && face->bbox.yMin == host->bbox.yMin && face->bbox.xMax == host->bbox.xMax && face->bbox.yMax == host->bbox.yMax);
    Require(face->units_per_EM == 1000 && face->ascender == host->ascender && face->descender == host->descender && face->height == host->height);
    Require(face->max_advance_width == host->max_advance_width && face->max_advance_height == host->max_advance_height);
    Require(face->underline_position == host->underline_position && face->underline_thickness == host->underline_thickness);
    Require(face->glyph != nullptr && face->glyph->face == face && face->glyph->library == library && face->size != nullptr && face->size->face == face);
}

static void SlotMatches(const GuestGlyphSlotRec* slot, FT_GlyphSlot host) {
    Require(slot->glyph_index == host->glyph_index && slot->format == static_cast<std::uint32_t>(host->format) && slot->next == nullptr);
    const auto& metrics = host->metrics;
    Require(slot->metrics.width == metrics.width && slot->metrics.height == metrics.height && slot->metrics.horiBearingX == metrics.horiBearingX);
    Require(slot->metrics.horiBearingY == metrics.horiBearingY && slot->metrics.horiAdvance == metrics.horiAdvance && slot->metrics.vertBearingX == metrics.vertBearingX);
    Require(slot->metrics.vertBearingY == metrics.vertBearingY && slot->metrics.vertAdvance == metrics.vertAdvance);
    Require(slot->linearHoriAdvance == host->linearHoriAdvance && slot->linearVertAdvance == host->linearVertAdvance);
    Require(slot->advance.x == host->advance.x && slot->advance.y == host->advance.y);
    Require(slot->bitmap.rows == host->bitmap.rows && slot->bitmap.width == host->bitmap.width && slot->bitmap.pitch == host->bitmap.pitch);
    Require(slot->bitmap.num_grays == host->bitmap.num_grays && slot->bitmap.pixel_mode == host->bitmap.pixel_mode && slot->bitmap_left == host->bitmap_left && slot->bitmap_top == host->bitmap_top);
    const std::size_t bytes = static_cast<std::size_t>(host->bitmap.rows) * static_cast<std::size_t>(host->bitmap.pitch < 0 ? -host->bitmap.pitch : host->bitmap.pitch);
    Require((slot->bitmap.buffer == nullptr) == (host->bitmap.buffer == nullptr));
    Require(!host->bitmap.buffer || std::memcmp(slot->bitmap.buffer, host->bitmap.buffer, bytes) == 0);
    Require(slot->outline.n_points == host->outline.n_points && slot->outline.n_contours == host->outline.n_contours && slot->outline.flags == host->outline.flags);
    for (unsigned index = 0; index < host->outline.n_points; ++index) {
        Require(slot->outline.points[index].x == host->outline.points[index].x && slot->outline.points[index].y == host->outline.points[index].y);
        Require(slot->outline.tags[index] == host->outline.tags[index]);
    }
    for (unsigned index = 0; index < host->outline.n_contours; ++index) Require(slot->outline.contours[index] == host->outline.contours[index]);
    Require(slot->lsb_delta == host->lsb_delta && slot->rsb_delta == host->rsb_delta && slot->control_len == host->control_len);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_guest_freetype-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "app0");
    const std::vector<unsigned char> font = SquareGlyphFont('A', 700);
    {
        std::ofstream file(root / "app0" / "font.ttf", std::ios::binary);
        file.write(reinterpret_cast<const char*>(font.data()), static_cast<std::streamsize>(font.size()));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    FT_Library hostLibrary = nullptr;
    Require(FT_Init_FreeType(&hostLibrary) == 0);
    FT_Face host = nullptr;
    Require(FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &host) == 0);
    FT_Face hostSecond = nullptr;
    const FT_Error secondFace = FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), 1, &hostSecond);
    Require(secondFace != 0);

    Require(FT_Init_FreeType_nid_postfix(nullptr) == FT_Err_Invalid_Argument);
    void* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && library != nullptr);

    GuestFaceRec* face = nullptr;
    Require(FT_New_Face_nid_postfix(library, nullptr, 0, &face) == FT_Err_Invalid_Argument);
    Require(FT_New_Face_nid_postfix(nullptr, "/app0/font.ttf", 0, &face) == FT_Err_Invalid_Library_Handle);
    Require(FT_New_Face_nid_postfix(library, "/app0/missing.ttf", 0, &face) == FT_Err_Cannot_Open_Resource && face == nullptr);
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", 0, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", -1, nullptr) == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", 1, &face) == secondFace && face == nullptr);
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", 0x80000001ll, &face) == secondFace && face == nullptr);
    GuestFaceRec* masked = nullptr;
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", 0x180000000ll, &masked) == 0 && masked != nullptr && masked->face_index == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/font.ttf", 0, &face) == 0 && face != nullptr && face != masked);
    FaceMatches(face, host, library);

    Require(FT_Set_Pixel_Sizes_nid_postfix(nullptr, 0, 100) == FT_Err_Invalid_Face_Handle);
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 100) == 0 && FT_Set_Pixel_Sizes(host, 0, 100) == 0);
    const auto& metrics = host->size->metrics;
    const auto& guestMetrics = face->size->metrics;
    Require(guestMetrics.x_ppem == 100 && guestMetrics.y_ppem == 100 && guestMetrics.x_scale == metrics.x_scale && guestMetrics.y_scale == metrics.y_scale);
    Require(guestMetrics.ascender == metrics.ascender && guestMetrics.descender == metrics.descender && guestMetrics.height == metrics.height && guestMetrics.max_advance == metrics.max_advance);

    Require(FT_Get_Char_Index_nid_postfix(face, 'A') == 1 && FT_Get_Char_Index_nid_postfix(face, 'B') == 0);
    Require(FT_Get_Char_Index_nid_postfix(face, 0x100000041ull) == 1);
    Require(FT_Get_Char_Index_nid_postfix(nullptr, 'A') == 0);

    Require(FT_Load_Glyph_nid_postfix(nullptr, 1, FT_LOAD_DEFAULT) == FT_Err_Invalid_Face_Handle);
    Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(host, 1, FT_LOAD_DEFAULT) == 0);
    GuestGlyphSlotRec* slot = face->glyph;
    Require(slot->format == FT_GLYPH_FORMAT_OUTLINE && slot->glyph_index == 1 && slot->advance.x == 70 * 64 && slot->outline.n_points == 4);
    SlotMatches(slot, host->glyph);

    Require(FT_Render_Glyph_nid_postfix(nullptr, FT_RENDER_MODE_NORMAL) == FT_Err_Invalid_Argument);
    for (unsigned index = 0; index < slot->outline.n_points; ++index) slot->outline.points[index].x += 64;
    FT_Outline_Translate(&host->glyph->outline, 64, 0);
    Require(FT_Render_Glyph_nid_postfix(slot, FT_RENDER_MODE_NORMAL) == 0 && FT_Render_Glyph(host->glyph, FT_RENDER_MODE_NORMAL) == 0);
    Require(slot->format == FT_GLYPH_FORMAT_BITMAP && slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY && slot->bitmap.num_grays == 256);
    Require(slot->bitmap.rows == 70 && slot->bitmap.width == 50 && slot->bitmap_left == 11 && slot->bitmap_top == 70);
    Require(slot->bitmap.buffer[35 * slot->bitmap.pitch + 25] == 255);
    SlotMatches(slot, host->glyph);

    Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_DEFAULT) == 0);
    GuestVector replaced[4]{};
    GuestVector* original = slot->outline.points;
    slot->outline.points = replaced;
    Require(ThrowsWith([slot] { FT_Render_Glyph_nid_postfix(slot, FT_RENDER_MODE_NORMAL); }, "FT_Render_Glyph: an outline whose arrays the guest replaced is not implemented"));
    slot->outline.points = original;
    Require(FT_Render_Glyph_nid_postfix(slot, FT_RENDER_MODE_NORMAL) == 0 && slot->bitmap_left == 10);

    FT_Done_Face(host);
    FT_Done_FreeType(hostLibrary);
    std::filesystem::current_path(previous);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
