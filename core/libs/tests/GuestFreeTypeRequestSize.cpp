#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Request_Size_nid_postfix(GuestFaceRec*, const GuestSizeRequestRec*);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType size request check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Fixture {
    std::vector<unsigned char> bytes;
    FT_Library library = nullptr;
    FT_Face native = nullptr;
    void* guestLibrary = nullptr;
    GuestFaceRec* guest = nullptr;

    explicit Fixture(std::vector<unsigned char> font, int faceIndex = 0) : bytes(std::move(font)) {
        Require(FT_Init_FreeType(&library) == 0);
        Require(FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), faceIndex, &native) == 0);
        Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0);
        Require(FT_New_Memory_Face_nid_postfix(guestLibrary, bytes.data(), static_cast<std::int64_t>(bytes.size()), faceIndex, &guest) == 0);
    }

    ~Fixture() {
        Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0);
        Require(FT_Done_FreeType(library) == 0);
    }

    void CheckMetrics() const {
        const auto& actual = guest->size->metrics;
        const auto& expected = native->size->metrics;
        Require(actual.x_ppem == expected.x_ppem && actual.y_ppem == expected.y_ppem);
        Require(actual.x_scale == expected.x_scale && actual.y_scale == expected.y_scale);
        Require(actual.ascender == expected.ascender && actual.descender == expected.descender);
        Require(actual.height == expected.height && actual.max_advance == expected.max_advance);
    }

    int Request(GuestSizeRequestRec request) {
        FT_Size_RequestRec hostRequest{static_cast<FT_Size_Request_Type>(request.type), static_cast<FT_Long>(request.width),
            static_cast<FT_Long>(request.height), request.horiResolution, request.vertResolution};
        const auto before = request;
        const auto* size = guest->size;
        const auto generic = size->generic;
        const int error = FT_Request_Size(native, &hostRequest);
        Require(FT_Request_Size_nid_postfix(guest, &request) == error);
        Require(request.type == before.type && request.width == before.width && request.height == before.height);
        Require(request.horiResolution == before.horiResolution && request.vertResolution == before.vertResolution);
        Require(guest->size == size && size->face == guest && size->generic.data == generic.data && size->generic.finalizer == generic.finalizer);
        CheckMetrics();
        return error;
    }

    void Load() const {
        Require(FT_Load_Glyph(native, 1, FT_LOAD_DEFAULT) == 0);
        Require(FT_Load_Glyph_nid_postfix(guest, 1, FT_LOAD_DEFAULT) == 0);
        const auto& actual = guest->glyph->metrics;
        const auto& expected = native->glyph->metrics;
        Require(actual.width == expected.width && actual.height == expected.height);
        Require(actual.horiBearingX == expected.horiBearingX && actual.horiBearingY == expected.horiBearingY);
        Require(actual.horiAdvance == expected.horiAdvance && actual.vertAdvance == expected.vertAdvance);
        Require(guest->glyph->advance.x == native->glyph->advance.x && guest->glyph->advance.y == native->glyph->advance.y);
    }
};

static void CheckScalable() {
    Fixture fixture(SquareGlyphFont('A', 913));
    fixture.guest->size->generic.data = &fixture;
    for (std::uint32_t type = FT_SIZE_REQUEST_TYPE_NOMINAL; type < FT_SIZE_REQUEST_TYPE_MAX; ++type) {
        Require(fixture.Request({type, 0, 0, 0, 0}) == FT_Err_Invalid_PPem);
        for (const auto& dimensions : std::array<std::array<std::int64_t, 2>, 4>{{{0, 23 * 64 + 17},
                 {31 * 64 + 49, 0}, {17 * 64 + 31, 29 * 64 + 33}, {48 * 64, 12 * 64}}}) {
            for (const auto& dpi : std::array<std::array<std::uint32_t, 2>, 4>{{{0, 0}, {72, 144}, {96, 0}, {0, 120}}}) {
                const auto factor = type == FT_SIZE_REQUEST_TYPE_SCALES ? 64 : 1;
                Require(fixture.Request({type, dimensions[0] * factor, dimensions[1] * factor, dpi[0], dpi[1]}) == 0);
                fixture.Load();
            }
        }
    }
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_SCALES, 0x18000, 0x28000, 0, 0}) == 0);
    const auto scales = fixture.guest->size->metrics;
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_SCALES, 0x18000, 0x28000,
        std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()}) == 0);
    Require(fixture.guest->size->metrics.x_scale == scales.x_scale && fixture.guest->size->metrics.y_scale == scales.y_scale);
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 65536LL * 64, 17 * 64, 0, 0}) == FT_Err_Invalid_Pixel_Size);
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 19 * 64, 27 * 64, 0, 0}) == 0);
    fixture.Load();
    auto* slot = fixture.guest->glyph;
    Require(slot->outline.n_points > 0);
    slot->outline.points[0].x += 137;
    slot->outline.flags ^= 2;
    const auto point = slot->outline.points[0];
    const auto savedSlot = *slot;
    for (const auto width : {21LL * 64, 65536LL * 64}) {
        fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, width, 15 * 64, 0, 0});
        Require(std::memcmp(slot, &savedSlot, sizeof(savedSlot)) == 0);
        Require(slot->outline.points[0].x == point.x && slot->outline.points[0].y == point.y);
    }
}

static void CheckInvalid() {
    Fixture fixture(SquareGlyphFont('A', 913));
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 17 * 64, 23 * 64, 0, 0}) == 0);
    for (const auto& request : std::array<GuestSizeRequestRec, 6>{{
             {FT_SIZE_REQUEST_TYPE_NOMINAL, -1, 1024, 0, 0},
             {FT_SIZE_REQUEST_TYPE_NOMINAL, 1024, std::numeric_limits<std::int64_t>::min(), 0, 0},
             {FT_SIZE_REQUEST_TYPE_MAX, 1024, 1024, 0, 0},
             {std::numeric_limits<std::uint32_t>::max(), 1024, 1024, 0, 0},
             {FT_SIZE_REQUEST_TYPE_MAX, std::numeric_limits<std::int64_t>::max(), 1024, 0, 0},
             {FT_SIZE_REQUEST_TYPE_NOMINAL, std::numeric_limits<std::int64_t>::max(), -1, 0, 0}}}) {
        Require(FT_Request_Size_nid_postfix(fixture.guest, &request) == FT_Err_Invalid_Argument);
        Require(FT_Request_Size_nid_postfix(nullptr, &request) == FT_Err_Invalid_Face_Handle);
        fixture.CheckMetrics();
    }
    Require(FT_Request_Size_nid_postfix(nullptr, nullptr) == FT_Err_Invalid_Face_Handle);
    Require(FT_Request_Size_nid_postfix(fixture.guest, nullptr) == FT_Request_Size(fixture.native, nullptr));
    fixture.CheckMetrics();

    Fixture probe(SquareGlyphFont('A', 913), -1);
    Require(probe.guest->size == nullptr && probe.native->size == nullptr);
    GuestSizeRequestRec request{FT_SIZE_REQUEST_TYPE_NOMINAL, -1, -1, 0, 0};
    Require(FT_Request_Size_nid_postfix(probe.guest, &request) == FT_Err_Invalid_Size_Handle);
    Require(FT_Request_Size_nid_postfix(probe.guest, nullptr) == FT_Err_Invalid_Size_Handle);

    for (const auto& dimensions : std::array<std::array<std::int64_t, 2>, 2>{{{0x100000000LL + 1024, 1024}, {1024, 0x100000000LL + 1024}}}) {
        request = {FT_SIZE_REQUEST_TYPE_NOMINAL, dimensions[0], dimensions[1], 0, 0};
        if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
            Require(fixture.Request(request) == FT_Err_Invalid_Pixel_Size);
        } else {
            bool threw = false;
            try {
                FT_Request_Size_nid_postfix(fixture.guest, &request);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            Require(threw);
            fixture.CheckMetrics();
        }
    }
}

static void CheckBitmap() {
    const std::string bdf = "STARTFONT 2.1\nFONT -AnyPS5-Fixed-Medium-R-Normal--8-80-72-72-C-80-ISO10646-1\n"
        "SIZE 8 72 72\nFONTBOUNDINGBOX 8 8 0 0\nSTARTPROPERTIES 4\nFONT_ASCENT 8\nFONT_DESCENT 0\n"
        "CHARSET_REGISTRY \"ISO10646\"\nCHARSET_ENCODING \"1\"\nENDPROPERTIES\nCHARS 1\nSTARTCHAR A\nENCODING 65\n"
        "SWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 8 0 0\nBITMAP\n18\n24\n42\n7E\n42\n42\n42\n00\nENDCHAR\nENDFONT\n";
    Fixture fixture({bdf.begin(), bdf.end()});
    Require(!FT_IS_SCALABLE(fixture.native) && fixture.guest->num_fixed_sizes == 1);
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 8 * 64, 8 * 64, 0, 0}) == 0);
    fixture.Load();
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 0, 8 * 64, 72, 72}) == 0);
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_REAL_DIM, 8 * 64, 8 * 64, 0, 0}) == 0);
    Require(fixture.Request({FT_SIZE_REQUEST_TYPE_NOMINAL, 0, 17 * 64, 0, 0}) == FT_Err_Invalid_Pixel_Size);
    for (const auto type : {FT_SIZE_REQUEST_TYPE_BBOX, FT_SIZE_REQUEST_TYPE_CELL, FT_SIZE_REQUEST_TYPE_SCALES}) {
        Require(fixture.Request({static_cast<std::uint32_t>(type), 8 * 64, 8 * 64, 0, 0}) == FT_Err_Unimplemented_Feature);
    }
}

int main() {
    CheckScalable();
    CheckInvalid();
    CheckBitmap();
}
