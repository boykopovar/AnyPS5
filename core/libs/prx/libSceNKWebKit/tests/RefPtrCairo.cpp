#include "prx/libSceNKWebKit/include/RefPtrCairoExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit RefPtrCairo check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

const CairoUserDataKey releasedKey{};

void APS5_VABI MarkReleased(void* data) {
    ++*static_cast<int*>(data);
}

CairoSurface* TrackedSurface(int* released) {
    CairoSurface* surface = cairo_image_surface_create(0, 4, 4);
    Require(surface && cairo_surface_status(surface) == 0);
    Require(cairo_surface_set_user_data(surface, &releasedKey, released, MarkReleased) == 0);
    return surface;
}

void TestSurface() {
    int released = 0;
    CairoSurface* surface = TrackedSurface(&released);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    cairo_surface_destroy(surface);
    Require(released == 0);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(released == 0);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(released == 1);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
}

void TestContext() {
    int released = 0;
    CairoSurface* surface = TrackedSurface(&released);
    CairoContext* context = cairo_create(surface);
    Require(context && cairo_get_target(context) == surface);
    cairo_surface_destroy(surface);
    Require(released == 0);
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(context);
    cairo_destroy(context);
    Require(released == 0);
    Require(cairo_get_target(context) == surface);
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(context);
    Require(released == 1);
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(nullptr);
}

}

int main() {
    TestSurface();
    TestContext();
    return 0;
}
