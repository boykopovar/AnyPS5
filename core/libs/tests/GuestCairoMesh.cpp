#ifdef CAIRO_MESH_NATIVE_ORACLE
struct cairo_pattern_t;

union cairo_path_data_t {
    struct {
        int type;
        int length;
    } header;
    struct {
        double x;
        double y;
    } point;
};

struct cairo_path_t {
    int status;
    cairo_path_data_t* data;
    int num_data;
};

constexpr int CAIRO_STATUS_SUCCESS = 0;

#define NativeCairo(name) cairo_##name

extern "C" {
cairo_pattern_t* NativeCairo(pattern_create_mesh)();
cairo_pattern_t* NativeCairo(pattern_create_linear)(double, double, double, double);
cairo_pattern_t* NativeCairo(pattern_reference)(cairo_pattern_t*);
void NativeCairo(pattern_destroy)(cairo_pattern_t*);
int NativeCairo(pattern_status)(cairo_pattern_t*);
int NativeCairo(pattern_get_type)(cairo_pattern_t*);
unsigned int NativeCairo(pattern_get_reference_count)(cairo_pattern_t*);
void NativeCairo(mesh_pattern_begin_patch)(cairo_pattern_t*);
void NativeCairo(mesh_pattern_end_patch)(cairo_pattern_t*);
void NativeCairo(mesh_pattern_move_to)(cairo_pattern_t*, double, double);
void NativeCairo(mesh_pattern_line_to)(cairo_pattern_t*, double, double);
void NativeCairo(mesh_pattern_curve_to)(cairo_pattern_t*, double, double, double, double, double, double);
void NativeCairo(mesh_pattern_set_corner_color_rgba)(cairo_pattern_t*, unsigned int, double, double, double, double);
int NativeCairo(mesh_pattern_get_patch_count)(cairo_pattern_t*, unsigned int*);
cairo_path_t* NativeCairo(mesh_pattern_get_path)(cairo_pattern_t*, unsigned int);
int NativeCairo(mesh_pattern_get_control_point)(cairo_pattern_t*, unsigned int, unsigned int, double*, double*);
int NativeCairo(mesh_pattern_get_corner_color_rgba)(cairo_pattern_t*, unsigned int, unsigned int,
                                                    double*, double*, double*, double*);
void NativeCairo(path_destroy)(cairo_path_t*);
}
#define CairoPattern cairo_pattern_t
#else
#include "prx/libcairo/include/Pattern.hpp"
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <type_traits>

#ifndef CAIRO_MESH_NATIVE_ORACLE
static_assert(std::is_same_v<decltype(&cairo_pattern_create_mesh), CairoPattern* (APS5_VABI*)()>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_begin_patch), void (APS5_VABI*)(CairoPattern*)>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_end_patch), void (APS5_VABI*)(CairoPattern*)>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_move_to), void (APS5_VABI*)(CairoPattern*, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_line_to), void (APS5_VABI*)(CairoPattern*, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_curve_to),
                             void (APS5_VABI*)(CairoPattern*, double, double, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_mesh_pattern_set_corner_color_rgba),
                             void (APS5_VABI*)(CairoPattern*, unsigned int, double, double, double, double)>);
#endif

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "mesh check failed at %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

struct Point {
    double x;
    double y;
};

struct Color {
    double red;
    double green;
    double blue;
    double alpha;
};

constexpr int kPathI[12]{0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1};
constexpr int kPathJ[12]{0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
constexpr int kControlI[4]{1, 1, 2, 2};
constexpr int kControlJ[4]{1, 2, 2, 1};

int Status(const CairoPattern* pattern) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    return static_cast<int>(cairo_pattern_status(const_cast<CairoPattern*>(pattern)));
#else
    return pattern->status;
#endif
}

int Type(const CairoPattern* pattern) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    return static_cast<int>(cairo_pattern_get_type(const_cast<CairoPattern*>(pattern)));
#else
    return pattern->type;
#endif
}

unsigned int Count(const CairoPattern* pattern) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    unsigned int count = 0;
    Require(cairo_mesh_pattern_get_patch_count(const_cast<CairoPattern*>(pattern), &count) == CAIRO_STATUS_SUCCESS);
    return count;
#else
    Require(pattern->patches.size() >= static_cast<std::size_t>(pattern->patchActive));
    return static_cast<unsigned int>(pattern->patches.size() - static_cast<std::size_t>(pattern->patchActive));
#endif
}

unsigned int References(const CairoPattern* pattern) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    return cairo_pattern_get_reference_count(const_cast<CairoPattern*>(pattern));
#else
    return static_cast<unsigned int>(pattern->refCount.load());
#endif
}

Point Perimeter(CairoPattern* pattern, unsigned int patchIndex, int index) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    cairo_path_t* path = cairo_mesh_pattern_get_path(pattern, patchIndex);
    Require(path && path->status == CAIRO_STATUS_SUCCESS && path->num_data == 18);
    const int offset = index == 0 ? 1 : 3 + (index - 1) / 3 * 4 + (index - 1) % 3;
    const Point point{path->data[offset].point.x, path->data[offset].point.y};
    cairo_path_destroy(path);
    return point;
#else
    const auto& point = pattern->patches[patchIndex].points[kPathI[index]][kPathJ[index]];
    return {point.x, point.y};
#endif
}

Point Control(CairoPattern* pattern, unsigned int patchIndex, unsigned int index) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    double x = 0, y = 0;
    Require(cairo_mesh_pattern_get_control_point(pattern, patchIndex, index, &x, &y) == CAIRO_STATUS_SUCCESS);
    return {x, y};
#else
    const auto& point = pattern->patches[patchIndex].points[kControlI[index]][kControlJ[index]];
    return {point.x, point.y};
#endif
}

Color Corner(CairoPattern* pattern, unsigned int patchIndex, unsigned int index) {
#ifdef CAIRO_MESH_NATIVE_ORACLE
    double r = 0, g = 0, b = 0, a = 0;
    Require(cairo_mesh_pattern_get_corner_color_rgba(pattern, patchIndex, index, &r, &g, &b, &a) ==
            CAIRO_STATUS_SUCCESS);
    return {r, g, b, a};
#else
    const auto& color = pattern->patches[patchIndex].colors[index];
    return {color.red, color.green, color.blue, color.alpha};
#endif
}

void Snapshot(CairoPattern* pattern, int caseIndex, unsigned int expectedCount) {
    Require(Status(pattern) == 0);
    Require(Type(pattern) == 4);
    Require(Count(pattern) == expectedCount);
    std::printf("case %d type %d status %d patches %u refs %u\n",
                caseIndex, Type(pattern), Status(pattern), Count(pattern), References(pattern));
    for (unsigned int patch = 0; patch < expectedCount; ++patch) {
        for (int point = 0; point < 12; ++point) {
            const Point p = Perimeter(pattern, patch, point);
            Require(std::isfinite(p.x) && std::isfinite(p.y));
            std::printf("p %u %d %.7f %.7f\n", patch, point, p.x, p.y);
        }
        for (unsigned int control = 0; control < 4; ++control) {
            const Point p = Control(pattern, patch, control);
            Require(std::isfinite(p.x) && std::isfinite(p.y));
            std::printf("i %u %u %.7f %.7f\n", patch, control, p.x, p.y);
            const Color c = Corner(pattern, patch, control);
            std::printf("c %u %u %.7f %.7f %.7f %.7f\n", patch, control,
                        c.red, c.green, c.blue, c.alpha);
        }
    }
}

void TestSquare() {
    CairoPattern* pattern = cairo_pattern_create_mesh();
    Require(pattern && Status(pattern) == 0 && Type(pattern) == 4 && References(pattern) == 1);
    cairo_mesh_pattern_begin_patch(pattern);
    cairo_mesh_pattern_move_to(pattern, 0, 0);
    cairo_mesh_pattern_line_to(pattern, 10, 0);
    cairo_mesh_pattern_line_to(pattern, 10, 10);
    cairo_mesh_pattern_line_to(pattern, 0, 10);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 1, 0, 0, 1);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 1, 0, 1, 0, 1);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 2, 0, 0, 1, 1);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 3, 1, 1, 0, 1);
    cairo_mesh_pattern_end_patch(pattern);
    Snapshot(pattern, 0, 1);
    const Point first = Perimeter(pattern, 0, 1);
    Require(std::abs(first.x - 10. / 3) < 1e-10 && first.y == 0);
    cairo_pattern_reference(pattern);
    Require(References(pattern) == 2);
    cairo_pattern_destroy(pattern);
    Require(References(pattern) == 1);
    cairo_pattern_destroy(pattern);
}

void TestMixedAndPartial() {
    CairoPattern* pattern = cairo_pattern_create_mesh();
    cairo_mesh_pattern_begin_patch(pattern);
    cairo_mesh_pattern_move_to(pattern, 4, 5);
    cairo_mesh_pattern_line_to(pattern, 12, 5);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 0, 0, 0, 0);
    cairo_mesh_pattern_end_patch(pattern);
    Snapshot(pattern, 1, 1);
    const Point last = Perimeter(pattern, 0, 11);
    Require(last.x == 4 && last.y == 5);

    cairo_mesh_pattern_begin_patch(pattern);
    cairo_mesh_pattern_curve_to(pattern, 2, 3, 8, 14, 12, 8);
    cairo_mesh_pattern_curve_to(pattern, 18, -7, 22, -4, 30, 5);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 2.5, -1, 0.35, 0.8);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 1, -1, 1.1, 0.2, 1);
    cairo_mesh_pattern_end_patch(pattern);
    Snapshot(pattern, 2, 2);
    const Color first = Corner(pattern, 1, 0);
    Require(first.red == 1 && first.green == 0 && first.blue == 0.35 && first.alpha == 0.8);
    const Color third = Corner(pattern, 1, 3);
    Require(third.red == 1 && third.green == 0 && third.blue == 0.35 && third.alpha == 0.8);
    cairo_pattern_destroy(pattern);
}

void TestRandomized() {
    std::uint32_t seed = 0x2496c0dfU;
    auto next = [&seed] {
        seed = seed * 1664525U + 1013904223U;
        return (static_cast<int>((seed >> 8) % 2001U) - 1000) / 37.0;
    };
    for (int i = 0; i < 64; ++i) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        cairo_mesh_pattern_begin_patch(pattern);
        const int operations = i % 5;
        if (operations == 0) {
            cairo_mesh_pattern_line_to(pattern, next(), next());
        } else if (operations == 1) {
            cairo_mesh_pattern_curve_to(pattern, next(), next(), next(), next(), next(), next());
        } else {
            cairo_mesh_pattern_move_to(pattern, next(), next());
        }
        for (int j = 0; j < operations && j < 4; ++j) {
            if (j % 2)
                cairo_mesh_pattern_curve_to(pattern, next(), next(), next(), next(), next(), next());
            else
                cairo_mesh_pattern_line_to(pattern, next(), next());
        }
        if (i % 3 == 0)
            cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, next(), next(), next(), next());
        else
            cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 0, 0, 0, 0);
        if (i % 7 == 0)
            cairo_mesh_pattern_set_corner_color_rgba(pattern, 3, next(), next(), next(), next());
        cairo_mesh_pattern_end_patch(pattern);
        Snapshot(pattern, i + 100, 1);
        cairo_pattern_destroy(pattern);
    }
}

void TestErrors() {
    auto scenario = [](int id, auto modify, int expected) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        modify(pattern);
        Require(Status(pattern) == expected);
        std::printf("error %d %d\n", id, Status(pattern));
        cairo_mesh_pattern_begin_patch(pattern);
        Require(Status(pattern) == expected);
        cairo_pattern_destroy(pattern);
    };
    scenario(0, [](CairoPattern* p) { cairo_mesh_pattern_end_patch(p); }, 36);
    scenario(1, [](CairoPattern* p) {
        cairo_mesh_pattern_begin_patch(p);
        cairo_mesh_pattern_begin_patch(p);
    }, 36);
    scenario(2, [](CairoPattern* p) {
        cairo_mesh_pattern_begin_patch(p);
        cairo_mesh_pattern_end_patch(p);
    }, 36);
    scenario(3, [](CairoPattern* p) {
        cairo_mesh_pattern_move_to(p, 1, 2);
    }, 36);
    scenario(5, [](CairoPattern* p) {
        cairo_mesh_pattern_begin_patch(p);
        cairo_mesh_pattern_set_corner_color_rgba(p, 4, 0, 0, 0, 1);
    }, 21);
    scenario(6, [](CairoPattern* p) {
        cairo_mesh_pattern_set_corner_color_rgba(p, 0, 0, 0, 0, 1);
    }, 36);
    scenario(7, [](CairoPattern* p) {
        cairo_mesh_pattern_begin_patch(p);
        cairo_mesh_pattern_move_to(p, 0, 0);
        for (int i = 0; i < 5; ++i)
            cairo_mesh_pattern_line_to(p, i + 1, i + 1);
    }, 36);
    CairoPattern* gradient = cairo_pattern_create_linear(0, 0, 1, 1);
    cairo_mesh_pattern_begin_patch(gradient);
    Require(Status(gradient) == 14);
    cairo_mesh_pattern_curve_to(gradient, 0, 0, 1, 0, 1, 1);
    Require(Status(gradient) == 14);
    std::printf("error gradient %d\n", Status(gradient));
    cairo_pattern_destroy(gradient);
#ifndef CAIRO_MESH_NATIVE_ORACLE
    cairo_mesh_pattern_begin_patch(nullptr);
    cairo_mesh_pattern_end_patch(nullptr);
    cairo_mesh_pattern_move_to(nullptr, 0, 0);
    cairo_mesh_pattern_line_to(nullptr, 0, 0);
    cairo_mesh_pattern_curve_to(nullptr, 0, 0, 0, 0, 0, 0);
    cairo_mesh_pattern_set_corner_color_rgba(nullptr, 0, 0, 0, 0, 0);
#endif
}

}

int main() {
    TestSquare();
    TestMixedAndPartial();
    TestRandomized();
    TestErrors();
    return 0;
}
