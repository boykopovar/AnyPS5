#include "prx/libSceNKWebKit/include/RefPtrCairoExports.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

using ContextFunction = void (APS5_VABI *)(CairoContext*);
using SurfaceFunction = void (APS5_VABI *)(CairoSurface*);
static_assert(std::is_same_v<decltype(&_ZN3WTF12refIfNotNullI6_cairoEEvPT_), ContextFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF14derefIfNotNullI6_cairoEEvPT_), ContextFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_), SurfaceFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_), SurfaceFunction>);

int released = 0;
CairoUserDataKey key{};

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "RefPtrCairo: %s\n", message);
        std::abort();
    }
}

void APS5_VABI Release(void* data) {
    Require(data == &released, "user data mismatch");
    ++released;
}

CairoSurface* TrackedSurface() {
    released = 0;
    auto* surface = cairo_image_surface_create(0, 4, 4);
    Require(surface && cairo_surface_status(surface) == 0, "surface creation failed");
    Require(cairo_surface_set_user_data(surface, &key, &released, Release) == 0, "user data rejected");
    return surface;
}

void CheckNull() {
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(nullptr);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
}

void CheckSurface() {
    auto* surface = TrackedSurface();
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    cairo_surface_destroy(surface);
    Require(released == 0, "surface released while a reference remains");
    Require(cairo_surface_status(surface) == 0, "surface unusable while referenced");
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(released == 1, "surface not released by the last deref");
}

void CheckContext() {
    auto* surface = TrackedSurface();
    auto* context = cairo_create(surface);
    Require(context != nullptr, "context creation failed");
    cairo_surface_destroy(surface);
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(context);
    cairo_destroy(context);
    Require(released == 0, "context released while a reference remains");
    Require(cairo_get_target(context) == surface, "context lost its target");
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(context);
    Require(released == 1, "context not released by the last deref");
}

}

int main() {
    CheckNull();
    CheckSurface();
    CheckContext();
    std::puts("RefPtrCairo tests passed");
    return 0;
}
