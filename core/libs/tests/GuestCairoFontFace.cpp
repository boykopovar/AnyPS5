#include "FreeTypeFontFixture.hpp"
#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "prx/libfontconfig/include/InventoryExports.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <type_traits>

namespace {

std::array<void*, 16> events{};
unsigned eventCount{};
cairo_font_face_t* reentryFace{};
cairo_user_data_key_t reentryKey{};
int reentryData{};
int reentryStatus{-1};

void Require(bool value, const char* reason) {
    if (value) return;
    std::fprintf(stderr, "cairo font face: %s\n", reason);
    std::abort();
}

void APS5_VABI Record(void* data) {
    Require(eventCount < events.size(), "too many user-data callbacks");
    events[eventCount++] = data;
}

void APS5_VABI Reenter(void*) {
    reentryStatus = cairo_font_face_set_user_data(reentryFace, &reentryKey, &reentryData, nullptr);
}

}

int main() {
    static_assert(sizeof(cairo_user_data_key_t) == sizeof(int));
    static_assert(std::is_same_v<decltype(cairo_font_face_reference(nullptr)), cairo_font_face_t*>);
    static_assert(std::is_same_v<decltype(cairo_font_face_destroy(nullptr)), void>);
    static_assert(std::is_same_v<decltype(cairo_font_face_get_user_data(nullptr, nullptr)), void*>);
    static_assert(std::is_same_v<decltype(cairo_font_face_set_user_data(nullptr, nullptr, nullptr, nullptr)), int>);
    static_assert(std::is_same_v<decltype(cairo_ft_font_options_substitute(nullptr, nullptr)), void>);

    FreeType::GuestLibrary* guestLibrary = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0, "guest FreeType initialization failed");
    Require(FT_Init_FreeType(&nativeLibrary) == 0, "native FreeType initialization failed");

    auto font = SquareGlyphFont('A', 913);
    FreeType::GuestFaceRec* guestFace = nullptr;
    FT_Face nativeFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(guestLibrary, font.data(), static_cast<std::int64_t>(font.size()), 0, &guestFace) == 0,
            "guest face creation failed");
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace) == 0,
            "native face creation failed");

    auto* face = cairo_ft_font_face_create_for_ft_face(guestFace, FT_LOAD_DEFAULT);
    Require(face != nullptr, "Cairo face creation returned null");
    Require(FT_Reference_Face(nativeFace) == 0, "native face reference failed");
    Require(FT_Done_Face_nid_postfix(guestFace) == 0, "guest caller face release failed");
    Require(FT_Done_Face(nativeFace) == 0, "native caller face release failed");
    Require(FT_Get_Char_Index_nid_postfix(guestFace, 'A') == FT_Get_Char_Index(nativeFace, 'A'),
            "Cairo face did not retain the FreeType face");

    cairo_user_data_key_t first{7};
    cairo_user_data_key_t equal{7};
    int firstData{};
    int secondData{};
    int replacementData{};
    int nullKeyData{};
    Require(cairo_font_face_set_user_data(face, &first, &firstData, Record) == 0, "first user-data insert failed");
    Require(cairo_font_face_set_user_data(face, &equal, &secondData, Record) == 0, "second user-data insert failed");
    Require(cairo_font_face_get_user_data(face, &first) == &firstData, "first key lookup failed");
    Require(cairo_font_face_get_user_data(face, &equal) == &secondData, "keys were compared by value");
    Require(cairo_font_face_set_user_data(face, &first, &replacementData, Record) == 0, "user-data replacement failed");
    Require(eventCount == 1 && events[0] == &firstData, "replacement did not destroy the previous value");
    Require(cairo_font_face_get_user_data(face, &first) == &replacementData, "replacement value was not retained");
    Require(cairo_font_face_set_user_data(face, &equal, nullptr, Record) == 0, "user-data removal failed");
    Require(eventCount == 2 && events[1] == &secondData, "removal did not destroy the previous value");
    Require(cairo_font_face_get_user_data(face, &equal) == nullptr, "removed value is still visible");
    Require(cairo_font_face_set_user_data(face, nullptr, &nullKeyData, Record) == 0, "null-key insertion failed");
    Require(cairo_font_face_get_user_data(face, nullptr) == &nullKeyData, "null key lookup failed");
    Require(cairo_font_face_reference(face) == face, "reference did not return the same face");
    cairo_font_face_destroy(face);
    Require(eventCount == 2, "nonfinal destroy ran user-data callbacks");
    cairo_font_face_destroy(face);
    Require(eventCount == 4 && events[2] == &nullKeyData && events[3] == &replacementData,
            "final destroy did not release user data in reverse registration order");
    Require(FT_Done_Face(nativeFace) == 0, "native Cairo face release failed");

    guestFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(guestLibrary, font.data(), static_cast<std::int64_t>(font.size()), 0, &guestFace) == 0,
            "guest face creation for callback reentry failed");
    face = cairo_ft_font_face_create_for_ft_face(guestFace, FT_LOAD_DEFAULT);
    Require(face != nullptr && FT_Done_Face_nid_postfix(guestFace) == 0, "callback face retention failed");
    reentryFace = face;
    reentryStatus = -1;
    Require(cairo_font_face_set_user_data(face, &reentryKey, &reentryData, Reenter) == 0,
            "reentrant user-data registration failed");
    cairo_font_face_destroy(face);
    Require(reentryStatus == 12, "finalizer reentry did not report invalid status");
    reentryFace = nullptr;

    auto* nullFace = cairo_ft_font_face_create_for_ft_face(nullptr, 0);
    Require(nullFace != nullptr, "null FreeType face did not return an error face");
    Require(cairo_font_face_set_user_data(nullFace, &first, &firstData, Record) == 7,
            "error face did not preserve its null-pointer status");
    Require(cairo_font_face_get_user_data(nullFace, &first) == nullptr, "error face returned user data");
    Require(cairo_font_face_reference(nullFace) == nullFace, "error face reference changed its identity");
    cairo_font_face_destroy(nullFace);

    auto* defaultOptions = cairo_font_options_create();
    auto* emptyPattern = FcPatternCreate();
    Require(defaultOptions != nullptr && emptyPattern != nullptr, "default options or pattern allocation failed");
    cairo_ft_font_options_substitute(defaultOptions, emptyPattern);
    int integerValue = -1;
    FcBool booleanValue = -1;
    Require(FcPatternGetBool(emptyPattern, "antialias", 0, &booleanValue) == FcResultNoMatch,
            "default antialias option unexpectedly changed the pattern");
    Require(FcPatternGetInteger(emptyPattern, "rgba", 0, &integerValue) == FcResultNoMatch,
            "default subpixel order unexpectedly changed the pattern");
    Require(FcPatternGetInteger(emptyPattern, "hintstyle", 0, &integerValue) == FcResultNoMatch,
            "default hint style unexpectedly changed the pattern");
    Require(FcPatternGetBool(emptyPattern, "hinting", 0, &booleanValue) == FcResultNoMatch,
            "default hint metrics unexpectedly changed the pattern");
    cairo_ft_font_options_substitute(nullptr, emptyPattern);
    cairo_font_options_destroy(defaultOptions);
    FcPatternDestroy(emptyPattern);

    auto* options = cairo_font_options_create();
    auto* pattern = FcPatternCreate();
    Require(options != nullptr && pattern != nullptr, "options or pattern allocation failed");
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_VBGR);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_FULL);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    cairo_ft_font_options_substitute(options, pattern);
    Require(FcPatternGetBool(pattern, "antialias", 0, &booleanValue) == FcResultMatch && booleanValue == 1,
            "subpixel antialias option was not substituted");
    Require(FcPatternGetInteger(pattern, "rgba", 0, &integerValue) == FcResultMatch && integerValue == 4,
            "vertical BGR subpixel order was not substituted");
    Require(FcPatternGetInteger(pattern, "hintstyle", 0, &integerValue) == FcResultMatch && integerValue == 3,
            "full hint style was not substituted");
    Require(FcPatternGetBool(pattern, "hinting", 0, &booleanValue) == FcResultMatch && booleanValue == 0,
            "disabled hint metrics were not substituted");
    cairo_font_options_destroy(options);
    FcPatternDestroy(pattern);

    options = cairo_font_options_create();
    pattern = FcPatternCreate();
    Require(options != nullptr && pattern != nullptr, "second options or pattern allocation failed");
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_NONE);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_RGB);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    cairo_ft_font_options_substitute(options, pattern);
    Require(FcPatternGetBool(pattern, "antialias", 0, &booleanValue) == FcResultMatch && booleanValue == 0,
            "disabled antialias option was not substituted");
    Require(FcPatternGetInteger(pattern, "rgba", 0, &integerValue) == FcResultNoMatch,
            "non-subpixel antialias option set an RGBA mode");
    Require(FcPatternGetInteger(pattern, "hintstyle", 0, &integerValue) == FcResultMatch && integerValue == 0,
            "no hint style was not substituted");
    Require(FcPatternGetBool(pattern, "hinting", 0, &booleanValue) == FcResultMatch && booleanValue == 1,
            "enabled hint metrics were not substituted");
    cairo_font_options_destroy(options);
    FcPatternDestroy(pattern);

    Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0, "guest FreeType shutdown failed");
    Require(FT_Done_FreeType(nativeLibrary) == 0, "native FreeType shutdown failed");
    std::puts("cairo font face tests passed");
    return 0;
}
