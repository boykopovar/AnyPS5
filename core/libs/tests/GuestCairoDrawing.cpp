#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>

static_assert(std::is_same_v<decltype(cairo_in_stroke(nullptr, 0, 0)), int>);

namespace {

constexpr int kArgb32 = 0;
constexpr int kStatusErrorTarget = 1;
constexpr std::uint32_t kRed = 0xffff0000;
constexpr std::uint32_t kGreen = 0xff00ff00;
constexpr std::uint32_t kBlue = 0xff0000ff;

struct Canvas {
    CairoSurface* surface;
    CairoContext* context;
};

Canvas Make(int width, int height) {
    CairoSurface* surface = cairo_image_surface_create(kArgb32, width, height);
    assert(surface && cairo_surface_status(surface) == 0);
    CairoContext* context = cairo_create(surface);
    assert(context);
    return {surface, context};
}

void Release(Canvas canvas) {
    cairo_destroy(canvas.context);
    cairo_surface_destroy(canvas.surface);
}

int Status(CairoContext* context) {
    return cairo_surface_status(cairo_get_target(context));
}

std::uint32_t Pixel(CairoSurface* surface, int x, int y) {
    cairo_surface_flush(surface);
    const unsigned char* row = cairo_image_surface_get_data(surface) + y * cairo_image_surface_get_stride(surface);
    std::uint32_t value = 0;
    std::memcpy(&value, row + x * 4, sizeof(value));
    return value;
}

void Bounds(const double* actual, double x1, double y1, double x2, double y2) {
    assert(actual[0] == x1 && actual[1] == y1 && actual[2] == x2 && actual[3] == y2);
}

void CheckFill() {
    Canvas canvas = Make(8, 8);
    cairo_set_source_rgb(canvas.context, 1, 0, 0);
    cairo_rectangle(canvas.context, 2, 2, 4, 4);

    double box[4] = {-1, -1, -1, -1};
    cairo_fill_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 2, 2, 6, 6);
    cairo_fill_extents(canvas.context, nullptr, &box[1], nullptr, &box[3]);

    cairo_fill_preserve(canvas.context);
    assert(Pixel(canvas.surface, 2, 2) == kRed);
    assert(Pixel(canvas.surface, 5, 5) == kRed);
    assert(Pixel(canvas.surface, 1, 1) == 0);
    assert(Pixel(canvas.surface, 6, 6) == 0);
    assert(cairo_has_current_point(canvas.context));

    cairo_set_source_rgb(canvas.context, 0, 0, 1);
    cairo_fill(canvas.context);
    assert(Pixel(canvas.surface, 3, 3) == kBlue);
    assert(!cairo_has_current_point(canvas.context));
    cairo_fill_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    assert(Status(canvas.context) == 0);
    Release(canvas);
}

void CheckTranslucentPaint() {
    Canvas canvas = Make(2, 2);
    cairo_set_source_rgba(canvas.context, 0, 0, 1, 0.5);
    cairo_paint(canvas.context);
    assert(Pixel(canvas.surface, 0, 0) == 0x80000080);
    assert(Pixel(canvas.surface, 1, 1) == 0x80000080);
    Release(canvas);

    canvas = Make(2, 2);
    cairo_set_source_rgb(canvas.context, 1, 0, 0);
    cairo_paint_with_alpha(canvas.context, 0.5);
    assert(Pixel(canvas.surface, 1, 0) == 0x80800000);
    cairo_paint_with_alpha(canvas.context, 0);
    assert(Pixel(canvas.surface, 1, 0) == 0x80800000);
    cairo_paint_with_alpha(canvas.context, 1);
    assert(Pixel(canvas.surface, 1, 0) == kRed);
    Release(canvas);
}

void CheckStroke() {
    Canvas canvas = Make(8, 8);
    cairo_set_source_rgb(canvas.context, 0, 1, 0);
    cairo_set_line_width(canvas.context, 2);
    cairo_move_to(canvas.context, 1, 4);
    cairo_line_to(canvas.context, 7, 4);

    double box[4] = {};
    cairo_stroke_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 1, 3, 7, 5);
    assert(cairo_in_stroke(canvas.context, 4, 4) == 1);
    assert(cairo_in_stroke(canvas.context, 4, 3.5) == 1);
    assert(cairo_in_stroke(canvas.context, 4, 1) == 0);
    assert(cairo_in_stroke(canvas.context, 0.5, 4) == 0);

    cairo_stroke_preserve(canvas.context);
    assert(Pixel(canvas.surface, 1, 3) == kGreen);
    assert(Pixel(canvas.surface, 6, 4) == kGreen);
    assert(Pixel(canvas.surface, 4, 2) == 0);
    assert(Pixel(canvas.surface, 4, 5) == 0);
    assert(Pixel(canvas.surface, 0, 4) == 0);
    assert(cairo_has_current_point(canvas.context));

    cairo_stroke(canvas.context);
    assert(!cairo_has_current_point(canvas.context));
    assert(cairo_in_stroke(canvas.context, 4, 4) == 0);
    cairo_stroke_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    assert(Status(canvas.context) == 0);
    Release(canvas);
}

void CheckClip() {
    Canvas canvas = Make(8, 8);
    double box[4] = {};
    cairo_clip_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 8, 8);

    cairo_save(canvas.context);
    cairo_rectangle(canvas.context, 0, 0, 4, 8);
    cairo_clip(canvas.context);
    assert(!cairo_has_current_point(canvas.context));
    cairo_clip_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 4, 8);
    cairo_translate(canvas.context, 1, 2);
    cairo_clip_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, -1, -2, 3, 6);
    cairo_set_source_rgb(canvas.context, 1, 0, 0);
    cairo_paint(canvas.context);
    cairo_restore(canvas.context);

    assert(Pixel(canvas.surface, 0, 0) == kRed);
    assert(Pixel(canvas.surface, 3, 7) == kRed);
    assert(Pixel(canvas.surface, 4, 0) == 0);
    assert(Pixel(canvas.surface, 7, 7) == 0);
    cairo_clip_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 8, 8);
    Release(canvas);
}

void CheckSurfaceSource() {
    Canvas source = Make(2, 2);
    cairo_set_source_rgb(source.context, 0, 1, 0);
    cairo_paint(source.context);

    Canvas canvas = Make(8, 8);
    cairo_set_source_surface(canvas.context, source.surface, 3, 4);
    Release(source);
    cairo_paint(canvas.context);
    assert(Pixel(canvas.surface, 3, 4) == kGreen);
    assert(Pixel(canvas.surface, 4, 5) == kGreen);
    assert(Pixel(canvas.surface, 2, 4) == 0);
    assert(Pixel(canvas.surface, 5, 4) == 0);
    assert(Pixel(canvas.surface, 3, 6) == 0);
    assert(Status(canvas.context) == 0);

    cairo_set_source_surface(canvas.context, nullptr, 0, 0);
    assert(Status(canvas.context) == kStatusErrorTarget);
    cairo_set_source_rgb(canvas.context, 1, 0, 0);
    cairo_paint(canvas.context);
    assert(Pixel(canvas.surface, 0, 0) == 0);
    Release(canvas);
}

void CheckGroups() {
    Canvas canvas = Make(4, 4);
    cairo_push_group(canvas.context);
    assert(cairo_get_target(canvas.context) == canvas.surface);
    cairo_set_source_rgb(canvas.context, 0, 0, 1);
    cairo_rectangle(canvas.context, 0, 0, 2, 4);
    cairo_fill(canvas.context);
    assert(Pixel(canvas.surface, 0, 0) == 0);
    cairo_pop_group_to_source(canvas.context);
    cairo_paint(canvas.context);
    assert(Pixel(canvas.surface, 0, 0) == kBlue);
    assert(Pixel(canvas.surface, 1, 3) == kBlue);
    assert(Pixel(canvas.surface, 2, 0) == 0);
    assert(Status(canvas.context) == 0);

    cairo_pop_group_to_source(canvas.context);
    assert(Status(canvas.context) == kStatusErrorTarget);
    Release(canvas);
}

void CheckErrorAndNullContexts() {
    double box[4] = {-1, -1, -1, -1};
    cairo_fill_extents(nullptr, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    box[0] = box[1] = box[2] = box[3] = -1;
    cairo_stroke_extents(nullptr, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    box[0] = box[1] = box[2] = box[3] = -1;
    cairo_clip_extents(nullptr, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    cairo_fill_extents(nullptr, nullptr, nullptr, nullptr, nullptr);
    assert(cairo_in_stroke(nullptr, 0, 0) == 0);

    Canvas canvas = Make(4, 4);
    cairo_set_line_width(canvas.context, 4);
    cairo_rectangle(canvas.context, 0, 0, 4, 4);
    cairo_scale(canvas.context, 0, 1);
    assert(Status(canvas.context) == kStatusErrorTarget);
    box[0] = box[1] = box[2] = box[3] = -1;
    cairo_fill_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    box[0] = box[1] = box[2] = box[3] = -1;
    cairo_stroke_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    box[0] = box[1] = box[2] = box[3] = -1;
    cairo_clip_extents(canvas.context, &box[0], &box[1], &box[2], &box[3]);
    Bounds(box, 0, 0, 0, 0);
    assert(cairo_in_stroke(canvas.context, 0, 0) == 0);
    cairo_set_source_rgb(canvas.context, 1, 0, 0);
    cairo_paint(canvas.context);
    cairo_fill(canvas.context);
    cairo_stroke(canvas.context);
    cairo_clip(canvas.context);
    cairo_push_group(canvas.context);
    cairo_pop_group_to_source(canvas.context);
    assert(Status(canvas.context) == kStatusErrorTarget);
    assert(Pixel(canvas.surface, 0, 0) == 0);
    Release(canvas);
}

void CheckConcurrentDrawing() {
    const auto work = [](double red) {
        for (int i = 0; i < 512; ++i) {
            Canvas canvas = Make(8, 8);
            cairo_set_source_rgb(canvas.context, red, 0, 1 - red);
            cairo_rectangle(canvas.context, 0, 0, 8, 8);
            cairo_fill(canvas.context);
            assert(Pixel(canvas.surface, 7, 7) == (red == 1 ? kRed : kBlue));
            Release(canvas);
        }
    };
    std::thread first(work, 1.0);
    std::thread second(work, 0.0);
    first.join();
    second.join();
}

} // namespace

int main() {
    CheckFill();
    CheckTranslucentPaint();
    CheckStroke();
    CheckClip();
    CheckSurfaceSource();
    CheckGroups();
    CheckErrorAndNullContexts();
    CheckConcurrentDrawing();
    std::puts("guest cairo drawing tests passed");
}
