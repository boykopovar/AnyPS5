#include "FreeTypeFontFixture.hpp"
#include "prx/libcairo/include/InventoryExports.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

    Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0, "guest FreeType shutdown failed");
    Require(FT_Done_FreeType(nativeLibrary) == 0, "native FreeType shutdown failed");
    std::puts("cairo font face tests passed");
    return 0;
}
