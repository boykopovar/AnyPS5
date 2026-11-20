#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

static_assert(std::is_same_v<decltype(&_ZN3WTF12refIfNotNullI6_cairoEEvPT_), void (APS5_VABI*)(CairoContext*)>);
static_assert(std::is_same_v<decltype(&_ZN3WTF14derefIfNotNullI6_cairoEEvPT_), void (APS5_VABI*)(CairoContext*)>);
static_assert(std::is_same_v<decltype(&_ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_), void (APS5_VABI*)(CairoSurface*)>);
static_assert(std::is_same_v<decltype(&_ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_), void (APS5_VABI*)(CairoSurface*)>);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "RefPtrCairo check failed at line %u\n", location.line());
    std::abort();
}

struct DestroyTracker {
    int count = 0;
};

CairoUserDataKey trackerKey{};

void APS5_VABI RecordDestroy(void* data) {
    ++static_cast<DestroyTracker*>(data)->count;
}

CairoSurface* CreateTrackedSurface(DestroyTracker& tracker) {
    CairoSurface* surface = cairo_image_surface_create(0, 4, 4);
    Require(surface != nullptr);
    Require(cairo_surface_status(surface) == 0);
    Require(cairo_surface_set_user_data(surface, &trackerKey, &tracker, &RecordDestroy) == 0);
    return surface;
}

void NullPointersAreNoOps() {
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(nullptr);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(nullptr);
}

void SurfaceReferenceCounting() {
    DestroyTracker tracker;
    CairoSurface* surface = CreateTrackedSurface(tracker);

    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(tracker.count == 0);
    Require(cairo_surface_status(surface) == 0);

    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(surface);
    cairo_surface_destroy(surface);
    Require(tracker.count == 0);

    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(tracker.count == 0);
    Require(cairo_surface_status(surface) == 0);

    _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(surface);
    Require(tracker.count == 1);
}

void ContextReferenceCounting() {
    DestroyTracker tracker;
    CairoSurface* surface = CreateTrackedSurface(tracker);
    CairoContext* context = cairo_create(surface);
    Require(context != nullptr);
    Require(cairo_get_target(context) == surface);

    cairo_surface_destroy(surface);
    Require(tracker.count == 0);

    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(context);
    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(context);
    Require(tracker.count == 0);
    Require(cairo_get_target(context) == surface);

    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(context);
    _ZN3WTF12refIfNotNullI6_cairoEEvPT_(context);
    cairo_destroy(context);
    Require(tracker.count == 0);

    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(context);
    Require(tracker.count == 0);
    Require(cairo_get_target(context) == surface);

    _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(context);
    Require(tracker.count == 1);
}

} // namespace

int main() {
    NullPointersAreNoOps();
    SurfaceReferenceCounting();
    ContextReferenceCounting();
    return 0;
}
