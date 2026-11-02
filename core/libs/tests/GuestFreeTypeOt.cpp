#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRIGONOMETRY_H
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestOutline;
using FreeType::GuestOutlineFuncs;
using FreeType::GuestVector;

namespace {

struct Exports {
    decltype(&FT_Sin_nid_postfix) sin;
    decltype(&FT_Tan_nid_postfix) tan;
    decltype(&FT_Vector_Unit_nid_postfix) vectorUnit;
    decltype(&FT_Outline_Get_Orientation_nid_postfix) orientation;
    decltype(&FT_Outline_Copy_nid_postfix) copy;
    decltype(&FT_Outline_Decompose_nid_postfix) decompose;
};

Exports ot;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt: %s\n", message);
    std::exit(1);
}

using Events = std::vector<std::array<std::int64_t, 7>>;

int Add(void* user, int kind, GuestVector first, GuestVector second = {}, GuestVector third = {}) {
    static_cast<Events*>(user)->push_back({kind, first.x, first.y, second.x, second.y, third.x, third.y});
    return 0;
}

GuestVector Guest(const FT_Vector* point) { return {point->x, point->y}; }

int NativeMove(const FT_Vector* to, void* user) { return Add(user, 1, Guest(to)); }
int NativeLine(const FT_Vector* to, void* user) { return Add(user, 2, Guest(to)); }
int NativeConic(const FT_Vector* control, const FT_Vector* to, void* user) { return Add(user, 3, Guest(control), Guest(to)); }
int NativeCubic(const FT_Vector* first, const FT_Vector* second, const FT_Vector* to, void* user) {
    return Add(user, 4, Guest(first), Guest(second), Guest(to));
}
int APS5_VABI GuestMove(const GuestVector* to, void* user) { return Add(user, 1, *to); }
int APS5_VABI GuestLine(const GuestVector* to, void* user) { return Add(user, 2, *to); }
int APS5_VABI GuestConic(const GuestVector* control, const GuestVector* to, void* user) { return Add(user, 3, *control, *to); }
int APS5_VABI GuestCubic(const GuestVector* first, const GuestVector* second, const GuestVector* to, void* user) {
    return Add(user, 4, *first, *second, *to);
}

void Trigonometry() {
    std::mt19937 random(0x0F7u);
    std::uniform_int_distribution<std::int32_t> angles(INT32_MIN, INT32_MAX);
    for (int sample = 0; sample < 4096; ++sample) {
        const std::int32_t angle = angles(random);
        Require(ot.sin(angle) == FT_Sin(angle), "FT_Sin");
        Require(static_cast<std::int32_t>(ot.tan(angle)) == FT_Tan(angle), "FT_Tan");
        FT_Vector expected{};
        GuestVector actual{};
        FT_Vector_Unit(&expected, angle);
        ot.vectorUnit(&actual, angle);
        Require(actual.x == expected.x && actual.y == expected.y, "FT_Vector_Unit");
    }
}

void Outlines() {
    std::vector<GuestVector> points{{0, 0}, {640, 0}, {640, 640}, {0, 640}, {64, 64}, {320, 576}, {576, 64}, {320, 32}, {200, 100}};
    std::vector<unsigned char> tags{FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_CONIC, FT_CURVE_TAG_ON,
                                    FT_CURVE_TAG_ON, FT_CURVE_TAG_CUBIC, FT_CURVE_TAG_CUBIC, FT_CURVE_TAG_ON, FT_CURVE_TAG_CONIC};
    std::vector<std::uint16_t> contours{3, 8};
    std::vector<FT_Vector> nativePoints;
    for (const auto& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    GuestOutline outline{2, 9, points.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL};
    FT_Outline native{2, 9, nativePoints.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL};

    Require(ot.orientation(&outline) == FT_Outline_Get_Orientation(&native), "orientation");
    Require(ot.orientation(&outline) == FT_ORIENTATION_POSTSCRIPT, "counter-clockwise outline");
    Require(ot.orientation(nullptr) == FT_ORIENTATION_TRUETYPE, "null outline orientation");

    for (const auto [shift, delta] : {std::pair{0, std::int64_t{0}}, std::pair{2, std::int64_t{-7}}}) {
        Events actual;
        Events expected;
        const GuestOutlineFuncs functions{GuestMove, GuestLine, GuestConic, GuestCubic, shift, delta};
        const FT_Outline_Funcs nativeFunctions{NativeMove, NativeLine, NativeConic, NativeCubic, shift, static_cast<FT_Pos>(delta)};
        Require(ot.decompose(&outline, &functions, &actual) == FT_Outline_Decompose(&native, &nativeFunctions, &expected), "decompose result");
        Require(!actual.empty() && actual == expected, "decompose events");
    }
    Require(ot.decompose(nullptr, nullptr, nullptr) == FT_Err_Invalid_Outline, "decompose null outline");

    std::vector<GuestVector> copiedPoints(points.size());
    std::vector<unsigned char> copiedTags(tags.size());
    std::vector<std::uint16_t> copiedContours(contours.size());
    GuestOutline copy{2, 9, copiedPoints.data(), copiedTags.data(), copiedContours.data(), FT_OUTLINE_OWNER};
    Require(ot.copy(&outline, &copy) == FT_Err_Ok, "copy");
    for (std::size_t index = 0; index < points.size(); ++index) {
        Require(copiedPoints[index].x == points[index].x && copiedPoints[index].y == points[index].y && copiedTags[index] == tags[index], "copied points");
    }
    Require(copiedContours == contours && copy.flags == (FT_OUTLINE_EVEN_ODD_FILL | FT_OUTLINE_OWNER), "copied contours and flags");
    copy.n_points = 8;
    Require(ot.copy(&outline, &copy) == FT_Err_Invalid_Argument, "copy size mismatch");
    Require(ot.copy(nullptr, &copy) == FT_Err_Invalid_Outline, "copy null source");
}

template<class Function>
void Resolve(Function& function, void* library, bool patched, const char* nid, const char* name) {
    const char* symbol = patched ? nid : name;
#ifdef _WIN32
    function = reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), symbol));
#else
    function = reinterpret_cast<Function>(dlsym(library, symbol));
#endif
    if (!function) std::fprintf(stderr, "libSceFreeTypeOt: missing %s\n", symbol);
    Require(function != nullptr, "missing export");
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "expected library and export mode");
    const bool patched = std::strcmp(argv[2], "patched") == 0;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    void* library = LoadLibraryA(argv[1]);
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(library != nullptr, "load requested library");
    Resolve(ot.sin, library, patched, "L1uHkWTazkI", "FT_Sin_nid_postfix");
    Resolve(ot.tan, library, patched, "dojyaLKzKN0", "FT_Tan_nid_postfix");
    Resolve(ot.vectorUnit, library, patched, "3WpTQrvGUcU", "FT_Vector_Unit_nid_postfix");
    Resolve(ot.orientation, library, patched, "weVsBCT+TTQ", "FT_Outline_Get_Orientation_nid_postfix");
    Resolve(ot.copy, library, patched, "Bhm8Injl0KA", "FT_Outline_Copy_nid_postfix");
    Resolve(ot.decompose, library, patched, "++lgV9pRsO4", "FT_Outline_Decompose_nid_postfix");
    Trigonometry();
    Outlines();
    std::puts("libSceFreeTypeOt: trigonometry and outline helpers match FreeType");
    return 0;
}
