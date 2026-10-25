#include "prx/libSceFreeType/include/Api.hpp"
#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GASP_H
#include FT_OUTLINE_H
#include FT_TRIGONOMETRY_H
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CheckGaspTable(const std::vector<unsigned char>* table, const std::vector<std::pair<unsigned, int>>& ranges,
                           bool versionZero, bool render) {
    const auto font = SquareGlyphFont('A', 700, false, table);
    FreeType::GuestLibrary* library = nullptr;
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

static void FaceMatches(const GuestFaceRec* face, FT_Face host, const FreeType::GuestLibrary* library) {
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
    FreeType::GuestLibrary* library = nullptr;
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

static void Trigonometry() {
    constexpr std::int64_t circle = 360ll << 16;
    const std::array<std::pair<std::int64_t, std::int64_t>, 68> poleTangents{{
        {-17694728ll, 33692373208ll},
        {-17694727ll, 45725363639ll},
        {-17694726ll, 45725363639ll},
        {-17694725ll, 64015509094ll},
        {-17694724ll, 64015509094ll},
        {-17694723ll, 128031018189ll},
        {-17694722ll, 128031018189ll},
        {-17694721ll, 640155090944ll},
        {-17694720ll, -640155090944ll},
        {-17694719ll, -128031018189ll},
        {-17694718ll, -128031018189ll},
        {-17694717ll, -64015509094ll},
        {-17694716ll, -64015509094ll},
        {-17694715ll, -45725363639ll},
        {-17694714ll, -45725363639ll},
        {-17694713ll, -33692373208ll},
        {-17694712ll, -33692373208ll},
        {-5898248ll, 33692373208ll},
        {-5898247ll, 45725363639ll},
        {-5898246ll, 45725363639ll},
        {-5898245ll, 64015509094ll},
        {-5898244ll, 64015509094ll},
        {-5898243ll, 128031018189ll},
        {-5898242ll, 128031018189ll},
        {-5898241ll, 640155090944ll},
        {-5898240ll, -640155090944ll},
        {-5898239ll, -128031018189ll},
        {-5898238ll, -128031018189ll},
        {-5898237ll, -64015509094ll},
        {-5898236ll, -64015509094ll},
        {-5898235ll, -45725363639ll},
        {-5898234ll, -45725363639ll},
        {-5898233ll, -33692373208ll},
        {-5898232ll, -33692373208ll},
        {5898232ll, 33692373208ll},
        {5898233ll, 45725363639ll},
        {5898234ll, 45725363639ll},
        {5898235ll, 64015509094ll},
        {5898236ll, 64015509094ll},
        {5898237ll, 128031018189ll},
        {5898238ll, 128031018189ll},
        {5898239ll, 640155090944ll},
        {5898240ll, -640155090944ll},
        {5898241ll, -128031018189ll},
        {5898242ll, -128031018189ll},
        {5898243ll, -64015509094ll},
        {5898244ll, -64015509094ll},
        {5898245ll, -45725363639ll},
        {5898246ll, -45725363639ll},
        {5898247ll, -33692373208ll},
        {5898248ll, -33692373208ll},
        {17694712ll, 33692373208ll},
        {17694713ll, 45725363639ll},
        {17694714ll, 45725363639ll},
        {17694715ll, 64015509094ll},
        {17694716ll, 64015509094ll},
        {17694717ll, 128031018189ll},
        {17694718ll, 128031018189ll},
        {17694719ll, 640155090944ll},
        {17694720ll, -640155090944ll},
        {17694721ll, -128031018189ll},
        {17694722ll, -128031018189ll},
        {17694723ll, -64015509094ll},
        {17694724ll, -64015509094ll},
        {17694725ll, -45725363639ll},
        {17694726ll, -45725363639ll},
        {17694727ll, -33692373208ll},
        {17694728ll, -33692373208ll}}};
    int scalarCases = 0;
    int vectorCases = 0;
    int differenceCases = 0;
    for (const auto& [angle, tangent] : poleTangents) {
        Require(FT_Tan_nid_postfix(angle) == tangent);
        if (sizeof(FT_Fixed) == 4) {
            Require(static_cast<std::int64_t>(FT_Tan(static_cast<FT_Angle>(angle))) != tangent);
        } else {
            Require(static_cast<std::int64_t>(FT_Tan(static_cast<FT_Angle>(angle))) == tangent);
        }
    }
    auto compare = [&](std::int64_t angle, FT_Angle nativeAngle, std::int64_t expectedTangent) {
        Require(FT_Cos_nid_postfix(angle) == FT_Cos(nativeAngle));
        Require(FT_Sin_nid_postfix(angle) == FT_Sin(nativeAngle));
        Require(FT_Tan_nid_postfix(angle) == expectedTangent);
        FT_Vector expected{};
        FT_Vector_Unit(&expected, nativeAngle);
        struct GuardedVector {
            std::uint64_t before;
            GuestVector vector;
            std::uint64_t after;
        } actual{0xA55A0FF0FF00AA55ull, {INT64_MAX, INT64_MIN}, 0x55AA00FFF00F5AA5ull};
        FT_Vector_Unit_nid_postfix(&actual.vector, angle);
        Require(actual.vector.x == expected.x && actual.vector.y == expected.y);
        Require(actual.vector.x == FT_Cos_nid_postfix(angle) && actual.vector.y == FT_Sin_nid_postfix(angle));
        Require(actual.before == 0xA55A0FF0FF00AA55ull && actual.after == 0x55AA00FFF00F5AA5ull);
        ++scalarCases;
        ++vectorCases;
    };
    std::mt19937 random(0x17A693Bu);
    std::uniform_int_distribution<std::int32_t> angles(INT32_MIN, INT32_MAX);
    for (unsigned sample = 0; sample < 4096; ++sample) {
        const std::int32_t angle = angles(random);
        Require(FT_Cos_nid_postfix(angle) == FT_Cos(angle));
        Require(FT_Sin_nid_postfix(angle) == FT_Sin(angle));
        Require(static_cast<std::int32_t>(FT_Tan_nid_postfix(angle)) == FT_Tan(angle));
        FT_Vector expected{};
        GuestVector actual{};
        FT_Vector_Unit(&expected, angle);
        FT_Vector_Unit_nid_postfix(&actual, angle);
        Require(actual.x == expected.x && actual.y == expected.y);
        ++scalarCases;
        ++vectorCases;
    }
    const std::array<std::int64_t, 14> bases{-circle + 1, -(180ll << 16), -(90ll << 16) - 1, -(45ll << 16),
        -1, 0, 1, 25783, 45ll << 16, (90ll << 16) - 1, 90ll << 16, (90ll << 16) + 1, 180ll << 16, circle - 1};
    for (const std::int64_t turns : {0ll, 1ll, -1ll, 97ll, -97ll, 100000000000ll, -100000000000ll, 390000000000ll, -390000000000ll}) {
        for (const std::int64_t base : bases) {
            const std::int64_t tangent = FT_Tan_nid_postfix(base);
            compare(base + circle * turns, static_cast<FT_Angle>(base), tangent);
            for (const std::int64_t other : bases) {
                Require(FT_Angle_Diff_nid_postfix(base + circle * turns, other + circle * turns) ==
                        FT_Angle_Diff(static_cast<FT_Angle>(base), static_cast<FT_Angle>(other)));
                ++differenceCases;
            }
        }
    }
    for (const auto boundary : std::array<std::pair<std::int64_t, std::int64_t>, 4>{
             std::pair{INT64_MIN, -16252928ll}, std::pair{INT64_MAX, 16252927ll},
             std::pair{INT64_MIN + 1, -16252927ll}, std::pair{INT64_MAX - 1, 16252926ll}}) {
        Require(FT_Cos_nid_postfix(boundary.first) == FT_Cos(static_cast<FT_Angle>(boundary.second)));
        Require(FT_Sin_nid_postfix(boundary.first) == FT_Sin(static_cast<FT_Angle>(boundary.second)));
        Require(FT_Tan_nid_postfix(boundary.first) == FT_Tan_nid_postfix(boundary.second));
        FT_Vector expected{};
        GuestVector actual{};
        FT_Vector_Unit(&expected, static_cast<FT_Angle>(boundary.second));
        FT_Vector_Unit_nid_postfix(&actual, boundary.first);
        Require(actual.x == expected.x && actual.y == expected.y);
        ++scalarCases;
        ++vectorCases;
    }
    FT_Vector_Unit_nid_postfix(nullptr, INT64_MIN);
    Require(FT_Angle_Diff_nid_postfix(INT64_MIN, INT64_MIN + 1) == 1);
    Require(FT_Angle_Diff_nid_postfix(INT64_MAX, INT64_MAX - 1) == -1);
    Require(FT_Angle_Diff_nid_postfix(INT64_MIN, INT64_MIN) == 0);
    Require(FT_Angle_Diff_nid_postfix(INT64_MAX, INT64_MAX) == 0);
    for (const auto pair : std::array<std::pair<std::int64_t, std::int64_t>, 4>{
             std::pair{INT64_MIN, INT64_MAX}, std::pair{INT64_MAX, INT64_MIN}, std::pair{INT64_MIN, 0ll}, std::pair{0ll, INT64_MIN}}) {
        if (pair.first == 0) {
            Require(FT_Angle_Diff_nid_postfix(pair.first, pair.second) == FT_Angle_Diff_nid_postfix(0, -16252928));
        } else {
            Require(ThrowsWith([&] { FT_Angle_Diff_nid_postfix(pair.first, pair.second); }, "FT_Angle_Diff: overflowing signed angle subtraction"));
        }
    }
    const auto font = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    FT_Library oracle = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Face native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&oracle) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face(oracle, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    int rendered = 0;
    for (const unsigned pixels : {32u, 64u}) {
        Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, pixels) == FT_Set_Pixel_Sizes(native, 0, pixels));
        for (const FT_Angle angle : {0L, 30L << 16, 45L << 16, 90L << 16, 135L << 16}) {
            Require(FT_Load_Char_nid_postfix(face, 'A', FT_LOAD_DEFAULT) == FT_Load_Char(native, 'A', FT_LOAD_DEFAULT));
            GuestVector unit{};
            FT_Vector_Unit_nid_postfix(&unit, angle + circle * 100000000000ll);
            for (unsigned point = 0; point < face->glyph->outline.n_points; ++point) {
                auto& value = face->glyph->outline.points[point];
                const auto x = static_cast<FT_Long>(value.x);
                const auto y = static_cast<FT_Long>(value.y);
                value = {FT_MulFix(x, static_cast<FT_Long>(unit.x)) - FT_MulFix(y, static_cast<FT_Long>(unit.y)),
                         FT_MulFix(x, static_cast<FT_Long>(unit.y)) + FT_MulFix(y, static_cast<FT_Long>(unit.x))};
            }
            FT_Matrix matrix{FT_Cos(angle), -FT_Sin(angle), FT_Sin(angle), FT_Cos(angle)};
            FT_Outline_Transform(&native->glyph->outline, &matrix);
            Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL));
            SlotMatches(face->glyph, native->glyph);
            ++rendered;
        }
    }
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(oracle) == 0);
    std::printf("FreeType trigonometry: %d scalar cases, %d vectors, %d angle differences, 68 LP64 tangent goldens, %d rendered glyphs\n",
                scalarCases, vectorCases, differenceCases, rendered);
}

struct DecomposeEvents {
    std::vector<std::array<std::int64_t, 7>> calls;
    std::size_t stopAt = static_cast<std::size_t>(-1);
    int result = 0;

    int Add(int kind, GuestVector first, GuestVector second = {}, GuestVector third = {}) {
        calls.push_back({kind, first.x, first.y, second.x, second.y, third.x, third.y});
        return calls.size() - 1 == stopAt ? result : 0;
    }
};

static GuestVector GuestPoint(const FT_Vector& point) { return {point.x, point.y}; }

static int NativeMove(const FT_Vector* to, void* user) { return static_cast<DecomposeEvents*>(user)->Add(1, GuestPoint(*to)); }
static int NativeLine(const FT_Vector* to, void* user) { return static_cast<DecomposeEvents*>(user)->Add(2, GuestPoint(*to)); }
static int NativeConic(const FT_Vector* control, const FT_Vector* to, void* user) {
    return static_cast<DecomposeEvents*>(user)->Add(3, GuestPoint(*control), GuestPoint(*to));
}
static int NativeCubic(const FT_Vector* first, const FT_Vector* second, const FT_Vector* to, void* user) {
    return static_cast<DecomposeEvents*>(user)->Add(4, GuestPoint(*first), GuestPoint(*second), GuestPoint(*to));
}

static int APS5_VABI GuestMove(const GuestVector* to, void* user) { return static_cast<DecomposeEvents*>(user)->Add(1, *to); }
static int APS5_VABI GuestLine(const GuestVector* to, void* user) { return static_cast<DecomposeEvents*>(user)->Add(2, *to); }
static int APS5_VABI GuestConic(const GuestVector* control, const GuestVector* to, void* user) {
    return static_cast<DecomposeEvents*>(user)->Add(3, *control, *to);
}
static int APS5_VABI GuestCubic(const GuestVector* first, const GuestVector* second, const GuestVector* to, void* user) {
    return static_cast<DecomposeEvents*>(user)->Add(4, *first, *second, *to);
}

static int APS5_VABI NullUserMove(const GuestVector*, void* user) {
    Require(user == nullptr);
    return -17;
}

static std::size_t CompareDecomposition(std::vector<GuestVector> points, std::vector<unsigned char> tags,
                                        std::vector<unsigned short> contours, int shift, std::int64_t delta,
                                        std::size_t stopAt = static_cast<std::size_t>(-1), int stopResult = 0) {
    std::vector<FT_Vector> nativePoints;
    for (const auto point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    GuestOutline outline{static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(points.size()), points.data(), tags.data(), contours.data(), 0};
    FT_Outline native{outline.n_contours, outline.n_points, nativePoints.data(), tags.data(), contours.data(), 0};
    DecomposeEvents actual;
    DecomposeEvents expected;
    actual.stopAt = expected.stopAt = stopAt;
    actual.result = expected.result = stopResult;
    const GuestOutlineFuncs functions{GuestMove, GuestLine, GuestConic, GuestCubic, shift, delta};
    const FT_Outline_Funcs nativeFunctions{NativeMove, NativeLine, NativeConic, NativeCubic, shift, static_cast<FT_Pos>(delta)};
    const int result = FT_Outline_Decompose_nid_postfix(&outline, &functions, &actual);
    Require(result == FT_Outline_Decompose(&native, &nativeFunctions, &expected));
    Require(actual.calls == expected.calls);
    for (std::size_t index = 0; index < points.size(); ++index) {
        Require(points[index].x == nativePoints[index].x && points[index].y == nativePoints[index].y);
    }
    if (stopAt < expected.calls.size()) Require(result == stopResult && actual.calls.size() == stopAt + 1);
    return actual.calls.size();
}

struct ChangingDecomposition {
    DecomposeEvents events;
    GuestOutlineFuncs guest{};
    FT_Outline_Funcs native{};
    GuestOutline* outline = nullptr;
    FT_Outline* nativeOutline = nullptr;
    GuestVector* replacement = nullptr;
    FT_Vector* nativeReplacement = nullptr;
};

static int NativeChangedLine(const FT_Vector* to, void* user) {
    return static_cast<ChangingDecomposition*>(user)->events.Add(5, GuestPoint(*to));
}
static int APS5_VABI GuestChangedLine(const GuestVector* to, void* user) {
    return static_cast<ChangingDecomposition*>(user)->events.Add(5, *to);
}
static int NativeChangingMove(const FT_Vector* to, void* user) {
    auto& state = *static_cast<ChangingDecomposition*>(user);
    state.native.line_to = NativeChangedLine;
    state.native.shift = 4;
    state.native.delta = 999;
    state.nativeOutline->points = state.nativeReplacement;
    return state.events.Add(1, GuestPoint(*to));
}
static int APS5_VABI GuestChangingMove(const GuestVector* to, void* user) {
    auto& state = *static_cast<ChangingDecomposition*>(user);
    state.guest.line_to = GuestChangedLine;
    state.guest.shift = 4;
    state.guest.delta = 999;
    state.outline->points = state.replacement;
    GuestOutline empty{};
    const GuestOutlineFuncs noCallbacks{};
    Require(FT_Outline_Decompose_nid_postfix(&empty, &noCallbacks, nullptr) == 0);
    return state.events.Add(1, *to);
}

static void OutlineDecomposition() {
    GuestOutline empty{};
    const GuestOutlineFuncs callbacks{GuestMove, GuestLine, GuestConic, GuestCubic, 0, 0};
    const FT_Outline_Funcs nativeCallbacks{NativeMove, NativeLine, NativeConic, NativeCubic, 0, 0};
    Require(FT_Outline_Decompose_nid_postfix(nullptr, nullptr, nullptr) == FT_Outline_Decompose(nullptr, nullptr, nullptr));
    Require(FT_Outline_Decompose_nid_postfix(nullptr, &callbacks, nullptr) == FT_Outline_Decompose(nullptr, &nativeCallbacks, nullptr));
    Require(FT_Outline_Decompose_nid_postfix(&empty, nullptr, nullptr) == FT_Err_Invalid_Argument);
    const GuestOutlineFuncs unused{nullptr, nullptr, nullptr, nullptr, -1, INT64_MIN};
    Require(FT_Outline_Decompose_nid_postfix(&empty, &unused, nullptr) == 0);
    int scenarios = 0;
    std::size_t calls = 0;
    for (unsigned count = 1; count <= 6; ++count) {
        std::vector<GuestVector> points;
        for (unsigned point = 0; point < count; ++point) points.push_back({static_cast<std::int64_t>(point) * 13 - 37, static_cast<std::int64_t>(point) * -17 + 23});
        const unsigned patterns = 1u << (2 * count);
        for (unsigned pattern = 0; pattern < patterns; ++pattern) {
            std::vector<unsigned char> tags;
            for (unsigned point = 0; point < count; ++point) tags.push_back(static_cast<unsigned char>(((pattern >> (point * 2)) & 3) | 0xA4));
            for (const int shift : {0, 1, 4}) {
                for (const int delta : {-17, 0, 19}) {
                    calls += CompareDecomposition(points, tags, {static_cast<unsigned short>(count - 1)}, shift, delta);
                    ++scenarios;
                }
            }
        }
    }
    const std::vector<GuestVector> curve{{-17, 3}, {7, -5}, {29, 31}, {43, -11}, {59, 37}, {73, -13}, {89, 41}, {101, -17}};
    for (const auto& tags : std::vector<std::vector<unsigned char>>{{1, 0, 0, 1, 2, 2, 1, 0}, {1, 2, 2, 1, 0, 1, 2, 2}, {0, 0, 0, 0, 0, 0, 0, 0}}) {
        for (const auto& contours : std::vector<std::vector<unsigned short>>{{7}, {2, 7}, {0, 3, 7}, {2, 1}}) {
            const std::size_t events = CompareDecomposition(curve, tags, contours, 2, -3);
            ++scenarios;
            for (std::size_t stop = 0; stop < events; ++stop) {
                for (const int error : {1, -17, 0x12345678, INT32_MIN}) {
                    calls += CompareDecomposition(curve, tags, contours, 2, -3, stop, error);
                    ++scenarios;
                }
            }
        }
    }
    {
        GuestVector points[]{{7, -5}, {11, 13}, {17, -19}, {23, 29}};
        FT_Vector nativePoints[]{{7, -5}, {11, 13}, {17, -19}, {23, 29}};
        GuestVector replacement[]{{107, 95}, {111, 113}, {117, 81}, {123, 129}};
        FT_Vector nativeReplacement[]{{107, 95}, {111, 113}, {117, 81}, {123, 129}};
        unsigned char tags[]{1, 1, 1, 1};
        unsigned short contours[]{1, 3};
        GuestOutline outline{2, 4, points, tags, contours, 0};
        FT_Outline native{2, 4, nativePoints, tags, contours, 0};
        ChangingDecomposition actual;
        ChangingDecomposition expected;
        actual.guest = {GuestChangingMove, nullptr, nullptr, nullptr, 1, -3};
        expected.native = {NativeChangingMove, nullptr, nullptr, nullptr, 1, -3};
        actual.outline = &outline;
        actual.replacement = replacement;
        expected.nativeOutline = &native;
        expected.nativeReplacement = nativeReplacement;
        Require(FT_Outline_Decompose_nid_postfix(&outline, &actual.guest, &actual) == FT_Outline_Decompose(&native, &expected.native, &expected));
        Require(actual.events.calls == expected.events.calls && actual.events.calls.size() == 6);
    }
    {
        GuestVector point{7, -5};
        unsigned char tag = 1;
        unsigned short contour = 0;
        GuestOutline outline{1, 1, &point, &tag, &contour, 0};
        const GuestOutlineFuncs nullUser{NullUserMove, nullptr, nullptr, nullptr, 0, 0};
        Require(FT_Outline_Decompose_nid_postfix(&outline, &nullUser, nullptr) == -17);
    }
    {
        GuestVector points[]{{0x10000000001ll, -0x10000000003ll}, {0x10000000005ll, -0x10000000007ll}, {0x10000000009ll, -0x1000000000Bll}};
        unsigned char tags[]{1, 1, 1};
        unsigned short contours[]{1};
        GuestOutline outline{1, 2, points, tags, contours, 0};
        auto functions = callbacks;
        functions.shift = 2;
        functions.delta = 0x100000000ll;
        DecomposeEvents events;
        Require(FT_Outline_Decompose_nid_postfix(&outline, &functions, &events) == 0);
        const std::vector<std::array<std::int64_t, 7>> expected{{1, 0x3FF00000004ll, -0x4010000000Cll, 0, 0, 0, 0},
                                                            {2, 0x3FF00000014ll, -0x4010000001Cll, 0, 0, 0, 0},
                                                            {2, 0x3FF00000004ll, -0x4010000000Cll, 0, 0, 0, 0}};
        Require(events.calls == expected);
        tags[0] = tags[1] = 0;
        events.calls.clear();
        Require(FT_Outline_Decompose_nid_postfix(&outline, &functions, &events) == 0);
        const std::vector<std::array<std::int64_t, 7>> conic{{1, 0x3FF0000000Cll, -0x40100000014ll, 0, 0, 0, 0},
                                                         {3, 0x3FF00000004ll, -0x4010000000Cll, 0x3FF0000000Cll, -0x40100000014ll, 0, 0},
                                                         {3, 0x3FF00000014ll, -0x4010000001Cll, 0x3FF0000000Cll, -0x40100000014ll, 0, 0}};
        Require(events.calls == conic);
        tags[0] = 1;
        tags[1] = tags[2] = 2;
        contours[0] = 2;
        outline.n_points = 3;
        events.calls.clear();
        Require(FT_Outline_Decompose_nid_postfix(&outline, &functions, &events) == 0);
        const std::vector<std::array<std::int64_t, 7>> cubic{{1, 0x3FF00000004ll, -0x4010000000Cll, 0, 0, 0, 0},
                                                         {4, 0x3FF00000014ll, -0x4010000001Cll, 0x3FF00000024ll, -0x4010000002Cll, 0x3FF00000004ll, -0x4010000000Cll}};
        Require(events.calls == cubic);
        contours[0] = 1;
        outline.n_points = 2;
        for (const int shift : {-1, 63, INT32_MAX}) {
            functions.shift = shift;
            Require(ThrowsWith([&] { FT_Outline_Decompose_nid_postfix(&outline, &functions, &events); }, "FT_Outline_Decompose: a shift"));
        }
        functions.shift = 62;
        functions.delta = 0;
        Require(ThrowsWith([&] { FT_Outline_Decompose_nid_postfix(&outline, &functions, &events); }, "FT_Outline_Decompose: overflowing coordinate scaling"));
        functions.shift = 0;
        functions.delta = INT64_MIN;
        Require(ThrowsWith([&] { FT_Outline_Decompose_nid_postfix(&outline, &functions, &events); }, "FT_Outline_Decompose: overflowing coordinate translation"));
        functions.delta = 0;
        points[0] = {INT64_MAX, 0};
        points[1] = {INT64_MAX, 0};
        tags[0] = tags[1] = 0;
        Require(ThrowsWith([&] { FT_Outline_Decompose_nid_postfix(&outline, &functions, &events); }, "FT_Outline_Decompose: overflowing midpoint"));
    }
    const auto font = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    FT_Library oracle = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Face native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&oracle) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face(oracle, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    int rendered = 0;
    for (const unsigned pixels : {8u, 16u, 31u, 100u}) {
        Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, pixels) == FT_Set_Pixel_Sizes(native, 0, pixels));
        for (const unsigned glyph : {0u, 1u}) {
            Require(FT_Load_Glyph_nid_postfix(face, glyph, FT_LOAD_DEFAULT) == FT_Load_Glyph(native, glyph, FT_LOAD_DEFAULT));
            DecomposeEvents actual;
            DecomposeEvents expected;
            Require(FT_Outline_Decompose_nid_postfix(&face->glyph->outline, &callbacks, &actual) == FT_Outline_Decompose(&native->glyph->outline, &nativeCallbacks, &expected));
            Require(actual.calls == expected.calls);
            SlotMatches(face->glyph, native->glyph);
            Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL));
            SlotMatches(face->glyph, native->glyph);
            ++rendered;
        }
    }
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(oracle) == 0);
    std::printf("FreeType outline decomposition: %d native comparisons, %zu callbacks, %d rendered glyphs, wide coordinates and callback mutation\n", scenarios, calls, rendered);
}

struct Finalizers {
    std::vector<int> calls;
    bool reenter = false;
};

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
        FreeType::GuestLibrary* library = nullptr;
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
    auto* events = static_cast<Finalizers*>(face->generic.data);
    events->calls.push_back(3);
    if (events->reenter) {
        Require(FT_Reference_Face(face) == 0 && FT_Reference_Face(face) == 0);
        Require(FT_Done_Face(face) == 0);
        Require(face->glyph == nullptr && face->size == nullptr);
    }
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
    auto* events = static_cast<Finalizers*>(face->generic.data);
    events->calls.push_back(3);
    if (events->reenter) {
        const auto* library = static_cast<const GuestFace*>(face->internal)->library;
        Require(FT_Reference_Face_nid_postfix(face) == 0 && FT_Reference_Face_nid_postfix(face) == 0);
        Require(FT_Done_Face_nid_postfix(face) == 0 && library->faces.size() == 1);
        Require(face->glyph == nullptr && face->size == nullptr);
    }
}

static std::string BitmapStrikeFont(int width, int height, int descent) {
    std::string font = "STARTFONT 2.1\nFONT -anyps5-strike-medium-r-normal--" + std::to_string(height) + "-0-72-72-c-0-iso10646-1\n";
    font += "SIZE " + std::to_string(height) + " 72 72\nFONTBOUNDINGBOX " + std::to_string(width) + " " + std::to_string(height) + " 0 -" + std::to_string(descent);
    font += "\nSTARTPROPERTIES 4\nFONT_ASCENT " + std::to_string(height - descent) + "\nFONT_DESCENT " + std::to_string(descent);
    font += "\nCHARSET_REGISTRY \"ISO10646\"\nCHARSET_ENCODING \"1\"\nENDPROPERTIES\nCHARS 1\nSTARTCHAR A\nENCODING 65\nSWIDTH 1000 0\nDWIDTH ";
    font += std::to_string(width) + " 0\nBBX " + std::to_string(width) + " " + std::to_string(height) + " 0 -" + std::to_string(descent) + "\nBITMAP\n";
    for (int y = 0; y < height; ++y) {
        for (int byte = 0; byte < (width + 7) / 8; ++byte) {
            unsigned value = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = byte * 8 + bit;
                if (x < width && (x == 0 || y == height - 1 || (x + y) % 3 == 0)) value |= 0x80u >> bit;
            }
            font += "0123456789ABCDEF"[value >> 4];
            font += "0123456789ABCDEF"[value & 15];
        }
        font += '\n';
    }
    return font + "ENDCHAR\nENDFONT\n";
}

static void CheckStrikeSelection(GuestLibrary* library, FT_Library nativeLibrary, GuestFaceRec* scalable, FT_Face nativeScalable) {
    const auto sizeMatches = [](const GuestSizeRec* size, FT_Size host) {
        const auto& actual = size->metrics;
        const auto& expected = host->metrics;
        Require(actual.x_ppem == expected.x_ppem && actual.y_ppem == expected.y_ppem);
        Require(actual.x_scale == expected.x_scale && actual.y_scale == expected.y_scale);
        Require(actual.ascender == expected.ascender && actual.descender == expected.descender);
        Require(actual.height == expected.height && actual.max_advance == expected.max_advance);
    };
    constexpr std::array<std::int32_t, 5> invalidIndices{-1, 1, 2, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    int requests = 0;
    int rendered = 0;
    for (const auto index : invalidIndices) {
        const auto nullError = FT_Select_Size(nullptr, index);
        Require(nullError != 0 && FT_Select_Size_nid_postfix(nullptr, index) == nullError);
        ++requests;
    }
    for (const auto index : {0, -1, std::numeric_limits<std::int32_t>::max()}) {
        const auto error = FT_Select_Size(nativeScalable, index);
        Require(error == FT_Err_Invalid_Face_Handle && FT_Select_Size_nid_postfix(scalable, index) == error);
        sizeMatches(scalable->size, nativeScalable->size);
        ++requests;
    }
    for (const auto& dimensions : {std::array{5, 7, 2}, std::array{11, 13, 3}}) {
        const auto [width, height, descent] = dimensions;
        const std::string font = BitmapStrikeFont(width, height, descent);
        const auto* bytes = reinterpret_cast<const unsigned char*>(font.data());
        GuestFaceRec* face = nullptr;
        FT_Face native = nullptr;
        Require(FT_New_Memory_Face_nid_postfix(library, bytes, static_cast<std::int64_t>(font.size()), 0, &face) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes, static_cast<FT_Long>(font.size()), 0, &native) == 0);
        Require(FT_HAS_FIXED_SIZES(native) && !FT_IS_SCALABLE(native) && face->num_fixed_sizes == 1);
        const GuestBitmapSize* strikes = face->available_sizes;
        const auto& expectedStrike = native->available_sizes[0];
        Require(strikes[0].width == expectedStrike.width && strikes[0].height == expectedStrike.height && strikes[0].size == expectedStrike.size);
        Require(strikes[0].x_ppem == expectedStrike.x_ppem && strikes[0].y_ppem == expectedStrike.y_ppem);
        GuestSizeRec* size = face->size;
        int marker = 0;
        size->generic.data = &marker;
        sizeMatches(size, native->size);
        Require(size->metrics.y_ppem == 0);
        const std::array<std::int32_t, 9> indices{-1, 1, invalidIndices[3], invalidIndices[4], 0, 2, 0, -1, 0};
        for (const auto index : indices) {
            face->glyph->advance.x = 0x123456789ABCll;
            face->bbox.xMin = -0x123456789ABCll;
            const auto error = FT_Select_Size(native, index);
            Require((index == 0) == (error == 0));
            Require(FT_Select_Size_nid_postfix(face, index) == error);
            ++requests;
            sizeMatches(size, native->size);
            Require(face->size == size && size->face == face && size->generic.data == &marker && face->available_sizes == strikes);
            Require(face->glyph->advance.x == 0x123456789ABCll && face->bbox.xMin == -0x123456789ABCll);
            if (error != 0) continue;
            Require(size->metrics.y_ppem == height && size->metrics.ascender == (height - descent) * 64 && size->metrics.descender == -descent * 64);
            for (const auto flags : std::array<std::int32_t, 3>{FT_LOAD_DEFAULT, FT_LOAD_RENDER, FT_LOAD_RENDER | FT_LOAD_MONOCHROME}) {
                Require(FT_Load_Char_nid_postfix(face, 'A', flags) == 0 && FT_Load_Char(native, 'A', flags) == 0);
                SlotMatches(face->glyph, native->glyph);
                const auto& bitmap = face->glyph->bitmap;
                Require(face->glyph->format == FT_GLYPH_FORMAT_BITMAP && bitmap.pixel_mode == FT_PIXEL_MODE_MONO);
                Require(bitmap.width == width && bitmap.rows == height && bitmap.pitch > 0 && face->glyph->bitmap_top == height - descent);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const bool pixel = (bitmap.buffer[y * bitmap.pitch + x / 8] & (0x80u >> (x % 8))) != 0;
                        Require(pixel == (x == 0 || y == height - 1 || (x + y) % 3 == 0));
                    }
                }
                ++rendered;
            }
        }
        Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(native) == 0);
    }
    std::printf("FT_Select_Size: %d native request comparisons, %d bitmap glyph comparisons\n", requests, rendered);
}

static void SetFinalizers(FT_Face face, Finalizers& events) {
    face->generic = {&events, NativeFaceFinalizer};
    face->glyph->generic = {&events, NativeSlotFinalizer};
    face->size->generic = {&events, NativeSizeFinalizer};
}

static bool Equal(GuestVector a, GuestVector b) {
    return a.x == b.x && a.y == b.y;
}

static bool Equal(const GuestOutline& a, const GuestOutline& b) {
    return a.n_points == b.n_points && a.n_contours == b.n_contours && a.points == b.points &&
           a.tags == b.tags && a.contours == b.contours && a.flags == b.flags;
}

static void CopyCase(std::uint16_t pointCount, std::uint16_t contourCount, int sourceFlags, int targetFlags, bool wide) {
    const GuestVector guard{0x123456789ABCDEF, -0x123456789ABCDEF};
    std::vector<GuestVector> points(pointCount + 2, guard), copied(pointCount + 2, guard);
    std::vector<unsigned char> tags(pointCount + 2, 0xA5), copiedTags(pointCount + 2, 0x5A);
    std::vector<std::uint16_t> contours(contourCount + 2, 0xABCD), copiedContours(contourCount + 2, 0xDCBA);
    std::vector<FT_Vector> hostPoints(pointCount), hostCopied(pointCount);
    std::vector<unsigned char> hostTags(pointCount), hostCopiedTags(pointCount);
    std::vector<unsigned short> hostContours(contourCount), hostCopiedContours(contourCount);
    for (unsigned index = 0; index < pointCount; ++index) {
        const auto value = static_cast<std::int64_t>(index) * 65537 - 100000;
        points[index + 1] = wide ? GuestVector{INT64_MIN + index,
                                             INT64_MAX - index}
                                : GuestVector{value / 4, -value / 4};
        tags[index + 1] = static_cast<unsigned char>(index);
        hostPoints[index] = {static_cast<FT_Pos>(points[index + 1].x), static_cast<FT_Pos>(points[index + 1].y)};
        hostTags[index] = tags[index + 1];
    }
    for (unsigned index = 0; index < contourCount; ++index) {
        contours[index + 1] = static_cast<std::uint16_t>(index);
        hostContours[index] = contours[index + 1];
    }
    GuestOutline source{contourCount, pointCount, pointCount ? points.data() + 1 : nullptr,
                        pointCount ? tags.data() + 1 : nullptr, contourCount ? contours.data() + 1 : nullptr, sourceFlags};
    GuestOutline target{contourCount, pointCount, pointCount ? copied.data() + 1 : nullptr,
                        pointCount ? copiedTags.data() + 1 : nullptr, contourCount ? copiedContours.data() + 1 : nullptr, targetFlags};
    const auto originalSource = source;
    auto expectedTarget = target;
    const auto originalPoints = points;
    const auto originalTags = tags;
    const auto originalContours = contours;
    FT_Outline hostSource{contourCount, pointCount, hostPoints.data(), hostTags.data(), hostContours.data(), sourceFlags};
    FT_Outline hostTarget{contourCount, pointCount, hostCopied.data(), hostCopiedTags.data(), hostCopiedContours.data(), targetFlags};
    Require(FT_Outline_Copy_nid_postfix(&source, &target) == FT_Outline_Copy(&hostSource, &hostTarget));
    expectedTarget.flags = hostTarget.flags;
    Require(Equal(source, originalSource) && Equal(target, expectedTarget));
    Require(Equal(copied.front(), guard) && Equal(copied.back(), guard));
    Require(copiedTags.front() == 0x5A && copiedTags.back() == 0x5A);
    Require(copiedContours.front() == 0xDCBA && copiedContours.back() == 0xDCBA);
    for (unsigned index = 0; index < pointCount; ++index) {
        Require(Equal(copied[index + 1], originalPoints[index + 1]));
        if (!wide) Require(copied[index + 1].x == hostCopied[index].x && copied[index + 1].y == hostCopied[index].y);
        Require(copiedTags[index + 1] == hostCopiedTags[index]);
    }
    for (unsigned index = 0; index < contourCount; ++index) Require(copiedContours[index + 1] == hostCopiedContours[index]);
    if (pointCount) {
        target.points[0] = {321, -654};
        target.tags[0] ^= 0xFF;
    }
    if (contourCount) target.contours[0] ^= 0xFFFF;
    for (std::size_t index = 0; index < points.size(); ++index) Require(Equal(points[index], originalPoints[index]));
    Require(tags == originalTags && contours == originalContours);
}

static void InvalidAndSelfCopies() {
    GuestOutline source{1, 4, nullptr, nullptr, nullptr, 0x1234};
    auto target = source;
    Require(FT_Outline_Copy_nid_postfix(nullptr, nullptr) == FT_Outline_Copy(nullptr, nullptr));
    Require(FT_Outline_Copy_nid_postfix(nullptr, &target) == FT_Err_Invalid_Outline);
    Require(Equal(target, source));
    Require(FT_Outline_Copy_nid_postfix(&source, nullptr) == FT_Err_Invalid_Outline);
    Require(FT_Outline_Copy_nid_postfix(&source, &source) == 0);
    for (const auto counts : std::array<std::array<std::uint16_t, 2>, 3>{{{1, 3}, {0, 4}, {0, 0}}}) {
        target.n_contours = counts[0];
        target.n_points = counts[1];
        const auto before = target;
        FT_Outline nativeSource{1, 4, nullptr, nullptr, nullptr, source.flags};
        FT_Outline nativeTarget{target.n_contours, target.n_points, nullptr, nullptr, nullptr, target.flags};
        Require(FT_Outline_Copy_nid_postfix(&source, &target) == FT_Outline_Copy(&nativeSource, &nativeTarget));
        Require(Equal(target, before));
    }
}

static void RenderCopy() {
    const auto font = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face nativeFace = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace) == 0);
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 32) == 0 && FT_Set_Pixel_Sizes(nativeFace, 0, 32) == 0);
    for (const int offset : {0, 37, -129}) {
        Require(FT_Load_Char_nid_postfix(face, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        Require(FT_Load_Char(nativeFace, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
        auto& target = face->glyph->outline;
        std::vector<GuestVector> points(target.points, target.points + target.n_points);
        std::vector<FT_Vector> nativePoints(nativeFace->glyph->outline.points,
                                            nativeFace->glyph->outline.points + nativeFace->glyph->outline.n_points);
        for (std::size_t index = 0; index < points.size(); ++index) {
            points[index].x += offset + points[index].y / 4;
            nativePoints[index].x += offset + nativePoints[index].y / 4;
        }
        auto source = target;
        source.points = points.data();
        source.flags ^= FT_OUTLINE_OWNER;
        auto nativeSource = nativeFace->glyph->outline;
        nativeSource.points = nativePoints.data();
        nativeSource.flags ^= FT_OUTLINE_OWNER;
        Require(FT_Outline_Copy_nid_postfix(&source, &target) == 0);
        Require(FT_Outline_Copy(&nativeSource, &nativeFace->glyph->outline) == 0);
        points[0] = {};
        nativePoints[0] = {};
        Require(FT_Render_Glyph_nid_postfix(face->glyph, FT_RENDER_MODE_NORMAL) == 0);
        Require(FT_Render_Glyph(nativeFace->glyph, FT_RENDER_MODE_NORMAL) == 0);
        const auto& actual = face->glyph->bitmap;
        const auto& expected = nativeFace->glyph->bitmap;
        Require(actual.rows > 0 && actual.width > 0);
        Require(actual.rows == expected.rows && actual.width == expected.width && actual.pitch == expected.pitch);
        Require(actual.pixel_mode == expected.pixel_mode && actual.num_grays == expected.num_grays);
        Require(face->glyph->bitmap_left == nativeFace->glyph->bitmap_left && face->glyph->bitmap_top == nativeFace->glyph->bitmap_top);
        const auto size = static_cast<std::size_t>(actual.rows) * std::abs(actual.pitch);
        Require(std::memcmp(actual.buffer, expected.buffer, size) == 0);
    }
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(nativeFace) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}

static void CheckOutlineCopies() {
    InvalidAndSelfCopies();
    for (const int sourceFlags : {0, FT_OUTLINE_OWNER, FT_OUTLINE_REVERSE_FILL, -1}) {
        for (const int targetFlags : {0, FT_OUTLINE_OWNER, FT_OUTLINE_EVEN_ODD_FILL, -1}) {
            CopyCase(0, 0, sourceFlags, targetFlags, false);
            CopyCase(4, 2, sourceFlags, targetFlags, false);
            CopyCase(4, 2, sourceFlags, targetFlags, true);
        }
    }
    CopyCase(65535, 65535, FT_OUTLINE_REVERSE_FILL, FT_OUTLINE_OWNER, false);
    CopyCase(65535, 65535, FT_OUTLINE_OWNER, 0, true);
    std::mt19937 random(0xF7C0);
    for (unsigned index = 0; index < 128; ++index) {
        const auto count = static_cast<std::uint16_t>(random() % 256 + 1);
        const auto contours = static_cast<std::uint16_t>(random() % (count + 1));
        const auto sourceFlags = static_cast<std::int32_t>(random() & 0x7FFFFFFFu);
        const auto targetFlags = static_cast<std::int32_t>(random() & 0x7FFFFFFFu);
        CopyCase(count, contours, sourceFlags, targetFlags, false);
    }
    RenderCopy();
    std::puts("FreeType outline copy: 178 copy cases, validation, self-copy and 3 rendered copies passed");
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
    FreeType::GuestLibrary* library = nullptr;
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
    {
        const auto referenceFont = SquareGlyphFont('A', 700);
        GuestLibrary* referenceLibrary = nullptr;
        FT_Library oracle = nullptr;
        Require(FT_Init_FreeType_nid_postfix(&referenceLibrary) == 0 && FT_Init_FreeType(&oracle) == 0);
        auto* owners = referenceLibrary;
        Require(FT_Reference_Face_nid_postfix(nullptr) == FT_Reference_Face(nullptr));
        int references = 0;
        int retained = 0;
        int finalized = 0;
        for (const unsigned depth : {1u, 2u, 3u, 17u}) {
            GuestFaceRec* shared = nullptr;
            FT_Face native = nullptr;
            Require(FT_New_Memory_Face_nid_postfix(referenceLibrary, referenceFont.data(), static_cast<std::int64_t>(referenceFont.size()), 0, &shared) == 0);
            Require(FT_New_Memory_Face(oracle, referenceFont.data(), static_cast<FT_Long>(referenceFont.size()), 0, &native) == 0);
            Finalizers guestEvents;
            Finalizers nativeEvents;
            guestEvents.reenter = nativeEvents.reenter = depth == 3;
            SetFinalizers(shared, guestEvents);
            SetFinalizers(native, nativeEvents);
            for (unsigned index = 1; index < depth; ++index) {
                Require(FT_Reference_Face_nid_postfix(shared) == FT_Reference_Face(native));
                ++references;
            }
            for (unsigned index = 1; index < depth; ++index) {
                Require(FT_Done_Face(native) == 0 && FT_Done_Face_nid_postfix(shared) == 0);
                Require(owners->faces.size() == 1 && guestEvents.calls.empty() && nativeEvents.calls.empty());
                Require(FT_Get_Char_Index_nid_postfix(shared, 'A') == FT_Get_Char_Index(native, 'A'));
                Require(FT_Set_Pixel_Sizes_nid_postfix(shared, 0, 16 + index) == 0 && FT_Set_Pixel_Sizes(native, 0, 16 + index) == 0);
                Require(FT_Load_Char_nid_postfix(shared, 'A', FT_LOAD_RENDER) == 0 && FT_Load_Char(native, 'A', FT_LOAD_RENDER) == 0);
                SlotMatches(shared->glyph, native->glyph);
                ++retained;
            }
            Require(FT_Done_Face(native) == 0 && FT_Done_Face_nid_postfix(shared) == 0);
            if (guestEvents.reenter) {
                references += 2;
                ++retained;
            }
            Require(owners->faces.empty() && guestEvents.calls == nativeEvents.calls && guestEvents.calls == std::vector<int>({1, 2, 3}));
            ++finalized;
        }
        GuestFaceRec* plain = nullptr;
        FT_Face nativePlain = nullptr;
        Require(FT_New_Memory_Face_nid_postfix(referenceLibrary, referenceFont.data(), static_cast<std::int64_t>(referenceFont.size()), 0, &plain) == 0);
        Require(FT_New_Memory_Face(oracle, referenceFont.data(), static_cast<FT_Long>(referenceFont.size()), 0, &nativePlain) == 0);
        Require(FT_Reference_Face_nid_postfix(plain) == FT_Reference_Face(nativePlain));
        ++references;
        Require(FT_Done_Face(nativePlain) == 0 && FT_Done_Face_nid_postfix(plain) == 0 && owners->faces.size() == 1);
        ++retained;
        Require(FT_Done_Face(nativePlain) == 0 && FT_Done_Face_nid_postfix(plain) == 0 && owners->faces.empty());
        ++finalized;
        GuestFaceRec* probe = nullptr;
        FT_Face nativeProbe = nullptr;
        Require(FT_New_Memory_Face_nid_postfix(referenceLibrary, referenceFont.data(), static_cast<std::int64_t>(referenceFont.size()), -1, &probe) == 0);
        Require(FT_New_Memory_Face(oracle, referenceFont.data(), static_cast<FT_Long>(referenceFont.size()), -1, &nativeProbe) == 0);
        Finalizers probeEvents;
        Finalizers nativeProbeEvents;
        probe->generic = {&probeEvents, GuestFaceFinalizer};
        nativeProbe->generic = {&nativeProbeEvents, NativeFaceFinalizer};
        Require(probe->glyph == nullptr && probe->size == nullptr);
        Require(FT_Reference_Face_nid_postfix(probe) == FT_Reference_Face(nativeProbe));
        ++references;
        Require(FT_Done_Face(nativeProbe) == 0 && FT_Done_Face_nid_postfix(probe) == 0 && owners->faces.size() == 1);
        ++retained;
        Require(probeEvents.calls.empty() && nativeProbeEvents.calls.empty());
        Require(FT_Done_Face(nativeProbe) == 0 && FT_Done_Face_nid_postfix(probe) == 0);
        Require(owners->faces.empty() && probeEvents.calls == nativeProbeEvents.calls && probeEvents.calls == std::vector<int>({3}));
        ++finalized;
        std::array<Finalizers, 2> shutdownEvents;
        std::array<Finalizers, 2> nativeShutdownEvents;
        std::array<GuestFaceRec*, 2> shutdown{};
        std::array<FT_Face, 2> nativeShutdown{};
        for (unsigned index = 0; index < shutdown.size(); ++index) {
            Require(FT_New_Memory_Face_nid_postfix(referenceLibrary, referenceFont.data(), static_cast<std::int64_t>(referenceFont.size()), 0, &shutdown[index]) == 0);
            Require(FT_New_Memory_Face(oracle, referenceFont.data(), static_cast<FT_Long>(referenceFont.size()), 0, &nativeShutdown[index]) == 0);
            SetFinalizers(shutdown[index], shutdownEvents[index]);
            SetFinalizers(nativeShutdown[index], nativeShutdownEvents[index]);
            for (unsigned reference = 0; reference < 3 - index; ++reference) {
                Require(FT_Reference_Face_nid_postfix(shutdown[index]) == FT_Reference_Face(nativeShutdown[index]));
                ++references;
            }
            Require(FT_Done_Face(nativeShutdown[index]) == 0 && FT_Done_Face_nid_postfix(shutdown[index]) == 0);
            Require(owners->faces.size() == index + 1 && shutdownEvents[index].calls.empty());
            ++retained;
        }
        Require(FT_Done_FreeType(oracle) == 0 && FT_Done_FreeType_nid_postfix(referenceLibrary) == 0);
        for (unsigned index = 0; index < shutdown.size(); ++index) {
            Require(shutdownEvents[index].calls == nativeShutdownEvents[index].calls && shutdownEvents[index].calls == std::vector<int>({1, 2, 3}));
            ++finalized;
        }
        Require(references == 28 && retained == 24 && finalized == 8);
        std::printf("FreeType face references: %d retains, %d non-final releases, %d finalized faces compared with native\n", references, retained, finalized);
    }
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_guest_freetype-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "app0");
    CheckGasp();
    Trigonometry();
    OutlineDecomposition();
    const std::vector<unsigned char> font = SquareGlyphFont('A', 700);
    CheckOutlineCopies();
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
    FreeType::GuestLibrary* library = nullptr;
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
    {
        FT_Face nativeFile = nullptr;
        Require(FT_New_Face(hostLibrary, "app0/font.ttf", 0, &nativeFile) == 0);
        const auto* bytes = FaceOf(face)->bytes.data();
        Require(FaceOf(face)->bytes.size() == font.size() && std::memcmp(bytes, font.data(), font.size()) == 0);
        Require(FT_Reference_Face_nid_postfix(face) == FT_Reference_Face(nativeFile));
        Require(FT_Done_Face(nativeFile) == 0 && FT_Done_Face_nid_postfix(face) == 0);
        Require(static_cast<GuestLibrary*>(library)->faces.size() == 2);
        Require(FaceOf(face)->bytes.data() == bytes && std::memcmp(bytes, font.data(), font.size()) == 0);
        Require(FT_Get_Char_Index_nid_postfix(face, 'A') == FT_Get_Char_Index(nativeFile, 'A'));
        Require(FT_Done_Face(nativeFile) == 0);
        std::puts("File-backed face reference: owned font bytes survive a non-final release");
    }
    FaceMatches(face, host, library);

    CheckStrikeSelection(library, hostLibrary, face, host);

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

    FreeType::GuestLibrary* independent = nullptr;
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

    FreeType::GuestLibrary* empty = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&empty) == 0 && FT_Done_FreeType_nid_postfix(empty) == 0);
    CheckTransforms(font);
    std::filesystem::current_path(previous);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
