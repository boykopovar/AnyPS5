#include "prx/libcairo/include/FontFace.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <vector>

namespace {

CairoUserDataKey firstKey;
CairoUserDataKey secondKey;
CairoUserDataKey thirdKey;
std::vector<int> destroyed;
FreeType::GuestFaceRec* reenterFace;
CairoFontFace* reentered;
void* reenteredData;

void APS5_VABI Record(void* data) {
    destroyed.push_back(*static_cast<int*>(data));
}

void APS5_VABI Reenter(void* data) {
    destroyed.push_back(*static_cast<int*>(data));
    reentered = cairo_ft_font_face_create_for_ft_face(reenterFace, 0);
    reenteredData = cairo_font_face_get_user_data(reentered, &firstKey);
}

CairoFontFace* heldFace;

void APS5_VABI ReferenceAndDestroy(void* data) {
    destroyed.push_back(*static_cast<int*>(data));
    assert(cairo_font_face_reference(heldFace) == heldFace);
    cairo_font_face_destroy(heldFace);
}

void CheckCache() {
    FreeType::GuestFaceRec face{};
    FreeType::GuestFaceRec other{};
    CairoFontFace* first = cairo_ft_font_face_create_for_ft_face(&face, 0);
    CairoFontFace* second = cairo_ft_font_face_create_for_ft_face(&face, 0);
    CairoFontFace* flagged = cairo_ft_font_face_create_for_ft_face(&face, 1);
    CairoFontFace* negative = cairo_ft_font_face_create_for_ft_face(&face, -1);
    CairoFontFace* foreign = cairo_ft_font_face_create_for_ft_face(&other, 0);
    assert(first && first == second);
    assert(flagged != first && negative != first && negative != flagged && foreign != first);
    face.face_index = 3;
    CairoFontFace* indexed = cairo_ft_font_face_create_for_ft_face(&face, 0);
    assert(indexed != first);
    assert(cairo_ft_font_face_create_for_ft_face(&face, 0) == indexed);
    cairo_font_face_destroy(indexed);
    cairo_font_face_destroy(indexed);
    face.face_index = 0;

    int value = 7;
    assert(cairo_font_face_set_user_data(first, &firstKey, &value, Record) == 0);
    assert(cairo_font_face_get_user_data(second, &firstKey) == &value);
    assert(cairo_font_face_get_user_data(flagged, &firstKey) == nullptr);
    assert(cairo_font_face_reference(first) == first);
    cairo_font_face_destroy(first);
    cairo_font_face_destroy(second);
    assert(destroyed.empty());
    assert(cairo_ft_font_face_create_for_ft_face(&face, 0) == first);
    cairo_font_face_destroy(first);
    cairo_font_face_destroy(first);
    assert(destroyed == std::vector<int>{7});
    destroyed.clear();

    CairoFontFace* fresh = cairo_ft_font_face_create_for_ft_face(&face, 0);
    assert(cairo_font_face_get_user_data(fresh, &firstKey) == nullptr);
    cairo_font_face_destroy(fresh);
    cairo_font_face_destroy(flagged);
    cairo_font_face_destroy(negative);
    cairo_font_face_destroy(foreign);
    assert(cairo_font_face_reference(nullptr) == nullptr);
    cairo_font_face_destroy(nullptr);
}

void CheckUserData() {
    FreeType::GuestFaceRec face{};
    CairoFontFace* fontFace = cairo_ft_font_face_create_for_ft_face(&face, 0);
    int one = 1, two = 2, three = 3, four = 4;
    assert(cairo_font_face_get_user_data(fontFace, &firstKey) == nullptr);
    assert(cairo_font_face_set_user_data(fontFace, &firstKey, &one, Record) == 0);
    assert(cairo_font_face_set_user_data(fontFace, &secondKey, &two, Record) == 0);
    assert(cairo_font_face_set_user_data(fontFace, &firstKey, &three, Record) == 0);
    assert(destroyed == std::vector<int>{1});
    assert(cairo_font_face_get_user_data(fontFace, &firstKey) == &three);
    assert(cairo_font_face_set_user_data(fontFace, &firstKey, nullptr, Record) == 0);
    assert(destroyed == (std::vector<int>{1, 3}));
    assert(cairo_font_face_get_user_data(fontFace, &firstKey) == nullptr);
    assert(cairo_font_face_set_user_data(fontFace, &thirdKey, nullptr, Record) == 0);
    assert(cairo_font_face_set_user_data(fontFace, &thirdKey, &four, nullptr) == 0);
    assert(cairo_font_face_get_user_data(fontFace, &thirdKey) == &four);
    assert(cairo_font_face_set_user_data(fontFace, &firstKey, &one, Record) == 0);
    destroyed.clear();
    cairo_font_face_destroy(fontFace);
    assert(destroyed == (std::vector<int>{1, 2}));
    destroyed.clear();
}

void CheckReentrantFinalizer() {
    FreeType::GuestFaceRec face{};
    reenterFace = &face;
    CairoFontFace* fontFace = cairo_ft_font_face_create_for_ft_face(&face, 0);
    int one = 1, two = 2, three = 3;
    cairo_font_face_set_user_data(fontFace, &firstKey, &one, Record);
    cairo_font_face_set_user_data(fontFace, &secondKey, &two, Record);
    cairo_font_face_set_user_data(fontFace, &thirdKey, &three, Reenter);
    cairo_font_face_destroy(fontFace);
    assert(destroyed == (std::vector<int>{3, 2, 1}));
    assert(reentered && reenteredData == nullptr);
    assert(cairo_ft_font_face_create_for_ft_face(&face, 0) == reentered);
    cairo_font_face_destroy(reentered);
    cairo_font_face_destroy(reentered);
    destroyed.clear();

    heldFace = cairo_ft_font_face_create_for_ft_face(&face, 0);
    cairo_font_face_set_user_data(heldFace, &firstKey, &one, Record);
    cairo_font_face_set_user_data(heldFace, &secondKey, &two, ReferenceAndDestroy);
    cairo_font_face_destroy(heldFace);
    assert(destroyed == (std::vector<int>{2, 1}));
    CairoFontFace* after = cairo_ft_font_face_create_for_ft_face(&face, 0);
    assert(cairo_font_face_get_user_data(after, &secondKey) == nullptr);
    cairo_font_face_destroy(after);
    destroyed.clear();
}

}

int main() {
    CheckCache();
    CheckUserData();
    CheckReentrantFinalizer();
    std::puts("cairo FreeType font face tests passed");
    return 0;
}
