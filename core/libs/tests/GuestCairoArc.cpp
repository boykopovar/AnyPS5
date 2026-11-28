#include "prx/libcairo/include/Path.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <CairoBackend.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>

using Arc = void (APS5_VABI*)(CairoContext*, double, double, double, double, double);
static_assert(std::is_same_v<decltype(&cairo_arc), Arc>);
static_assert(std::is_same_v<decltype(&cairo_arc_negative), Arc>);

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double FixedStep = 1.0 / 256.0;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cairo arc check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

bool Near(double actual, double expected, double tolerance) {
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

enum class Prefix { None, MoveTo, LineTo };
enum class Transform { Identity, ScaledAndTranslated, Singular };

struct Case {
    double xc;
    double yc;
    double radius;
    double angle1;
    double angle2;
};

const Case Cases[] = {
    {10, 20, 5, 0, Pi / 2},
    {10, 20, 5, Pi / 2, 0},
    {-3, -4, 2.5, -Pi, Pi},
    {0, 0, 1, 0, 7 * Pi},
    {0, 0, 1, 3, -100},
    {1, 2, 3, -0.25, -0.25},
    {7, 8, 0, 0, Pi},
    {7, 8, -1, 0, Pi},
    {7, 8, 1e-9, 0, 1},
    {1e6, -1e6, 1e3, 0.5, 2.5},
};

struct Element {
    int type;
    std::vector<double> points;

    bool operator==(const Element& other) const {
        return type == other.type && points == other.points;
    }
};

struct Snapshot {
    int status;
    std::vector<Element> elements;
    int hasCurrentPoint;
    double currentX;
    double currentY;

    bool operator==(const Snapshot& other) const {
        return status == other.status && elements == other.elements && hasCurrentPoint == other.hasCurrentPoint
            && currentX == other.currentX && currentY == other.currentY;
    }
};

Snapshot GuestSnapshot(CairoContext* context) {
    Snapshot snapshot{};
    CairoPath* path = cairo_copy_path(context);
    Require(path != nullptr);
    snapshot.status = path->status;
    for (int index = 0; index < path->num_data; index += path->data[index].header.length) {
        Element element{path->data[index].header.type, {}};
        for (int point = 1; point < path->data[index].header.length; ++point) {
            element.points.push_back(path->data[index + point].point.x);
            element.points.push_back(path->data[index + point].point.y);
        }
        snapshot.elements.push_back(element);
    }
    cairo_path_destroy(path);
    snapshot.hasCurrentPoint = cairo_has_current_point(context);
    cairo_get_current_point(context, &snapshot.currentX, &snapshot.currentY);
    return snapshot;
}

Snapshot NativeSnapshot(cairo_t* context) {
    Snapshot snapshot{};
    cairo_path_t* path = aps5_cairo_copy_path(context);
    Require(path != nullptr);
    snapshot.status = path->status;
    for (int index = 0; index < path->num_data; index += path->data[index].header.length) {
        Element element{path->data[index].header.type, {}};
        for (int point = 1; point < path->data[index].header.length; ++point) {
            element.points.push_back(path->data[index + point].point.x);
            element.points.push_back(path->data[index + point].point.y);
        }
        snapshot.elements.push_back(element);
    }
    aps5_cairo_path_destroy(path);
    snapshot.hasCurrentPoint = aps5_cairo_has_current_point(context);
    aps5_cairo_get_current_point(context, &snapshot.currentX, &snapshot.currentY);
    return snapshot;
}

CairoContext* GuestContext() {
    CairoSurface* surface = cairo_image_surface_create(0, 64, 64);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    return context;
}

cairo_t* NativeContext() {
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t* context = aps5_cairo_create(surface);
    aps5_cairo_surface_destroy(surface);
    return context;
}

void PrepareGuest(CairoContext* context, Prefix prefix, Transform transform) {
    if (transform == Transform::ScaledAndTranslated) {
        cairo_translate(context, 3.5, -2.25);
        cairo_scale(context, 2, 0.5);
    } else if (transform == Transform::Singular) {
        cairo_scale(context, 0, 1);
    }
    if (prefix == Prefix::MoveTo) cairo_move_to(context, -1.5, 2.5);
    if (prefix == Prefix::LineTo) {
        cairo_move_to(context, 4, 4);
        cairo_line_to(context, 6, 1);
    }
}

void PrepareNative(cairo_t* context, Prefix prefix, Transform transform) {
    if (transform == Transform::ScaledAndTranslated) {
        aps5_cairo_translate(context, 3.5, -2.25);
        aps5_cairo_scale(context, 2, 0.5);
    } else if (transform == Transform::Singular) {
        aps5_cairo_scale(context, 0, 1);
    }
    if (prefix == Prefix::MoveTo) aps5_cairo_move_to(context, -1.5, 2.5);
    if (prefix == Prefix::LineTo) {
        aps5_cairo_move_to(context, 4, 4);
        aps5_cairo_line_to(context, 6, 1);
    }
}

void CheckMatchesNative() {
    for (bool negative : {false, true}) {
        for (Prefix prefix : {Prefix::None, Prefix::MoveTo, Prefix::LineTo}) {
            for (Transform transform : {Transform::Identity, Transform::ScaledAndTranslated, Transform::Singular}) {
                for (const Case& arc : Cases) {
                    CairoContext* guest = GuestContext();
                    cairo_t* native = NativeContext();
                    PrepareGuest(guest, prefix, transform);
                    PrepareNative(native, prefix, transform);
                    if (negative) {
                        cairo_arc_negative(guest, arc.xc, arc.yc, arc.radius, arc.angle1, arc.angle2);
                        aps5_cairo_arc_negative(native, arc.xc, arc.yc, arc.radius, arc.angle1, arc.angle2);
                    } else {
                        cairo_arc(guest, arc.xc, arc.yc, arc.radius, arc.angle1, arc.angle2);
                        aps5_cairo_arc(native, arc.xc, arc.yc, arc.radius, arc.angle1, arc.angle2);
                    }
                    Snapshot guestSnapshot = GuestSnapshot(guest);
                    Snapshot nativeSnapshot = NativeSnapshot(native);
                    Require(guestSnapshot == nativeSnapshot);
                    Require((transform == Transform::Singular) == (guestSnapshot.status != 0));
                    cairo_destroy(guest);
                    aps5_cairo_destroy(native);
                }
            }
        }
    }
}

void CheckQuarterArcs() {
    CairoContext* context = GuestContext();
    cairo_arc(context, 10, 20, 5, 0, Pi / 2);
    Snapshot positive = GuestSnapshot(context);
    Require(positive.status == 0 && positive.elements.size() >= 2);
    Require(positive.elements[0].type == 0);
    Require(Near(positive.elements[0].points[0], 15, FixedStep) && Near(positive.elements[0].points[1], 20, FixedStep));
    for (std::size_t index = 1; index < positive.elements.size(); ++index) Require(positive.elements[index].type == 2);
    Require(positive.hasCurrentPoint == 1);
    Require(Near(positive.currentX, 10, FixedStep) && Near(positive.currentY, 25, FixedStep));
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    cairo_fill_extents(context, &x1, &y1, &x2, &y2);
    Require(Near(x1, 10, 0.05) && Near(y1, 20, 0.05) && Near(x2, 15, 0.05) && Near(y2, 25, 0.05));
    cairo_destroy(context);

    context = GuestContext();
    cairo_arc_negative(context, 10, 20, 5, 0, Pi / 2);
    Snapshot negative = GuestSnapshot(context);
    Require(negative.status == 0 && negative.elements.size() > positive.elements.size());
    Require(Near(negative.elements[0].points[0], 15, FixedStep) && Near(negative.elements[0].points[1], 20, FixedStep));
    Require(Near(negative.currentX, 10, FixedStep) && Near(negative.currentY, 25, FixedStep));
    cairo_fill_extents(context, &x1, &y1, &x2, &y2);
    Require(Near(x1, 5, 0.05) && Near(y1, 15, 0.05) && Near(x2, 15, 0.05) && Near(y2, 25, 0.05));
    cairo_destroy(context);
}

void CheckWrappedAngles() {
    CairoContext* context = GuestContext();
    cairo_arc(context, 0, 0, 1, Pi, 0);
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    cairo_fill_extents(context, &x1, &y1, &x2, &y2);
    Require(Near(x1, -1, 0.05) && Near(y1, -1, 0.05) && Near(x2, 1, 0.05) && Near(y2, 0, 0.05));
    cairo_destroy(context);

    context = GuestContext();
    cairo_arc_negative(context, 0, 0, 1, Pi, 0);
    cairo_fill_extents(context, &x1, &y1, &x2, &y2);
    Require(Near(x1, -1, 0.05) && Near(y1, 0, 0.05) && Near(x2, 1, 0.05) && Near(y2, 1, 0.05));
    cairo_destroy(context);
}

void CheckLineToArcStart() {
    CairoContext* context = GuestContext();
    cairo_move_to(context, 0, 0);
    cairo_arc(context, 10, 20, 5, 0, Pi / 2);
    Snapshot snapshot = GuestSnapshot(context);
    Require(snapshot.elements.size() >= 3);
    Require(snapshot.elements[0].type == 0 && snapshot.elements[1].type == 1);
    Require(Near(snapshot.elements[1].points[0], 15, FixedStep) && Near(snapshot.elements[1].points[1], 20, FixedStep));
    cairo_destroy(context);
}

void CheckNonPositiveRadius() {
    for (double radius : {0.0, -2.0}) {
        CairoContext* context = GuestContext();
        cairo_arc(context, 7, 8, radius, 0, Pi);
        Snapshot snapshot = GuestSnapshot(context);
        Require(snapshot.status == 0 && !snapshot.elements.empty());
        for (const Element& element : snapshot.elements) {
            Require(element.type == 0 || element.type == 1);
            Require(element.points[0] == 7 && element.points[1] == 8);
        }
        Require(snapshot.currentX == 7 && snapshot.currentY == 8);
        cairo_destroy(context);
    }
}

}

int main() {
    CheckMatchesNative();
    CheckQuarterArcs();
    CheckWrappedAngles();
    CheckLineToArcStart();
    CheckNonPositiveRadius();
    std::puts("cairo arc tests passed");
    return 0;
}
