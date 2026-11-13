#include "prx/libcairo/include/Path.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(&cairo_in_fill), int (APS5_VABI*)(CairoContext*, double, double)>);

namespace {

constexpr int kWinding = 0;
constexpr int kEvenOdd = 1;

struct Probe {
    double x;
    double y;
    int inside;
};

struct Point {
    double x;
    double y;
};

CairoContext* NewContext(double deviceScale = 1) {
    CairoSurface* surface = cairo_image_surface_create(0, 64, 64);
    cairo_surface_set_device_scale(surface, deviceScale, deviceScale);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    assert(context);
    return context;
}

void Expect(const char* label, CairoContext* context, std::initializer_list<Probe> probes) {
    for (const Probe& probe : probes) {
        const int actual = cairo_in_fill(context, probe.x, probe.y);
        if (actual != probe.inside) {
            std::fprintf(stderr, "%s: cairo_in_fill(%.17g, %.17g) returned %d, expected %d\n",
                         label, probe.x, probe.y, actual, probe.inside);
            std::abort();
        }
    }
}

bool Throws(CairoContext* context, double x, double y) {
    try {
        cairo_in_fill(context, x, y);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int PathElementCount(CairoContext* context) {
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0);
    const int count = path->num_data;
    cairo_path_destroy(path);
    return count;
}

void CheckInvalidContextsAndEmptyFills() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    assert(cairo_in_fill(nullptr, 1, 1) == 0);
    CairoContext* error = cairo_create(nullptr);
    assert(cairo_in_fill(error, 1, 1) == 0);
    cairo_destroy(error);

    CairoContext* c = NewContext();
    Expect("no path", c, {{0, 0, 0}});
    cairo_move_to(c, 25, 25);
    Expect("lone move", c, {{25, 25, 0}});
    cairo_line_to(c, 25, 25);
    Expect("zero-length line", c, {{25, 25, 0}});
    cairo_close_path(c);
    Expect("closed point", c, {{25, 25, 0}, {nan, infinity, 0}});
    cairo_rectangle(c, 0, 0, 10, 10);
    Expect("rectangle after a closed point", c, {{5, 5, 1}, {20, 20, 0}, {25, 25, 1}, {25.01, 25, 0}});
    cairo_restore(c);
    Expect("context in an error state", c, {{5, 5, 0}, {nan, nan, 0}});
    cairo_destroy(c);
}

void CheckRectangleEdgesAndQuantization() {
    CairoContext* c = NewContext();
    cairo_rectangle(c, 0, 0, 10, 10);
    const int elements = PathElementCount(c);
    Expect("rectangle interior and exterior", c,
           {{5, 5, 1}, {15, 5, 0}, {-1, 5, 0}, {5, -1, 0}, {5, 11, 0}});
    Expect("rectangle edges and corners", c,
           {{0, 5, 1}, {10, 5, 1}, {5, 0, 1}, {5, 10, 1}, {0, 0, 1}, {10, 0, 1}, {10, 10, 1}, {0, 10, 1}});
    Expect("query rounded to 1/256 with ties to even", c,
           {{10.001, 5, 1}, {10.002, 5, 0}, {10 + 0.5 / 256, 5, 1}, {10 + 1.5 / 256, 5, 0},
            {-0.5 / 256, 5, 1}, {-1.5 / 256, 5, 0}});
    Expect("query wrapped past the 24.8 range", c, {{16777216.0 + 5, 5, 1}, {16777216.0 + 15, 5, 0}});
    assert(PathElementCount(c) == elements);
    assert(cairo_has_current_point(c) == 1);
    cairo_destroy(c);

    c = NewContext();
    cairo_rectangle(c, 1000000, 1000000, 10, 10);
    Expect("rectangle far from the origin", c, {{1000005, 1000005, 1}, {1000011, 1000005, 0}});
    cairo_destroy(c);
}

void CheckImplicitCloseAndFillRules() {
    CairoContext* c = NewContext();
    cairo_move_to(c, 0, 0);
    cairo_line_to(c, 10, 0);
    cairo_line_to(c, 10, 10);
    Expect("open triangle", c, {{7, 3, 1}, {3, 7, 0}, {5, 5, 1}});

    cairo_new_path(c);
    cairo_rectangle(c, 0, 0, 30, 30);
    cairo_rectangle(c, 10, 10, 10, 10);
    assert(cairo_get_fill_rule(c) == kWinding);
    Expect("nested rectangles, winding", c, {{15, 15, 1}, {5, 5, 1}, {35, 15, 0}, {10, 15, 1}});
    cairo_save(c);
    cairo_set_fill_rule(c, kEvenOdd);
    Expect("nested rectangles, even-odd", c, {{15, 15, 0}, {5, 5, 1}, {35, 15, 0}, {10, 15, 1}});
    cairo_restore(c);
    Expect("nested rectangles, restored winding", c, {{15, 15, 1}});

    cairo_new_path(c);
    cairo_rectangle(c, 0, 0, 30, 30);
    cairo_move_to(c, 10, 10);
    cairo_line_to(c, 10, 20);
    cairo_line_to(c, 20, 20);
    cairo_line_to(c, 20, 10);
    cairo_close_path(c);
    Expect("reversed inner rectangle, winding", c, {{15, 15, 0}, {5, 5, 1}, {20, 15, 1}});
    cairo_set_fill_rule(c, kEvenOdd);
    Expect("reversed inner rectangle, even-odd", c, {{15, 15, 0}, {5, 5, 1}});
    cairo_destroy(c);
}

void CheckLonePointsAndPendingMoves() {
    CairoContext* c = NewContext();
    cairo_rectangle(c, 0, 0, 10, 10);
    cairo_move_to(c, 50, 50);
    Expect("pending move", c, {{50, 50, 0}, {5, 5, 1}});
    cairo_line_to(c, 50, 50);
    Expect("lone point of a filled path", c, {{50, 50, 1}, {50.01, 50, 0}, {5, 5, 1}});
    cairo_destroy(c);
}

void CheckTransforms() {
    CairoContext* c = NewContext();
    cairo_translate(c, 100, 50);
    cairo_rectangle(c, 0, 0, 10, 10);
    Expect("translated query", c, {{5, 5, 1}, {105, 55, 0}});
    cairo_identity_matrix(c);
    Expect("path kept in device space", c, {{105, 55, 1}, {5, 5, 0}});
    cairo_scale(c, 2, 3);
    Expect("scaled query", c, {{52.5, 18, 1}, {56, 18, 0}, {52.5, 21, 0}});
    cairo_destroy(c);

    c = NewContext(2);
    cairo_rectangle(c, 0, 0, 10, 10);
    Expect("device scale", c, {{9, 9, 1}, {10, 10, 1}, {11, 5, 0}});
    cairo_destroy(c);
}

void CheckCurves() {
    CairoContext* c = NewContext();
    cairo_move_to(c, 0, 0);
    cairo_curve_to(c, 0, 100, 100, 100, 100, 0);
    Expect("arch", c, {{50, 50, 1}, {50, 74.75, 1}, {50, 75, 1}, {50, 75.25, 0}, {50, 80, 0},
                       {50, 0, 1}, {50, -1, 0}, {-1, 50, 0}, {101, 50, 0}, {2, 60, 0}, {98, 60, 0}});

    cairo_new_path(c);
    cairo_rectangle(c, 0, 0, 10, 10);
    cairo_move_to(c, 20, 0);
    cairo_curve_to(c, 20, 5, 30, 5, 30, 0);
    Expect("curve outside the query row or to its right", c, {{5, 7, 1}, {5, 2, 1}, {15, 2, 0}});
    Expect("curve beside a rectangle", c, {{25, 2, 1}, {25, 4.5, 0}, {35, 2, 0}, {25, 3.75, 1}});
    cairo_destroy(c);
}

void CheckCurvesWithoutTangentsAreDropped() {
    CairoContext* lines = NewContext();
    cairo_move_to(lines, 0, 0);
    cairo_line_to(lines, 10, 0);
    cairo_line_to(lines, 10, 10);
    cairo_close_path(lines);
    Expect("triangle from lines", lines, {{7, 3, 1}, {5, 0, 1}, {10, 0, 1}, {0, 0, 1}});
    cairo_destroy(lines);

    CairoContext* curve = NewContext();
    cairo_move_to(curve, 0, 0);
    cairo_curve_to(curve, 10, 0, 10, 0, 10, 0);
    cairo_line_to(curve, 10, 10);
    cairo_close_path(curve);
    Expect("triangle whose base is a curve ending in its controls", curve,
           {{7, 3, 1}, {5, 0, 0}, {10, 0, 0}, {0, 0, 1}, {5, 5, 1}});
    cairo_destroy(curve);

    CairoContext* both = NewContext();
    cairo_move_to(both, 0, 0);
    cairo_curve_to(both, 0, 0, 10, 0, 10, 0);
    cairo_line_to(both, 10, 10);
    cairo_close_path(both);
    Expect("triangle whose base is a curve with both controls on its ends", both,
           {{7, 3, 1}, {5, 0, 0}, {10, 0, 0}, {0, 0, 1}});
    cairo_destroy(both);
}

void CheckUnmodelledInputsThrow() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    CairoContext* c = NewContext();
    cairo_rectangle(c, 0, 0, 10, 10);
    assert(Throws(c, nan, 5));
    assert(Throws(c, 5, infinity));
    assert(Throws(c, -infinity, 5));
    cairo_scale(c, 1e150, 1e150);
    assert(Throws(c, 1e200, 0));
    cairo_identity_matrix(c);
    cairo_set_fill_rule(c, 7);
    assert(Throws(c, 5, 5));
    assert(Throws(c, 20, 5));
    Expect("edge found before the fill rule is read", c, {{10, 5, 1}});
    cairo_set_fill_rule(c, kEvenOdd);
    Expect("valid rule after an invalid one", c, {{5, 5, 1}, {20, 5, 0}});
    cairo_destroy(c);
}

int Cross(const Point& a, const Point& b, const Point& q) {
    const double value = (b.x - a.x) * (q.y - a.y) - (q.x - a.x) * (b.y - a.y);
    return value > 0 ? 1 : value < 0 ? -1 : 0;
}

bool OnSegment(const Point& a, const Point& b, const Point& q) {
    return Cross(a, b, q) == 0 && q.x >= std::min(a.x, b.x) && q.x <= std::max(a.x, b.x) &&
           q.y >= std::min(a.y, b.y) && q.y <= std::max(a.y, b.y);
}

double DistanceToSegment(const Point& a, const Point& b, const Point& q) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length = dx * dx + dy * dy;
    double t = length == 0 ? 0 : ((q.x - a.x) * dx + (q.y - a.y) * dy) / length;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(q.x - (a.x + t * dx), q.y - (a.y + t * dy));
}

struct Reference {
    bool onBoundary;
    double boundaryDistance;
    int winding;
};

Reference Classify(const std::vector<std::vector<Point>>& polygons, const Point& q) {
    Reference result{false, std::numeric_limits<double>::infinity(), 0};
    for (const auto& polygon : polygons) {
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const Point& a = polygon[i];
            const Point& b = polygon[(i + 1) % polygon.size()];
            result.onBoundary = result.onBoundary || OnSegment(a, b, q);
            result.boundaryDistance = std::min(result.boundaryDistance, DistanceToSegment(a, b, q));
            if (a.y <= q.y) {
                if (b.y > q.y && Cross(a, b, q) > 0) ++result.winding;
            } else if (b.y <= q.y && Cross(a, b, q) < 0) {
                --result.winding;
            }
        }
    }
    return result;
}

void CheckPolygonsAgainstWindingNumbers() {
    std::mt19937 random(0x5eed);
    const auto integer = [&](int minimum, int maximum) {
        return minimum + static_cast<int>(random() % static_cast<unsigned>(maximum - minimum + 1));
    };
    CairoContext* c = NewContext();
    int inside = 0, outside = 0, boundary = 0;
    for (int iteration = 0; iteration < 3000; ++iteration) {
        cairo_new_path(c);
        std::vector<std::vector<Point>> polygons;
        const int count = integer(1, 3);
        for (int polygonIndex = 0; polygonIndex < count; ++polygonIndex) {
            std::vector<Point> polygon;
            const int vertices = integer(1, 6);
            for (int i = 0; i < vertices; ++i) {
                const Point point{static_cast<double>(integer(-6, 6)), static_cast<double>(integer(-6, 6))};
                polygon.push_back(point);
                if (i == 0) cairo_move_to(c, point.x, point.y);
                else cairo_line_to(c, point.x, point.y);
            }
            const bool closed = integer(0, 1) == 1;
            if (closed) cairo_close_path(c);
            if (closed || vertices > 1) polygons.push_back(polygon);
        }
        bool moves = false;
        for (const auto& polygon : polygons)
            for (const Point& point : polygon)
                moves = moves || point.x != polygon[0].x || point.y != polygon[0].y;
        const int rule = integer(0, 1);
        cairo_set_fill_rule(c, rule);
        for (int probe = 0; probe < 12; ++probe) {
            Point q{integer(-28, 28) / 4.0, integer(-28, 28) / 4.0};
            if (!polygons.empty() && probe % 3 == 0) {
                const auto& polygon = polygons[static_cast<std::size_t>(integer(0, static_cast<int>(polygons.size()) - 1))];
                const std::size_t index = static_cast<std::size_t>(integer(0, static_cast<int>(polygon.size()) - 1));
                const Point& a = polygon[index];
                const Point& b = polygon[(index + 1) % polygon.size()];
                q = probe % 2 == 0 ? a : Point{(a.x + b.x) / 2, (a.y + b.y) / 2};
            }
            const Reference reference = Classify(polygons, q);
            int expected = 0;
            if (moves && reference.onBoundary) expected = 1;
            else if (moves) expected = rule == kEvenOdd ? reference.winding & 1 : reference.winding != 0;
            const int actual = cairo_in_fill(c, q.x, q.y);
            if (actual != expected) {
                std::fprintf(stderr, "polygon %d: cairo_in_fill(%g, %g) returned %d, expected %d\n",
                             iteration, q.x, q.y, actual, expected);
                std::abort();
            }
            if (reference.onBoundary) ++boundary;
            else if (expected) ++inside;
            else ++outside;
        }
    }
    assert(inside > 1000 && outside > 10000 && boundary > 5000);
    cairo_destroy(c);
}

void CheckCurvesAgainstFineFlattening() {
    std::mt19937 random(0xc0ffee);
    const auto integer = [&](int minimum, int maximum) {
        return minimum + static_cast<int>(random() % static_cast<unsigned>(maximum - minimum + 1));
    };
    CairoContext* c = NewContext();
    int inside = 0, outside = 0;
    for (int iteration = 0; iteration < 300; ++iteration) {
        cairo_new_path(c);
        Point knots[4];
        for (Point& knot : knots) knot = {static_cast<double>(integer(-40, 40)), static_cast<double>(integer(-40, 40))};
        const bool distinct = [&] {
            for (int i = 0; i < 4; ++i)
                for (int j = i + 1; j < 4; ++j)
                    if (knots[i].x == knots[j].x && knots[i].y == knots[j].y) return false;
            return true;
        }();
        if (!distinct) continue;
        cairo_move_to(c, knots[0].x, knots[0].y);
        cairo_curve_to(c, knots[1].x, knots[1].y, knots[2].x, knots[2].y, knots[3].x, knots[3].y);
        std::vector<Point> fine;
        constexpr int kSteps = 512;
        for (int step = 0; step <= kSteps; ++step) {
            const double t = static_cast<double>(step) / kSteps, u = 1 - t;
            const auto cubic = [&](double p0, double p1, double p2, double p3) {
                return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
            };
            fine.push_back({cubic(knots[0].x, knots[1].x, knots[2].x, knots[3].x),
                            cubic(knots[0].y, knots[1].y, knots[2].y, knots[3].y)});
        }
        const int rule = integer(0, 1);
        cairo_set_fill_rule(c, rule);
        for (int probe = 0; probe < 24; ++probe) {
            Point q{integer(-180, 180) / 4.0, integer(-180, 180) / 4.0};
            if (probe % 2 == 0) {
                const Point& a = fine[static_cast<std::size_t>(integer(0, kSteps))];
                const Point& b = fine[static_cast<std::size_t>(integer(0, kSteps))];
                q = {std::round((a.x + b.x) * 2) / 4, std::round((a.y + b.y) * 2) / 4};
            }
            const Reference reference = Classify({fine}, q);
            if (reference.boundaryDistance < 0.75) continue;
            const int expected = rule == kEvenOdd ? reference.winding & 1 : reference.winding != 0;
            const int actual = cairo_in_fill(c, q.x, q.y);
            if (actual != expected) {
                std::fprintf(stderr, "curve %d: cairo_in_fill(%g, %g) returned %d, expected %d\n",
                             iteration, q.x, q.y, actual, expected);
                std::abort();
            }
            if (expected) ++inside;
            else ++outside;
        }
    }
    assert(inside > 800 && outside > 1800);
    cairo_destroy(c);
}

}

int main() {
    CheckInvalidContextsAndEmptyFills();
    CheckRectangleEdgesAndQuantization();
    CheckImplicitCloseAndFillRules();
    CheckLonePointsAndPendingMoves();
    CheckTransforms();
    CheckCurves();
    CheckCurvesWithoutTangentsAreDropped();
    CheckUnmodelledInputsThrow();
    CheckPolygonsAgainstWindingNumbers();
    CheckCurvesAgainstFineFlattening();
    std::puts("guest Cairo in-fill tests passed");
}
