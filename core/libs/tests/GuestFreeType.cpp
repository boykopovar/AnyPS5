#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GASP_H
#include FT_OUTLINE_H
#include FT_CID_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
void APS5_VABI FT_Set_Transform_nid_postfix(GuestFaceRec*, const GuestMatrix*, const GuestVector*);
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Face_nid_postfix(void*, const char*, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Done_Face_nid_postfix(GuestFaceRec*);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t);
int APS5_VABI FT_Get_Kerning_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t, std::uint32_t, GuestVector*);
std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(GuestFaceRec*, std::uint64_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Load_Char_nid_postfix(GuestFaceRec*, std::uint64_t, std::int32_t);
int APS5_VABI FT_Render_Glyph_nid_postfix(GuestGlyphSlotRec*, std::int32_t);
int APS5_VABI FT_Set_Char_Size_nid_postfix(GuestFaceRec*, std::int64_t, std::int64_t, std::uint32_t, std::uint32_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

extern "C" int APS5_VABI FT_Get_Gasp_nid_postfix(GuestFaceRec*, std::uint32_t);

static void CheckGaspTable(const std::vector<unsigned char>* table, const std::vector<std::pair<unsigned, int>>& ranges,
                           bool versionZero, bool render) {
    const auto font = SquareGlyphFont('A', 700, false, table);
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    auto* slot = face->glyph;
    const auto glyphIndex = slot->glyph_index;
    const auto bitmap = slot->bitmap.buffer;
    const auto points = slot->outline.points;
    for (std::uint32_t ppem = 0; ppem <= 65536; ++ppem) {
        int expected = FT_GASP_NO_TABLE;
        for (const auto& [maximum, flags] : ranges) {
            if (ppem > maximum) continue;
            expected = versionZero ? flags & 3 : flags;
            break;
        }
        Require(FT_Get_Gasp_nid_postfix(face, ppem) == expected);
        Require(FT_Get_Gasp(native, ppem) == expected);
    }
    Require(FT_Get_Gasp_nid_postfix(face, UINT32_MAX) == FT_GASP_NO_TABLE);
    Require(face->glyph == slot && slot->glyph_index == glyphIndex && slot->bitmap.buffer == bitmap && slot->outline.points == points);
    if (render) {
        for (const unsigned ppem : {7u, 8u, 15u, 16u, 31u, 32u, 64u}) {
            const int flags = FT_Get_Gasp_nid_postfix(face, ppem);
            const int load = flags & FT_GASP_DO_GRIDFIT ? FT_LOAD_DEFAULT : FT_LOAD_NO_HINTING;
            const auto mode = flags & FT_GASP_DO_GRAY ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO;
            Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, ppem) == 0 && FT_Set_Pixel_Sizes(native, 0, ppem) == 0);
            Require(FT_Load_Char_nid_postfix(face, 'A', load) == 0 && FT_Load_Char(native, 'A', load) == 0);
            Require(FT_Render_Glyph_nid_postfix(face->glyph, mode) == 0 && FT_Render_Glyph(native->glyph, mode) == 0);
            const auto& actual = face->glyph->bitmap;
            const auto& expected = native->glyph->bitmap;
            Require(actual.width > 0 && actual.rows > 0);
            Require(actual.width == expected.width && actual.rows == expected.rows && actual.pitch == expected.pitch);
            Require(actual.pixel_mode == expected.pixel_mode && actual.num_grays == expected.num_grays);
            Require(face->glyph->bitmap_left == native->glyph->bitmap_left && face->glyph->bitmap_top == native->glyph->bitmap_top);
            Require(std::memcmp(actual.buffer, expected.buffer, static_cast<std::size_t>(actual.rows) * std::abs(actual.pitch)) == 0);
        }
    }
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(native) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}

static void CheckGasp() {
    Require(FT_Get_Gasp_nid_postfix(nullptr, 0) == FT_GASP_NO_TABLE);
    Require(FT_Get_Gasp_nid_postfix(nullptr, UINT32_MAX) == FT_GASP_NO_TABLE);
    CheckGaspTable(nullptr, {}, false, false);
    const std::vector<std::pair<unsigned, int>> ranges{{0, 0x8003}, {7, 2}, {15, 1}, {31, 0}, {65535, 0xFFFF}};
    for (const unsigned version : {0u, 1u}) {
        std::vector<unsigned char> table;
        Put16(table, version);
        Put16(table, ranges.size());
        for (const auto& [maximum, flags] : ranges) {
            Put16(table, maximum);
            Put16(table, flags);
        }
        CheckGaspTable(&table, ranges, version == 0, true);
    }
    std::vector<unsigned char> shortRange;
    PutAll16(shortRange, {1, 1, 48, FT_GASP_DO_GRAY});
    CheckGaspTable(&shortRange, {{48, FT_GASP_DO_GRAY}}, false, false);
    for (const std::vector<unsigned char>& table : {std::vector<unsigned char>{}, {0, 1}, {0, 1, 0, 0}, {0, 2, 0, 1, 0, 10, 0, 3}}) {
        CheckGaspTable(&table, {}, false, false);
    }
    std::puts("FreeType gasp: 8 fonts, 524304 queries, null faces and 14 rendered glyphs passed");
}

template <typename TCall> static bool ThrowsWith(TCall call, const std::string& prefix) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).rfind(prefix, 0) == 0;
    }
    return false;
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

extern "C" int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(GuestOutline*);

static unsigned orientationCases = 0;

static void CheckOrientationCase(std::vector<GuestVector> points, std::vector<std::uint16_t> contours,
                                 int expected, bool compareNative = true) {
    ++orientationCases;
    const auto originalPoints = points;
    const auto originalContours = contours;
    std::vector<unsigned char> tags(points.size(), FT_CURVE_TAG_ON);
    GuestOutline outline{static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(points.size()),
                         points.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL | FT_OUTLINE_REVERSE_FILL};
    const auto original = outline;
    Require(FT_Outline_Get_Orientation_nid_postfix(&outline) == expected);
    Require(outline.n_points == original.n_points && outline.n_contours == original.n_contours && outline.flags == original.flags);
    Require(outline.points == original.points && outline.tags == original.tags && outline.contours == original.contours);
    for (std::size_t index = 0; index < points.size(); ++index) {
        Require(points[index].x == originalPoints[index].x && points[index].y == originalPoints[index].y);
        Require(tags[index] == FT_CURVE_TAG_ON);
    }
    Require(contours == originalContours);
    if (compareNative) {
        std::vector<FT_Vector> nativePoints;
        for (const auto& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
        FT_Outline native{outline.n_contours, outline.n_points, nativePoints.data(), tags.data(), contours.data(), outline.flags};
        Require(FT_Outline_Get_Orientation(&native) == expected);
    }
}

static void CheckOutlineOrientation() {
    Require(FT_Outline_Get_Orientation_nid_postfix(nullptr) == FT_ORIENTATION_TRUETYPE);
    CheckOrientationCase({}, {}, FT_ORIENTATION_TRUETYPE);
    CheckOrientationCase({{0, 0}}, {0}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {1, 1}}, {1}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {1, 1}, {2, 2}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {40, 0}, {40, 40}, {0, 40}}, {3}, FT_ORIENTATION_POSTSCRIPT);
    CheckOrientationCase({{0, 0}, {0, 40}, {40, 40}, {40, 0}}, {3}, FT_ORIENTATION_TRUETYPE);
    CheckOrientationCase({{0, 0}, {40, 0}, {40, 40}, {0, 40}, {10, 10}, {10, 30}, {30, 30}, {30, 10}},
                          {3, 7}, FT_ORIENTATION_POSTSCRIPT);
    CheckOrientationCase({{0, 0}, {40, 0}, {40, 40}, {0, 40}, {0, 0}, {0, 40}, {40, 40}, {40, 0}},
                          {3, 7}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {40, 0}, {40, 40}, {0, 40}}, {}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{1048577, 0}, {1048578, 1}, {1048577, 2}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{-0x1000000, -0x1000000}, {0x1000000, -0x1000000},
                           {0x1000000, 0x1000000}, {-0x1000000, 0x1000000}}, {3}, FT_ORIENTATION_POSTSCRIPT);
    CheckOrientationCase({{-0x1000001, 0}, {0, 100}, {100, 0}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, -0x1000001}, {100, 0}, {0, 100}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {0x1000001, 100}, {100, 0}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{0, 0}, {100, 0x1000001}, {0, 100}}, {2}, FT_ORIENTATION_NONE);
    CheckOrientationCase({{INT64_MIN, INT64_MIN}, {INT64_MAX, INT64_MAX}, {0, 1}}, {2}, FT_ORIENTATION_NONE, false);
    std::vector<GuestVector> repeated{{0, 0}, {32767, 0}, {32767, 32767}, {0, 32767},
                                      {0, 0}, {32767, 0}, {32767, 32767}, {0, 32767}};
    CheckOrientationCase(repeated, {3, 7}, FT_ORIENTATION_POSTSCRIPT, sizeof(FT_Pos) == 8);
    if constexpr (sizeof(FT_Pos) == 4) {
        FT_Vector nativePoints[]{{0, 0}, {32767, 0}, {32767, 32767}, {0, 32767},
                                  {0, 0}, {32767, 0}, {32767, 32767}, {0, 32767}};
        unsigned short contours[]{3, 7};
        FT_Outline native{2, 8, nativePoints, nullptr, contours, 0};
        Require(FT_Outline_Get_Orientation(&native) == FT_ORIENTATION_TRUETYPE);
    }
    std::vector<GuestVector> maximum(65535);
    const GuestVector square[]{{0, 0}, {32767, 0}, {32767, 32767}, {0, 32767}};
    for (std::size_t index = 0; index < maximum.size(); ++index) maximum[index] = square[index % 4];
    CheckOrientationCase(maximum, {65534}, FT_ORIENTATION_POSTSCRIPT, sizeof(FT_Pos) == 8);
    std::vector<std::uint16_t> singletons(65535);
    for (std::size_t index = 0; index < singletons.size(); ++index) singletons[index] = static_cast<std::uint16_t>(index);
    CheckOrientationCase(maximum, singletons, FT_ORIENTATION_NONE);
    std::mt19937 random(0xF7013);
    for (unsigned test = 0; test < 512; ++test) {
        std::vector<GuestVector> points(random() % 10 + 3);
        for (auto& point : points) point = {static_cast<int>(random() % 1024) - 512, static_cast<int>(random() % 1024) - 512};
        std::int64_t twiceArea = 0;
        auto previous = points.back();
        for (const auto& current : points) {
            twiceArea += previous.x * current.y - current.x * previous.y;
            previous = current;
        }
        const int expected = twiceArea > 0 ? FT_ORIENTATION_POSTSCRIPT : twiceArea < 0 ? FT_ORIENTATION_TRUETYPE : FT_ORIENTATION_NONE;
        const std::vector<std::uint16_t> contours{static_cast<std::uint16_t>(points.size() - 1)};
        CheckOrientationCase(points, contours, expected);
        for (auto& point : points) { point.x += 1000; point.y -= 2000; }
        CheckOrientationCase(points, contours, expected);
        std::reverse(points.begin(), points.end());
        CheckOrientationCase(points, contours, twiceArea == 0 ? FT_ORIENTATION_NONE : expected == FT_ORIENTATION_POSTSCRIPT ? FT_ORIENTATION_TRUETYPE : FT_ORIENTATION_POSTSCRIPT);
    }
    const auto font = SquareGlyphFont('A', 700);
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 32) == 0 && FT_Set_Pixel_Sizes(native, 0, 32) == 0);
    for (const bool reflect : {false, true}) {
        Require(FT_Load_Char_nid_postfix(face, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        Require(FT_Load_Char(native, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        if (reflect) {
            for (unsigned index = 0; index < face->glyph->outline.n_points; ++index) {
                face->glyph->outline.points[index].x = -face->glyph->outline.points[index].x;
                native->glyph->outline.points[index].x = -native->glyph->outline.points[index].x;
            }
        }
        Require(FT_Outline_Get_Orientation_nid_postfix(&face->glyph->outline) == (reflect ? FT_ORIENTATION_POSTSCRIPT : FT_ORIENTATION_TRUETYPE));
        Require(FT_Outline_Get_Orientation(&native->glyph->outline) == (reflect ? FT_ORIENTATION_POSTSCRIPT : FT_ORIENTATION_TRUETYPE));
        Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == 0 && FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL) == 0);
        const auto& actual = face->glyph->bitmap;
        const auto& expected = native->glyph->bitmap;
        Require(actual.width > 0 && actual.rows > 0 && actual.width == expected.width && actual.rows == expected.rows && actual.pitch == expected.pitch);
        Require(actual.pixel_mode == expected.pixel_mode && face->glyph->bitmap_left == native->glyph->bitmap_left && face->glyph->bitmap_top == native->glyph->bitmap_top);
        Require(std::memcmp(actual.buffer, expected.buffer, static_cast<std::size_t>(actual.rows) * std::abs(actual.pitch)) == 0);
    }
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(native) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
    std::printf("FreeType outline orientation: %u polygon cases and 2 rendered glyphs passed\n", orientationCases);
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

static void SizeMatches(const GuestFaceRec* face, FT_Face host) {
    const auto& actual = face->size->metrics;
    const auto& expected = host->size->metrics;
    Require(actual.x_ppem == expected.x_ppem && actual.y_ppem == expected.y_ppem);
    Require(actual.x_scale == expected.x_scale && actual.y_scale == expected.y_scale);
    Require(actual.ascender == expected.ascender && actual.descender == expected.descender);
    Require(actual.height == expected.height && actual.max_advance == expected.max_advance);
}

struct Finalizers {
    std::vector<int> calls;
};

extern "C" {
int APS5_VABI FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(GuestFaceRec*, const char**, const char**, std::int32_t*);
int APS5_VABI FT_Get_CID_Is_Internally_CID_Keyed_nid_postfix(GuestFaceRec*, std::uint8_t*);
int APS5_VABI FT_Get_CID_From_Glyph_Index_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t*);
}

static std::vector<unsigned char> CidMetadataFont(bool keyed, const std::string& registry, const std::string& ordering,
                                                int supplement, unsigned cid) {
    const auto index = [](const std::vector<std::vector<unsigned char>>& objects) {
        std::vector<unsigned char> result;
        Put16(result, objects.size());
        if (objects.empty()) return result;
        result.insert(result.end(), {1, 1});
        unsigned offset = 1;
        for (const auto& object : objects) {
            offset += object.size();
            Require(offset < 256);
            result.push_back(static_cast<unsigned char>(offset));
        }
        for (const auto& object : objects) result.insert(result.end(), object.begin(), object.end());
        return result;
    };
    const auto integer = [](std::vector<unsigned char>& dict, int value) {
        dict.push_back(29);
        Put32(dict, static_cast<std::uint32_t>(value));
    };
    const auto bytes = [](const std::string& text) { return std::vector<unsigned char>(text.begin(), text.end()); };
    std::array<int, 5> offsets{};
    std::vector<unsigned char> font;
    for (unsigned pass = 0; pass < 2; ++pass) {
        std::vector<unsigned char> top;
        for (const int value : {0, 0, 700, 800}) integer(top, value);
        top.push_back(5);
        if (keyed) {
            for (const int value : {391, 392, supplement}) integer(top, value);
            top.insert(top.end(), {12, 30});
            integer(top, cid + 1);
            top.insert(top.end(), {12, 34});
        }
        integer(top, offsets[0]); top.push_back(15);
        integer(top, offsets[1]); top.push_back(17);
        if (keyed) {
            integer(top, offsets[3]); top.insert(top.end(), {12, 36});
            integer(top, offsets[2]); top.insert(top.end(), {12, 37});
        }
        font = {1, 0, 4, 4};
        const auto append = [&](const std::vector<std::vector<unsigned char>>& objects) {
            const auto data = index(objects);
            font.insert(font.end(), data.begin(), data.end());
        };
        append({bytes(keyed ? "TestCID" : "TestNamed")});
        append({top});
        append({bytes(registry), bytes(ordering)});
        append({});
        offsets[0] = static_cast<int>(font.size());
        font.push_back(0); Put16(font, cid);
        offsets[2] = static_cast<int>(font.size());
        font.insert(font.end(), {0, 0, 0});
        offsets[1] = static_cast<int>(font.size());
        append({{248, 236, 14}, {248, 236, 239, 139, 21, 248, 136, 139, 139, 249, 80, 252, 136, 139, 139, 253, 80, 5, 14}});
        offsets[3] = static_cast<int>(font.size());
        std::vector<unsigned char> privateDict;
        integer(privateDict, 0); integer(privateDict, offsets[4]); privateDict.push_back(18);
        append({privateDict});
        offsets[4] = static_cast<int>(font.size());
    }
    return font;
}

static bool SameCidString(const char* a, const char* b) { return a && b ? std::strcmp(a, b) == 0 : a == b; }

static unsigned cidQueries = 0;

static void CheckCidQueries(GuestFaceRec* face, FT_Face native, int kind, unsigned cid, int supplement,
                            const char* registry, const char* ordering) {
    for (unsigned mask = 0; mask < 8; ++mask) {
        struct Result {
            std::uint64_t before = 0x123456789ABCDEF0;
            const char* registry = "registry sentinel";
            const char* ordering = "ordering sentinel";
            std::int32_t supplement = 0x12345678;
            std::uint32_t after = 0xFEDCBA98;
        } actual, expected;
        const int error = FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(face, mask & 1 ? &actual.registry : nullptr,
                            mask & 2 ? &actual.ordering : nullptr, mask & 4 ? &actual.supplement : nullptr);
        Require(error == FT_Get_CID_Registry_Ordering_Supplement(native, mask & 1 ? &expected.registry : nullptr,
                            mask & 2 ? &expected.ordering : nullptr, mask & 4 ? &expected.supplement : nullptr));
        Require(error == (kind > 0 ? 0 : FT_Err_Invalid_Argument));
        Require(SameCidString(actual.registry, expected.registry) && SameCidString(actual.ordering, expected.ordering));
        Require(actual.supplement == expected.supplement && actual.before == 0x123456789ABCDEF0 && actual.after == 0xFEDCBA98);
        if (mask & 1) Require(SameCidString(actual.registry, kind > 0 ? registry : nullptr));
        if (mask & 2) Require(SameCidString(actual.ordering, kind > 0 ? ordering : nullptr));
        if (mask & 4) Require(actual.supplement == (kind > 0 ? supplement : 0));
        ++cidQueries;
    }
    const char* alias = "sentinel";
    const char* nativeAlias = "sentinel";
    Require(FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(face, &alias, &alias, nullptr) ==
            FT_Get_CID_Registry_Ordering_Supplement(native, &nativeAlias, &nativeAlias, nullptr));
    Require(SameCidString(alias, nativeAlias) && SameCidString(alias, kind > 0 ? ordering : nullptr));
    ++cidQueries;
    for (const bool output : {false, true}) {
        struct Result { std::uint8_t before = 0x12; std::uint8_t value = 0xA5; std::array<std::uint8_t, 3> after{0x34, 0x56, 0x78}; } actual, expected;
        const int error = FT_Get_CID_Is_Internally_CID_Keyed_nid_postfix(face, output ? &actual.value : nullptr);
        Require(error == FT_Get_CID_Is_Internally_CID_Keyed(native, output ? &expected.value : nullptr));
        Require(error == (kind >= 0 ? 0 : FT_Err_Invalid_Argument));
        Require(actual.value == expected.value && (!output || actual.value == (kind > 0 ? 1 : 0)));
        Require(actual.before == 0x12 && actual.after == expected.after);
        ++cidQueries;
        for (const unsigned glyph : {0u, 1u, 2u, 41u, 65535u, UINT32_MAX}) {
            struct Mapping { std::uint32_t before = 0x12345678; std::uint32_t value = 0xDEADBEEF; std::uint32_t after = 0x87654321; } actualCid, nativeCid;
            const int mappingError = FT_Get_CID_From_Glyph_Index_nid_postfix(face, glyph, output ? &actualCid.value : nullptr);
            Require(mappingError == FT_Get_CID_From_Glyph_Index(native, glyph, output ? &nativeCid.value : nullptr));
            Require(mappingError == (kind > 0 && glyph < 2 ? 0 : FT_Err_Invalid_Argument));
            Require(actualCid.value == nativeCid.value && (!output || actualCid.value == (mappingError == 0 && glyph == 1 ? cid : 0)));
            Require(actualCid.before == 0x12345678 && actualCid.after == 0x87654321);
            ++cidQueries;
        }
    }
}

static void CheckCidMetadata() {
    CheckCidQueries(nullptr, nullptr, -2, 0, 0, nullptr, nullptr);
    struct Fixture { std::vector<unsigned char> bytes; int kind; unsigned cid; int supplement; const char* registry; const char* ordering; unsigned renderGlyph; };
    const std::array<Fixture, 4> fixtures{{
        {CidMetadataFont(true, "Adobe", "Identity", 7, 41), 1, 41, 7, "Adobe", "Identity", 41},
        {CidMetadataFont(true, "TestRegistry", "TestOrdering", 9, 65535), 1, 65535, 9, "TestRegistry", "TestOrdering", 65535},
        {CidMetadataFont(false, "Adobe", "Identity", 0, 41), 0, 0, 0, nullptr, nullptr, 1},
        {SquareGlyphFont('A', 700), -1, 0, 0, nullptr, nullptr, 1}}};
    for (const auto& fixture : fixtures) {
        void* library = nullptr;
        GuestFaceRec* face = nullptr;
        FT_Library nativeLibrary = nullptr;
        FT_Face native = nullptr;
        Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
        Require(FT_New_Memory_Face_nid_postfix(library, fixture.bytes.data(), fixture.bytes.size(), 0, &face) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, fixture.bytes.data(), static_cast<FT_Long>(fixture.bytes.size()), 0, &native) == 0);
        CheckCidQueries(face, native, fixture.kind, fixture.cid, fixture.supplement, fixture.registry, fixture.ordering);
        const char* registry = nullptr;
        const char* ordering = nullptr;
        if (fixture.kind > 0) Require(FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(face, &registry, &ordering, nullptr) == 0);
        Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 32) == 0 && FT_Set_Pixel_Sizes(native, 0, 32) == 0);
        Require(FT_Load_Glyph_nid_postfix(face, fixture.renderGlyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        Require(FT_Load_Glyph(native, fixture.renderGlyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == 0 && FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL) == 0);
        SlotMatches(face->glyph, native->glyph);
        Require(face->glyph->bitmap.width > 0 && face->glyph->bitmap.rows > 0);
        if (fixture.kind > 0) {
            const char* againRegistry = nullptr;
            const char* againOrdering = nullptr;
            Require(FT_Get_CID_Registry_Ordering_Supplement_nid_postfix(face, &againRegistry, &againOrdering, nullptr) == 0);
            Require(registry == againRegistry && ordering == againOrdering);
            Require(SameCidString(registry, fixture.registry) && SameCidString(ordering, fixture.ordering));
        }
        Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(native) == 0);
        Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
    }
    std::printf("FreeType CID metadata: %u query scenarios, 4 fonts and 4 rendered glyphs passed\n", cidQueries);
}

static void NativeSlotFinalizer(void* object) {
    auto* slot = static_cast<FT_GlyphSlot>(object);
    Require(slot->face->glyph == nullptr);
    static_cast<Finalizers*>(slot->generic.data)->calls.push_back(1);
}

static void NativeSizeFinalizer(void* object) {
    auto* size = static_cast<FT_Size>(object);
    Require(size->face->size == size);
    static_cast<Finalizers*>(size->generic.data)->calls.push_back(2);
}

static void NativeFaceFinalizer(void* object) {
    auto* face = static_cast<FT_Face>(object);
    Require(face->glyph == nullptr && face->size == nullptr);
    static_cast<Finalizers*>(face->generic.data)->calls.push_back(3);
}

static void APS5_VABI GuestSlotFinalizer(void* object) {
    auto* slot = static_cast<GuestGlyphSlotRec*>(object);
    Require(static_cast<GuestFaceRec*>(slot->face)->glyph == nullptr);
    static_cast<Finalizers*>(slot->generic.data)->calls.push_back(1);
}

static void APS5_VABI GuestSizeFinalizer(void* object) {
    auto* size = static_cast<GuestSizeRec*>(object);
    Require(static_cast<GuestFaceRec*>(size->face)->size == size);
    static_cast<Finalizers*>(size->generic.data)->calls.push_back(2);
}

static void APS5_VABI GuestFaceFinalizer(void* object) {
    auto* face = static_cast<GuestFaceRec*>(object);
    Require(face->glyph == nullptr && face->size == nullptr);
    static_cast<Finalizers*>(face->generic.data)->calls.push_back(3);
}

static void SetFinalizers(FT_Face face, Finalizers& events) {
    face->generic = {&events, NativeFaceFinalizer};
    face->glyph->generic = {&events, NativeSlotFinalizer};
    face->size->generic = {&events, NativeSizeFinalizer};
}

static void SetFinalizers(GuestFaceRec* face, Finalizers& events) {
    face->generic = {&events, GuestFaceFinalizer};
    face->glyph->generic = {&events, GuestSlotFinalizer};
    face->size->generic = {&events, GuestSizeFinalizer};
}

static void CheckTransforms(const std::vector<unsigned char>& font) {
    FT_Library native = nullptr;
    FT_Face host = nullptr;
    FT_Face nativeOther = nullptr;
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    GuestFaceRec* other = nullptr;
    Require(FT_Init_FreeType(&native) == 0 && FT_Init_FreeType_nid_postfix(&library) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &host) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeOther) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &other) == 0);
    Require(FT_Set_Pixel_Sizes(host, 0, 100) == 0 && FT_Set_Pixel_Sizes_nid_postfix(face, 0, 100) == 0);
    Require(FT_Set_Pixel_Sizes(nativeOther, 0, 100) == 0 && FT_Set_Pixel_Sizes_nid_postfix(other, 0, 100) == 0);
    const std::array matrices{
        GuestMatrix{65536, 0, 0, 65536}, GuestMatrix{0, -65536, 65536, 0}, GuestMatrix{0, 65536, -65536, 0},
        GuestMatrix{-65536, 0, 0, 65536}, GuestMatrix{131072, 0, 0, 32768}, GuestMatrix{65536, 16384, -8192, 65536},
        GuestMatrix{0, 0, 0, 0}
    };
    const std::array deltas{GuestVector{0, 0}, GuestVector{64, -128}, GuestVector{31, -63}, GuestVector{-96, 192}};
    std::size_t comparisons = 0;
    for (std::size_t m = 0; m <= matrices.size(); ++m) {
        for (std::size_t d = 0; d <= deltas.size(); ++d) {
            GuestMatrix input = m < matrices.size() ? matrices[m] : matrices[0];
            GuestVector delta = d < deltas.size() ? deltas[d] : deltas[0];
            FT_Matrix matrix{static_cast<FT_Fixed>(input.xx), static_cast<FT_Fixed>(input.xy), static_cast<FT_Fixed>(input.yx), static_cast<FT_Fixed>(input.yy)};
            FT_Vector translation{static_cast<FT_Pos>(delta.x), static_cast<FT_Pos>(delta.y)};
            std::array<unsigned char, sizeof(GuestGlyphSlotRec)> savedSlot{};
            std::array<unsigned char, sizeof(GuestSizeRec)> savedSize{};
            std::memcpy(savedSlot.data(), face->glyph, savedSlot.size());
            std::memcpy(savedSize.data(), face->size, savedSize.size());
            FT_Set_Transform_nid_postfix(face, m < matrices.size() ? &input : nullptr, d < deltas.size() ? &delta : nullptr);
            FT_Set_Transform(host, m < matrices.size() ? &matrix : nullptr, d < deltas.size() ? &translation : nullptr);
            Require(std::memcmp(savedSlot.data(), face->glyph, savedSlot.size()) == 0);
            Require(std::memcmp(savedSize.data(), face->size, savedSize.size()) == 0);
            Require(input.xx == matrix.xx && input.xy == matrix.xy && input.yx == matrix.yx && input.yy == matrix.yy);
            Require(delta.x == translation.x && delta.y == translation.y);
            input = matrices[0];
            delta = deltas[0];
            for (const auto flags : std::array<std::int32_t, 3>{FT_LOAD_DEFAULT, FT_LOAD_IGNORE_TRANSFORM, FT_LOAD_RENDER}) {
                for (const std::uint32_t glyph : {0u, 1u}) {
                    Require(FT_Load_Glyph_nid_postfix(face, glyph, flags) == FT_Load_Glyph(host, glyph, flags));
                    SlotMatches(face->glyph, host->glyph);
                    if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                        Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == FT_Render_Glyph(host->glyph, FT_RENDER_MODE_NORMAL));
                        SlotMatches(face->glyph, host->glyph);
                    }
                    ++comparisons;
                }
            }
            Require(FT_Load_Glyph_nid_postfix(other, 1, FT_LOAD_DEFAULT) == FT_Load_Glyph(nativeOther, 1, FT_LOAD_DEFAULT));
            SlotMatches(other->glyph, nativeOther->glyph);
        }
    }
    GuestMatrix rejected{65536, 0, 0, 65536};
    GuestVector rejectedDelta{};
    const auto loadUnchanged = [&] {
        Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_DEFAULT) == FT_Load_Glyph(host, 1, FT_LOAD_DEFAULT));
        SlotMatches(face->glyph, host->glyph);
    };
    if constexpr (sizeof(FT_Long) < sizeof(std::int64_t)) {
        for (const auto value : std::array<std::int64_t, 5>{-0x80000001ll, 0x80000000ll, 0x100000000ll,
                                                          std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
            for (auto member : {&GuestMatrix::xx, &GuestMatrix::xy, &GuestMatrix::yx, &GuestMatrix::yy}) {
                rejected = matrices[0];
                rejected.*member = value;
                Require(ThrowsWith([&] { FT_Set_Transform_nid_postfix(face, &rejected, nullptr); },
                                   "FT_Set_Transform: a component outside the host FT_Long range is not implemented"));
                loadUnchanged();
            }
            for (auto member : {&GuestVector::x, &GuestVector::y}) {
                rejectedDelta = {};
                rejectedDelta.*member = value;
                Require(ThrowsWith([&] { FT_Set_Transform_nid_postfix(face, &matrices[1], &rejectedDelta); },
                                   "FT_Set_Transform: a component outside the host FT_Long range is not implemented"));
                loadUnchanged();
            }
        }
    }
    rejected.xx = std::numeric_limits<std::int64_t>::max();
    rejectedDelta.x = std::numeric_limits<std::int64_t>::min();
    FT_Set_Transform_nid_postfix(nullptr, &rejected, &rejectedDelta);
    FT_Set_Transform_nid_postfix(nullptr, nullptr, nullptr);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(native) == 0);
    std::printf("FT_Set_Transform: %zu native glyph and bitmap comparisons\n", comparisons);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_guest_freetype-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "app0");
    CheckGasp();
    const std::vector<unsigned char> font = SquareGlyphFont('A', 700);
    {
        std::ofstream file(root / "app0" / "font.ttf", std::ios::binary);
        file.write(reinterpret_cast<const char*>(font.data()), static_cast<std::streamsize>(font.size()));
    }
    const auto previous = std::filesystem::current_path();
    CheckCidMetadata();
    std::filesystem::current_path(root);
    CheckOutlineOrientation();

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
    Require(FT_Done_FreeType_nid_postfix(nullptr) == FT_Err_Invalid_Library_Handle);
    Require(FT_Done_Face_nid_postfix(nullptr) == FT_Err_Invalid_Face_Handle);

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

    const auto fontSize = static_cast<std::int64_t>(font.size());
    GuestFaceRec* memoryFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, nullptr, fontSize, 0, &memoryFace) == FT_Err_Invalid_Argument);
    Require(FT_New_Memory_Face_nid_postfix(nullptr, font.data(), fontSize, 0, &memoryFace) == FT_Err_Invalid_Library_Handle);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, 0, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, -1, nullptr) == 0);
    FT_Face nativeProbe = nullptr;
    GuestFaceRec* probe = nullptr;
    Require(FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), -1, &nativeProbe) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, -1, &probe) == 0 && probe != nullptr);
    Require(probe->num_faces == nativeProbe->num_faces && probe->face_index == nativeProbe->face_index);
    Require((probe->glyph == nullptr) == (nativeProbe->glyph == nullptr) && (probe->size == nullptr) == (nativeProbe->size == nullptr));
    Require(FT_Set_Char_Size_nid_postfix(probe, std::numeric_limits<std::int64_t>::max(), 0, 0, 0) ==
            FT_Set_Char_Size(nativeProbe, 64, 64, 72, 72));
    Finalizers nativeProbeEvents;
    Finalizers guestProbeEvents;
    nativeProbe->generic = {&nativeProbeEvents, NativeFaceFinalizer};
    probe->generic = {&guestProbeEvents, GuestFaceFinalizer};
    Require(FT_Done_Face(nativeProbe) == 0 && FT_Done_Face_nid_postfix(probe) == 0);
    Require(nativeProbeEvents.calls == std::vector<int>({3}) && guestProbeEvents.calls == nativeProbeEvents.calls);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, 1, &memoryFace) == secondFace && memoryFace == nullptr);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, 0x80000001ll, &memoryFace) == secondFace && memoryFace == nullptr);
    FT_Face invalidHost = nullptr;
    for (const FT_Long length : {0L, 4L}) {
        const FT_Error expected = FT_New_Memory_Face(hostLibrary, font.data(), length, 0, &invalidHost);
        Require(expected != 0 && invalidHost == nullptr);
        Require(FT_New_Memory_Face_nid_postfix(library, font.data(), length, 0, &memoryFace) == expected && memoryFace == nullptr);
    }
    const unsigned char invalidFont[32]{};
    const FT_Error invalidFormat = FT_New_Memory_Face(hostLibrary, invalidFont, sizeof(invalidFont), 0, &invalidHost);
    Require(invalidFormat != 0 && invalidHost == nullptr);
    Require(FT_New_Memory_Face_nid_postfix(library, invalidFont, sizeof(invalidFont), 0, &memoryFace) == invalidFormat && memoryFace == nullptr);
    if constexpr (sizeof(FT_Long) < sizeof(std::int64_t)) {
        for (const std::int64_t length : {0x80000000ll, -0x80000001ll}) {
            Require(ThrowsWith([&] { FT_New_Memory_Face_nid_postfix(library, font.data(), length, 0, &memoryFace); },
                               "FT_New_Memory_Face: a font buffer size outside the host FT_Long range is not implemented"));
            Require(memoryFace == nullptr);
        }
    }
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), fontSize, 0x180000000ll, &memoryFace) == 0 && memoryFace != nullptr);
    FaceMatches(memoryFace, host, library);
    Require(memoryFace != face && memoryFace->glyph != face->glyph);
    Require(FT_Set_Pixel_Sizes_nid_postfix(memoryFace, 0, 100) == 0 && FT_Set_Pixel_Sizes(host, 0, 100) == 0);
    Require(FT_Get_Char_Index_nid_postfix(memoryFace, 'A') == 1);
    Require(FT_Load_Glyph_nid_postfix(memoryFace, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(host, 1, FT_LOAD_DEFAULT) == 0);
    SlotMatches(memoryFace->glyph, host->glyph);
    Require(FT_Render_Glyph_nid_postfix(memoryFace->glyph, FT_RENDER_MODE_NORMAL) == 0 && FT_Render_Glyph(host->glyph, FT_RENDER_MODE_NORMAL) == 0);
    SlotMatches(memoryFace->glyph, host->glyph);
    Require(face->size->metrics.y_ppem == 0);

    Require(FT_Set_Pixel_Sizes_nid_postfix(nullptr, 0, 100) == FT_Err_Invalid_Face_Handle);
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 100) == 0 && FT_Set_Pixel_Sizes(host, 0, 100) == 0);
    const auto& metrics = host->size->metrics;
    const auto& guestMetrics = face->size->metrics;
    Require(guestMetrics.x_ppem == 100 && guestMetrics.y_ppem == 100 && guestMetrics.x_scale == metrics.x_scale && guestMetrics.y_scale == metrics.y_scale);
    Require(guestMetrics.ascender == metrics.ascender && guestMetrics.descender == metrics.descender && guestMetrics.height == metrics.height && guestMetrics.max_advance == metrics.max_advance);

    GuestVector kerning{123, 456};
    Require(FT_Get_Kerning_nid_postfix(nullptr, 1, 1, FT_KERNING_DEFAULT, &kerning) == FT_Err_Invalid_Face_Handle);
    Require(kerning.x == 123 && kerning.y == 456);
    Require(FT_Get_Kerning_nid_postfix(nullptr, 1, 1, FT_KERNING_DEFAULT, nullptr) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_Kerning_nid_postfix(face, 1, 1, FT_KERNING_DEFAULT, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Get_Kerning_nid_postfix(face, 1, 1, FT_KERNING_DEFAULT, &kerning) == 0);
    Require(kerning.x == 0 && kerning.y == 0);

    auto kernFont = font;
    const std::uint32_t kernOffset = static_cast<std::uint32_t>(kernFont.size()) + 16;
    kernFont[5] = 8;
    kernFont[7] = 128;
    kernFont[9] = 3;
    kernFont[11] = 0;
    for (std::size_t entry = 12; entry < 12 + 7 * 16; entry += 16) {
        const std::uint32_t offset = (static_cast<std::uint32_t>(kernFont[entry + 8]) << 24) |
            (static_cast<std::uint32_t>(kernFont[entry + 9]) << 16) |
            (static_cast<std::uint32_t>(kernFont[entry + 10]) << 8) | kernFont[entry + 11];
        std::vector<unsigned char> adjusted;
        Put32(adjusted, offset + 16);
        std::memcpy(kernFont.data() + entry + 8, adjusted.data(), 4);
    }
    std::vector<unsigned char> kernEntry{'k', 'e', 'r', 'n'};
    Put32(kernEntry, 0);
    Put32(kernEntry, kernOffset);
    Put32(kernEntry, 24);
    kernFont.insert(kernFont.begin() + 12 + 5 * 16, kernEntry.begin(), kernEntry.end());
    PutAll16(kernFont, {0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 1, -73});
    {
        std::ofstream file(root / "app0" / "kern.ttf", std::ios::binary);
        file.write(reinterpret_cast<const char*>(kernFont.data()), static_cast<std::streamsize>(kernFont.size()));
    }
    FT_Face kernHost = nullptr;
    GuestFaceRec* kernFace = nullptr;
    Require(FT_New_Memory_Face(hostLibrary, kernFont.data(), static_cast<FT_Long>(kernFont.size()), 0, &kernHost) == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/kern.ttf", 0, &kernFace) == 0);
    Require(FT_HAS_KERNING(kernHost));
    const unsigned modes[] = {FT_KERNING_DEFAULT, FT_KERNING_UNFITTED, FT_KERNING_UNSCALED, 99};
    for (const unsigned size : {100u, 12u}) {
        Require(FT_Set_Pixel_Sizes_nid_postfix(kernFace, 0, size) == 0 && FT_Set_Pixel_Sizes(kernHost, 0, size) == 0);
        for (const unsigned mode : modes) {
            FT_Vector expected{};
            Require(FT_Get_Kerning(kernHost, 1, 1, mode, &expected) == 0);
            kerning = {123, 456};
            Require(FT_Get_Kerning_nid_postfix(kernFace, 1, 1, mode, &kerning) == 0);
            Require(kerning.x == expected.x && kerning.y == expected.y && kerning.y == 0);
            if (mode == FT_KERNING_UNSCALED) Require(kerning.x == -73);
            if (size == 100 && mode == FT_KERNING_DEFAULT) Require(kerning.x == -448);
            if (size == 100 && mode == FT_KERNING_UNFITTED) Require(kerning.x == -467);
            for (const unsigned leftGlyph : {0x10001u, 0x7FFF0001u}) {
                kerning = {123, 456};
                Require(FT_Get_Kerning_nid_postfix(kernFace, leftGlyph, 1, mode, &kerning) == 0);
                Require(kerning.x == 0 && kerning.y == 0);
            }
            kerning = {123, 456};
            Require(FT_Get_Kerning_nid_postfix(kernFace, 1, 0x10001u, mode, &kerning) == 0);
            Require(kerning.x == expected.x && kerning.y == expected.y);
        }
    }
    for (const unsigned glyph : {0u, 2u, 0xFFFFFFFFu}) {
        kerning = {123, 456};
        Require(FT_Get_Kerning_nid_postfix(kernFace, glyph, 1, FT_KERNING_UNSCALED, &kerning) == 0);
        Require(kerning.x == 0 && kerning.y == 0);
        kerning = {123, 456};
        Require(FT_Get_Kerning_nid_postfix(kernFace, 1, glyph, FT_KERNING_UNSCALED, &kerning) == 0);
        Require(kerning.x == 0 && kerning.y == 0);
    }
    FT_Done_Face(kernHost);

    Require(FT_Get_Char_Index_nid_postfix(face, 'A') == 1 && FT_Get_Char_Index_nid_postfix(face, 'B') == 0);
    Require(FT_Get_Char_Index_nid_postfix(face, 0x100000041ull) == 1);
    Require(FT_Get_Char_Index_nid_postfix(nullptr, 'A') == 0);

    Require(FT_Load_Char_nid_postfix(nullptr, 'A', FT_LOAD_DEFAULT) == FT_Err_Invalid_Face_Handle);
    for (const std::uint64_t code : std::array<std::uint64_t, 4>{'A', 'B', 0x100000041ull, 0xFFFFFFFFFFFFFFFFull}) {
        for (const std::int32_t flags : std::array<std::int32_t, 4>{FT_LOAD_DEFAULT, FT_LOAD_NO_SCALE, FT_LOAD_NO_HINTING, FT_LOAD_RENDER}) {
            Require(FT_Load_Char_nid_postfix(face, code, flags) == FT_Load_Char(host, static_cast<FT_ULong>(code), flags));
            SlotMatches(face->glyph, host->glyph);
            Require(face->glyph->glyph_index == (static_cast<std::uint32_t>(code) == 'A' ? 1u : 0u));
        }
    }
    {
        auto noMap = font;
        const std::size_t tableCount = (static_cast<std::size_t>(noMap[4]) << 8) | noMap[5];
        bool renamed = false;
        for (std::size_t offset = 12; offset < 12 + tableCount * 16; offset += 16) {
            if (std::memcmp(noMap.data() + offset, "cmap", 4) != 0) continue;
            std::memcpy(noMap.data() + offset, "none", 4);
            renamed = true;
            break;
        }
        Require(renamed);
        {
            std::ofstream file(root / "app0" / "no-map.ttf", std::ios::binary);
            file.write(reinterpret_cast<const char*>(noMap.data()), static_cast<std::streamsize>(noMap.size()));
            Require(file.good());
        }
        GuestFaceRec* guestNoMap = nullptr;
        FT_Face nativeNoMap = nullptr;
        Require(FT_New_Face_nid_postfix(library, "/app0/no-map.ttf", 0, &guestNoMap) == 0);
        Require(FT_New_Memory_Face(hostLibrary, noMap.data(), static_cast<FT_Long>(noMap.size()), 0, &nativeNoMap) == 0);
        Require(guestNoMap->charmap == nullptr && nativeNoMap->charmap == nullptr);
        Require(FT_Set_Pixel_Sizes_nid_postfix(guestNoMap, 0, 100) == 0 && FT_Set_Pixel_Sizes(nativeNoMap, 0, 100) == 0);
        for (const std::uint64_t code : {1ull, 2ull}) {
            for (const std::int32_t flags : std::array<std::int32_t, 2>{FT_LOAD_DEFAULT, FT_LOAD_RENDER}) {
                const FT_Error expected = FT_Load_Char(nativeNoMap, static_cast<FT_ULong>(code), flags);
                Require((code == 1) == (expected == 0));
                Require(FT_Load_Char_nid_postfix(guestNoMap, code, flags) == expected);
                SlotMatches(guestNoMap->glyph, nativeNoMap->glyph);
            }
        }
        Require(FT_Done_Face(nativeNoMap) == 0);
    }

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

    {
        struct Request {
            std::int64_t width;
            std::int64_t height;
            std::uint32_t horizontal;
            std::uint32_t vertical;
        };
        const std::array requests{
            Request{0, 0, 0, 0}, Request{0, 12 * 64, 0, 0}, Request{12 * 64, 0, 0, 0},
            Request{12 * 64, 18 * 64, 96, 144}, Request{12 * 64 + 32, 12 * 64 + 32, 72, 72},
            Request{1, 63, 72, 72}, Request{-1, -100, 0, 0}, Request{0, -100, 96, 0},
            Request{12 * 64, 18 * 64, 0, 144}, Request{12 * 64, 18 * 64, 96, 0},
            Request{65535ll * 64, 64, 72, 72}, Request{65536ll * 64, 64, 72, 72},
            Request{64, 65536ll * 64, 72, 72},
            Request{18 * 64, 12 * 64, 72, 72}
        };
        std::size_t successes = 0;
        std::size_t failures = 0;
        for (const auto& request : requests) {
            std::array<unsigned char, sizeof(GuestGlyphSlotRec)> previousSlot{};
            std::memcpy(previousSlot.data(), face->glyph, previousSlot.size());
            const auto expected = FT_Set_Char_Size(host, static_cast<FT_F26Dot6>(request.width), static_cast<FT_F26Dot6>(request.height),
                                                   request.horizontal, request.vertical);
            Require(FT_Set_Char_Size_nid_postfix(face, request.width, request.height, request.horizontal, request.vertical) == expected);
            Require(std::memcmp(previousSlot.data(), face->glyph, previousSlot.size()) == 0);
            expected == 0 ? ++successes : ++failures;
            SizeMatches(face, host);
            Require(FT_Load_Char_nid_postfix(face, 'A', FT_LOAD_DEFAULT) == FT_Load_Char(host, 'A', FT_LOAD_DEFAULT));
            SlotMatches(face->glyph, host->glyph);
        }
        Require(successes != 0 && failures != 0);
        for (const auto value : std::array<std::int64_t, 2>{std::numeric_limits<std::int64_t>::min(), -0x80000001ll}) {
            Require(FT_Set_Char_Size_nid_postfix(face, value, 0, 0, 0) == FT_Set_Char_Size(host, 64, 64, 0, 0));
            SizeMatches(face, host);
            Require(FT_Set_Char_Size_nid_postfix(face, 0, value, 0, 0) == 0);
            SizeMatches(face, host);
        }
        for (const auto value : {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
            Require(FT_Set_Char_Size_nid_postfix(nullptr, value, value, 0, 0) == FT_Err_Invalid_Face_Handle);
        }
        if constexpr (sizeof(FT_F26Dot6) < sizeof(std::int64_t)) {
            for (const auto value : std::array<std::int64_t, 4>{0x7FFFFFFFll, 0x80000000ll, 0x100000040ll, std::numeric_limits<std::int64_t>::max()}) {
                for (const bool width : {false, true}) {
                    Require(ThrowsWith([&] { FT_Set_Char_Size_nid_postfix(face, width ? value : 64, width ? 64 : value, 72, 72); },
                                       "FT_Set_Char_Size: a character size or resolution outside the host arithmetic range is not implemented"));
                    SizeMatches(face, host);
                }
            }
            Require(ThrowsWith([&] { FT_Set_Char_Size_nid_postfix(face, 64, 64, std::numeric_limits<std::uint32_t>::max(), 72); },
                               "FT_Set_Char_Size: a character size or resolution outside the host arithmetic range is not implemented"));
            SizeMatches(face, host);
        }
        Require(FT_Set_Char_Size_nid_postfix(memoryFace, 0, 48 * 64, 0, 144) == 0);
        Require(memoryFace->size->metrics.y_ppem == 96 && face->size->metrics.y_ppem == 1);
        Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 100) == 0 && FT_Set_Pixel_Sizes(host, 0, 100) == 0);
        std::printf("FT_Set_Char_Size: %zu native request comparisons, %zu successes, %zu errors\n", requests.size(), successes, failures);
    }

    Finalizers nativeDone;
    Finalizers guestDone;
    SetFinalizers(host, nativeDone);
    SetFinalizers(face, guestDone);
    Require(FT_Done_Face(host) == 0);
    Require(FT_Done_Face_nid_postfix(face) == 0);
    Require(nativeDone.calls == std::vector<int>({1, 2, 3}) && guestDone.calls == nativeDone.calls);
    Require(FT_Get_Char_Index_nid_postfix(masked, 'A') == 1);

    FT_Face nativeFirst = nullptr;
    FT_Face nativeSecond = nullptr;
    Require(FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFirst) == 0);
    Require(FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeSecond) == 0);
    Finalizers nativeFirstEvents;
    Finalizers nativeSecondEvents;
    Finalizers guestFirstEvents;
    Finalizers guestSecondEvents;
    SetFinalizers(nativeFirst, nativeFirstEvents);
    SetFinalizers(nativeSecond, nativeSecondEvents);
    SetFinalizers(masked, guestFirstEvents);
    SetFinalizers(memoryFace, guestSecondEvents);
    Require(FT_Done_FreeType(hostLibrary) == 0);

    void* independent = nullptr;
    GuestFaceRec* independentFace = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&independent) == 0);
    Require(FT_New_Memory_Face_nid_postfix(independent, font.data(), fontSize, 0, &independentFace) == 0);
    Finalizers independentEvents;
    SetFinalizers(independentFace, independentEvents);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    Require(guestFirstEvents.calls == nativeFirstEvents.calls && guestSecondEvents.calls == nativeSecondEvents.calls);
    Require(guestFirstEvents.calls == nativeDone.calls && independentEvents.calls.empty());
    Require(FT_Get_Char_Index_nid_postfix(independentFace, 'A') == 1);
    Require(FT_Done_FreeType_nid_postfix(independent) == 0 && independentEvents.calls == nativeDone.calls);

    void* empty = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&empty) == 0 && FT_Done_FreeType_nid_postfix(empty) == 0);
    CheckTransforms(font);
    std::filesystem::current_path(previous);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
