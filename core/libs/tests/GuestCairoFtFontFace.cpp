#include "prx/libcairo/include/FontFace.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&cairo_ft_font_face_create_for_pattern), CairoFontFace* (APS5_VABI*)(FcPattern*)>);

namespace {

CairoUserDataKey key;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "cairo ft font face: %s\n", message);
    std::abort();
}

template <class Callback>
bool Throws(Callback&& callback) {
    try {
        callback();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool IsNil(CairoFontFace* face, int status) {
    int value = 0;
    return cairo_font_face_set_user_data(face, &key, &value, nullptr) == status &&
           cairo_font_face_get_user_data(face, &key) == nullptr;
}

FcPattern* FilePattern(const std::string& file) {
    FcPattern* pattern = FcPatternCreate();
    Require(pattern && FcPatternAddString(pattern, "file", reinterpret_cast<const FcChar8*>(file.c_str())),
            "file pattern");
    return pattern;
}

CairoFontFace* FaceFor(const std::string& file, void (*configure)(FcPattern*)) {
    FcPattern* pattern = FilePattern(file);
    if (configure) configure(pattern);
    CairoFontFace* face = cairo_ft_font_face_create_for_pattern(pattern);
    FcPatternDestroy_nid_postfix(pattern);
    return face;
}

void SameFace(const std::string& file, void (*first)(FcPattern*), void (*second)(FcPattern*), bool same,
              const char* message) {
    CairoFontFace* a = FaceFor(file, first);
    CairoFontFace* b = FaceFor(file, second);
    Require((a == b) == same, message);
    cairo_font_face_destroy(a);
    cairo_font_face_destroy(b);
}

void CheckUnresolvedPattern() {
    FcPattern* pattern = FcPatternCreate();
    Require(FcPatternAddString(pattern, "family", reinterpret_cast<const FcChar8*>("Sans")), "family pattern");
    CairoFontFace* first = cairo_ft_font_face_create_for_pattern(pattern);
    CairoFontFace* second = cairo_ft_font_face_create_for_pattern(pattern);
    FcPatternDestroy_nid_postfix(pattern);
    Require(first != second, "unresolved patterns create distinct faces");
    Require(!IsNil(first, 1) && !IsNil(first, 18), "unresolved pattern face is usable");
    int value = 5;
    Require(cairo_font_face_set_user_data(first, &key, &value, nullptr) == 0, "user data on pattern face");
    Require(cairo_font_face_get_user_data(first, &key) == &value, "user data read back");
    Require(cairo_font_face_get_user_data(second, &key) == nullptr, "user data is per face");
    Require(cairo_font_face_reference(first) == first, "pattern face reference");
    cairo_font_face_destroy(first);
    cairo_font_face_destroy(first);
    cairo_font_face_destroy(second);
}

void CheckMissingFile(const std::string& missing) {
    CairoFontFace* face = FaceFor(missing, nullptr);
    Require(face == FaceFor(missing, nullptr), "missing files share the nil face");
    Require(IsNil(face, 18), "missing file gives CAIRO_STATUS_FILE_NOT_FOUND");
    Require(cairo_font_face_reference(face) == face, "nil face reference");
    cairo_font_face_destroy(face);
    Require(IsNil(face, 18), "nil face survives destroy");
}

void CheckReadableFile(const std::string& file) {
    CairoFontFace* first = FaceFor(file, nullptr);
    CairoFontFace* second = FaceFor(file, nullptr);
    Require(first == second && !IsNil(first, 18) && !IsNil(first, 1), "same file and options share a face");
    int value = 9;
    Require(cairo_font_face_set_user_data(first, &key, &value, nullptr) == 0, "file face user data");
    cairo_font_face_destroy(second);
    Require(FaceFor(file, nullptr) == first, "live face stays cached");
    cairo_font_face_destroy(first);
    cairo_font_face_destroy(first);
    CairoFontFace* fresh = FaceFor(file, nullptr);
    Require(cairo_font_face_get_user_data(fresh, &key) == nullptr, "destroyed face leaves the cache");
    cairo_font_face_destroy(fresh);

    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "index", 0); }, true,
             "index defaults to zero");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "index", 1); }, false,
             "index selects another face");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "hintstyle", 3); }, true,
             "hint style defaults to full");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "hintstyle", 1); }, false,
             "slight hinting differs");
    SameFace(file, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "hintstyle", 2); },
             [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "hintstyle", 7); }, true,
             "unknown hint style is medium");
    SameFace(file, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "hinting", 0); },
             [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "hintstyle", 0); }, true,
             "disabled hinting is hint style none");
    SameFace(file, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "hinting", 0); },
             [](FcPattern* p) {
                 FcPatternAddBool_nid_postfix(p, "hinting", 0);
                 FcPatternAddBool_nid_postfix(p, "embeddedbitmap", 1);
             },
             true, "no hinting forces embedded bitmaps off");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "embeddedbitmap", 1); }, false,
             "embedded bitmaps change the load flags");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "rgba", 5); }, true,
             "rgba none keeps the default subpixel order");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "rgba", 1); }, false,
             "rgba selects subpixel antialiasing");
    SameFace(file, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "rgba", 1); },
             [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "rgba", 2); }, false,
             "subpixel orders differ");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "lcdfilter", 9); }, true,
             "unknown lcd filter is ignored");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddInteger_nid_postfix(p, "lcdfilter", 1); }, false,
             "lcd filter is part of the options");
    SameFace(file, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "antialias", 0); },
             [](FcPattern* p) {
                 FcPatternAddBool_nid_postfix(p, "antialias", 0);
                 FcPatternAddInteger_nid_postfix(p, "rgba", 1);
                 FcPatternAddInteger_nid_postfix(p, "hintstyle", 1);
             },
             true, "without antialiasing rgba and hint style are ignored");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "antialias", 0); }, false,
             "antialiasing is part of the options");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "autohint", 1); }, false,
             "autohint changes the load flags");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "verticallayout", 1); }, false,
             "vertical layout changes the load flags");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddBool_nid_postfix(p, "embolden", 1); }, false,
             "embolden changes the synthesis flags");
    SameFace(file, nullptr,
             [](FcPattern* p) { FcPatternAddString(p, "fontvariations", reinterpret_cast<const FcChar8*>("")); },
             false, "empty variations differ from none");
    SameFace(file,
             [](FcPattern* p) { FcPatternAddString(p, "fontvariations", reinterpret_cast<const FcChar8*>("wght=7")); },
             [](FcPattern* p) { FcPatternAddString(p, "fontvariations", reinterpret_cast<const FcChar8*>("wght=7")); },
             true, "equal variations share a face");
    SameFace(file, nullptr, [](FcPattern* p) { FcPatternAddString(p, "family", reinterpret_cast<const FcChar8*>("Sans")); },
             true, "other properties do not change the face");
}

}

int main() {
    const auto name = "anyps5-cairo-ft-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string file = name + ".ttf";
    { std::ofstream output(std::filesystem::current_path() / file, std::ios::binary); output << "font"; }
    Require(Throws([] { cairo_ft_font_face_create_for_pattern(nullptr); }), "null pattern throws");
    CheckUnresolvedPattern();
    CheckMissingFile(name + "-missing.ttf");
    CheckReadableFile(file);
    std::filesystem::remove(std::filesystem::current_path() / file);
    std::puts("cairo ft font face PASS");
    return EXIT_SUCCESS;
}
