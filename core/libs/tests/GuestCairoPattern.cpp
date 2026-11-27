#include "prx/libcairo/include/Pattern.hpp"
#include "prx/libcairo/include/SurfacePattern.hpp"

#include <CairoBackend.h>

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
extern "C" void _cairo_mutex_initialize();
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(std::is_same_v<decltype(&cairo_pattern_create_linear),
    CairoPattern* (APS5_VABI*)(double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_create_radial),
    CairoPattern* (APS5_VABI*)(double, double, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_reference), CairoPattern* (APS5_VABI*)(CairoPattern*)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_destroy), void (APS5_VABI*)(CairoPattern*)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_add_color_stop_rgba),
    void (APS5_VABI*)(CairoPattern*, double, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_set_matrix), void (APS5_VABI*)(CairoPattern*, const CairoMatrix*)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_set_filter), void (APS5_VABI*)(CairoPattern*, int)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_set_extend), void (APS5_VABI*)(CairoPattern*, int)>);

namespace {

constexpr int Size = 64;

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

void Near(double value, double expected) {
    assert(std::abs(value - expected) < 1e-12);
}

void Stop(CairoPattern* pattern, int index, const std::array<double, 5>& expected) {
    std::array<double, 5> actual{};
    assert(aps5_cairo_pattern_get_color_stop_rgba(Backend(pattern), index,
        &actual[0], &actual[1], &actual[2], &actual[3], &actual[4]) == CAIRO_STATUS_SUCCESS);
    for (std::size_t i = 0; i < actual.size(); ++i) Near(actual[i], expected[i]);
}

int StopCount(CairoPattern* pattern) {
    int count = -1;
    assert(aps5_cairo_pattern_get_color_stop_count(Backend(pattern), &count) == CAIRO_STATUS_SUCCESS);
    return count;
}

void Gradients() {
    CairoPattern* linear = cairo_pattern_create_linear(1.5, -2, 30, 40.25);
    assert(linear && aps5_cairo_pattern_status(Backend(linear)) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_pattern_get_type(Backend(linear)) == CAIRO_PATTERN_TYPE_LINEAR);
    std::array<double, 4> points{};
    assert(aps5_cairo_pattern_get_linear_points(Backend(linear), &points[0], &points[1], &points[2], &points[3])
        == CAIRO_STATUS_SUCCESS);
    Near(points[0], 1.5);
    Near(points[1], -2);
    Near(points[2], 30);
    Near(points[3], 40.25);
    assert(StopCount(linear) == 0);
    cairo_pattern_add_color_stop_rgba(linear, 0.75, 0.125, 0.25, 0.5, 1);
    cairo_pattern_add_color_stop_rgba(linear, -3, 2, -1, 0.5, 0.25);
    cairo_pattern_add_color_stop_rgba(linear, 7, 0.5, 1.5, -0.5, 9);
    assert(StopCount(linear) == 3);
    Stop(linear, 0, {0, 1, 0, 0.5, 0.25});
    Stop(linear, 1, {0.75, 0.125, 0.25, 0.5, 1});
    Stop(linear, 2, {1, 0.5, 1, 0, 1});
    assert(aps5_cairo_pattern_status(Backend(linear)) == CAIRO_STATUS_SUCCESS);

    CairoPattern* radial = cairo_pattern_create_radial(3, 4, 0.5, 10, 11, 20);
    assert(radial && aps5_cairo_pattern_status(Backend(radial)) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_pattern_get_type(Backend(radial)) == CAIRO_PATTERN_TYPE_RADIAL);
    std::array<double, 6> circles{};
    assert(aps5_cairo_pattern_get_radial_circles(Backend(radial), &circles[0], &circles[1], &circles[2],
        &circles[3], &circles[4], &circles[5]) == CAIRO_STATUS_SUCCESS);
    const std::array<double, 6> expected{3, 4, 0.5, 10, 11, 20};
    for (std::size_t i = 0; i < circles.size(); ++i) Near(circles[i], expected[i]);
    cairo_pattern_add_color_stop_rgba(radial, 0.5, 0.25, 0.5, 0.75, 0.5);
    cairo_pattern_add_color_stop_rgba(radial, 0.5, 1, 1, 1, 1);
    assert(StopCount(radial) == 2);
    Stop(radial, 0, {0.5, 0.25, 0.5, 0.75, 0.5});
    Stop(radial, 1, {0.5, 1, 1, 1, 1});

    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_pattern_add_color_stop_rgba(mesh, 0.5, 1, 0, 0, 1);
    assert(aps5_cairo_pattern_status(Backend(mesh)) == CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2);
    CairoPattern* image = cairo_pattern_create_for_surface(surface);
    cairo_pattern_add_color_stop_rgba(image, 0.5, 1, 0, 0, 1);
    assert(aps5_cairo_pattern_status(Backend(image)) == CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    CairoPattern* singular = cairo_pattern_create_linear(0, 0, 1, 0);
    const CairoMatrix zero{0, 0, 0, 0, 0, 0};
    cairo_pattern_set_matrix(singular, &zero);
    assert(aps5_cairo_pattern_status(Backend(singular)) == CAIRO_STATUS_INVALID_MATRIX);
    cairo_pattern_add_color_stop_rgba(singular, 0.5, 1, 0, 0, 1);
    assert(aps5_cairo_pattern_status(Backend(singular)) == CAIRO_STATUS_INVALID_MATRIX);

    for (CairoPattern* pattern : {linear, radial, mesh, image, singular}) cairo_pattern_destroy(pattern);
    cairo_surface_destroy(surface);
}

void State() {
    CairoPattern* linear = cairo_pattern_create_linear(0, 0, 10, 0);
    CairoPattern* radial = cairo_pattern_create_radial(0, 0, 1, 0, 0, 5);
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2);
    CairoPattern* image = cairo_pattern_create_for_surface(surface);
    assert(aps5_cairo_pattern_get_extend(Backend(linear)) == CAIRO_EXTEND_PAD);
    assert(aps5_cairo_pattern_get_extend(Backend(radial)) == CAIRO_EXTEND_PAD);
    assert(aps5_cairo_pattern_get_extend(Backend(image)) == CAIRO_EXTEND_NONE);
    for (CairoPattern* pattern : {linear, radial, image}) {
        for (int extend : {CAIRO_EXTEND_REPEAT, CAIRO_EXTEND_REFLECT, CAIRO_EXTEND_NONE, CAIRO_EXTEND_PAD}) {
            cairo_pattern_set_extend(pattern, extend);
            assert(aps5_cairo_pattern_get_extend(Backend(pattern)) == extend);
        }
        for (int filter : {CAIRO_FILTER_FAST, CAIRO_FILTER_BEST, CAIRO_FILTER_NEAREST,
                           CAIRO_FILTER_BILINEAR, CAIRO_FILTER_GAUSSIAN, CAIRO_FILTER_GOOD}) {
            cairo_pattern_set_filter(pattern, filter);
            assert(aps5_cairo_pattern_get_filter(Backend(pattern)) == filter);
        }
        const CairoMatrix matrix{2, 0.5, -0.25, 4, 7, -9};
        cairo_pattern_set_matrix(pattern, &matrix);
        cairo_matrix_t actual{};
        aps5_cairo_pattern_get_matrix(Backend(pattern), &actual);
        Near(actual.xx, 2);
        Near(actual.yx, 0.5);
        Near(actual.xy, -0.25);
        Near(actual.yy, 4);
        Near(actual.x0, 7);
        Near(actual.y0, -9);
        cairo_pattern_set_matrix(pattern, &matrix);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
    }

    const CairoMatrix singular{1, 2, 2, 4, 0, 0};
    cairo_pattern_set_matrix(linear, &singular);
    assert(aps5_cairo_pattern_status(Backend(linear)) == CAIRO_STATUS_INVALID_MATRIX);
    cairo_pattern_set_extend(linear, CAIRO_EXTEND_REFLECT);
    cairo_pattern_set_filter(linear, CAIRO_FILTER_NEAREST);
    assert(aps5_cairo_pattern_get_extend(Backend(linear)) == CAIRO_EXTEND_PAD);
    assert(aps5_cairo_pattern_get_filter(Backend(linear)) == CAIRO_FILTER_GOOD);

    for (CairoPattern* pattern : {linear, radial, image}) cairo_pattern_destroy(pattern);
    cairo_surface_destroy(surface);
}

void Lifetime() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 1);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 1);
    assert(cairo_pattern_reference(pattern) == pattern);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 2);
    assert(cairo_pattern_reference(pattern) == pattern);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 3);
    cairo_pattern_destroy(pattern);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 2);
    cairo_pattern_destroy(pattern);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 1);
    cairo_pattern_destroy(pattern);

    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2);
    CairoPattern* image = cairo_pattern_create_for_surface(surface);
    cairo_surface_destroy(surface);
    cairo_surface_t* held = nullptr;
    assert(aps5_cairo_pattern_get_surface(Backend(image), &held) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_surface_get_reference_count(held) == 1);
    assert(cairo_pattern_reference(image) == image);
    cairo_pattern_destroy(image);
    assert(aps5_cairo_surface_get_reference_count(held) == 1);
    aps5_cairo_surface_reference(held);
    cairo_pattern_destroy(image);
    assert(aps5_cairo_surface_get_reference_count(held) == 1);
    aps5_cairo_surface_destroy(held);

    CairoPattern* mesh = cairo_pattern_create_mesh();
    assert(cairo_pattern_reference(mesh) == mesh);
    assert(aps5_cairo_pattern_get_reference_count(Backend(mesh)) == 2);
    cairo_pattern_destroy(mesh);
    cairo_pattern_destroy(mesh);

    assert(cairo_pattern_reference(nullptr) == nullptr);
    cairo_pattern_destroy(nullptr);
    CairoPattern* nil = cairo_pattern_create_for_surface(nullptr);
    assert(aps5_cairo_pattern_status(Backend(nil)) == CAIRO_STATUS_NULL_POINTER);
    assert(aps5_cairo_pattern_get_reference_count(Backend(nil)) == 0);
    assert(cairo_pattern_reference(nil) == nil);
    cairo_pattern_destroy(nil);
    cairo_pattern_destroy(nil);
    assert(aps5_cairo_pattern_get_reference_count(Backend(nil)) == 0);
    assert(aps5_cairo_pattern_status(Backend(nil)) == CAIRO_STATUS_NULL_POINTER);
}

std::uint32_t Pixel(cairo_surface_t* surface, int x, int y) {
    const unsigned char* data = aps5_cairo_image_surface_get_data(surface);
    std::uint32_t pixel = 0;
    std::memcpy(&pixel, data + y * aps5_cairo_image_surface_get_stride(surface) + x * 4, sizeof(pixel));
    return pixel;
}

struct Setup {
    bool radial;
    int extend;
    int filter;
    bool transformed;
};

cairo_surface_t* PaintGuest(const Setup& setup) {
    CairoPattern* pattern = setup.radial ? cairo_pattern_create_radial(32, 32, 4, 32, 32, 20)
                                         : cairo_pattern_create_linear(16, 0, 48, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0, 1, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0, 1, 0, 0.5);
    cairo_pattern_add_color_stop_rgba(pattern, 1, 0, 0, 1, 1);
    cairo_pattern_set_extend(pattern, setup.extend);
    cairo_pattern_set_filter(pattern, setup.filter);
    if (setup.transformed) {
        const CairoMatrix matrix{0.5, 0.25, -0.25, 0.75, 8, -4};
        cairo_pattern_set_matrix(pattern, &matrix);
    }
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    CairoContext* context = cairo_create(surface);
    cairo_set_source(context, pattern);
    cairo_paint(context);
    assert(aps5_cairo_status(reinterpret_cast<cairo_t*>(context)) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(context);
    cairo_pattern_destroy(pattern);
    cairo_surface_flush(surface);
    return reinterpret_cast<cairo_surface_t*>(surface);
}

cairo_surface_t* PaintReference(const Setup& setup) {
    cairo_pattern_t* pattern = setup.radial ? aps5_cairo_pattern_create_radial(32, 32, 4, 32, 32, 20)
                                            : aps5_cairo_pattern_create_linear(16, 0, 48, 0);
    aps5_cairo_pattern_add_color_stop_rgba(pattern, 0, 1, 0, 0, 1);
    aps5_cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0, 1, 0, 0.5);
    aps5_cairo_pattern_add_color_stop_rgba(pattern, 1, 0, 0, 1, 1);
    aps5_cairo_pattern_set_extend(pattern, static_cast<cairo_extend_t>(setup.extend));
    aps5_cairo_pattern_set_filter(pattern, static_cast<cairo_filter_t>(setup.filter));
    if (setup.transformed) {
        cairo_matrix_t matrix{};
        aps5_cairo_matrix_init(&matrix, 0.5, 0.25, -0.25, 0.75, 8, -4);
        aps5_cairo_pattern_set_matrix(pattern, &matrix);
    }
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    cairo_t* context = aps5_cairo_create(surface);
    aps5_cairo_set_source(context, pattern);
    aps5_cairo_paint(context);
    assert(aps5_cairo_status(context) == CAIRO_STATUS_SUCCESS);
    aps5_cairo_destroy(context);
    aps5_cairo_pattern_destroy(pattern);
    aps5_cairo_surface_flush(surface);
    return surface;
}

bool Same(cairo_surface_t* a, cairo_surface_t* b) {
    const int stride = aps5_cairo_image_surface_get_stride(a);
    assert(stride == aps5_cairo_image_surface_get_stride(b));
    return std::memcmp(aps5_cairo_image_surface_get_data(a), aps5_cairo_image_surface_get_data(b),
        static_cast<std::size_t>(stride) * Size) == 0;
}

void SurfaceFilter() {
    std::array<std::uint32_t, 4> texels{0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu};
    CairoSurface* checker = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(texels.data()),
        CAIRO_FORMAT_ARGB32, 2, 2, 8);
    std::array<cairo_surface_t*, 2> outputs{};
    for (int i = 0; i < 2; ++i) {
        CairoPattern* pattern = cairo_pattern_create_for_surface(checker);
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(pattern, i == 0 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_BILINEAR);
        const CairoMatrix scale{0.125, 0, 0, 0.125, 0, 0};
        cairo_pattern_set_matrix(pattern, &scale);
        CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
        CairoContext* context = cairo_create(target);
        cairo_set_source(context, pattern);
        cairo_paint(context);
        cairo_destroy(context);
        cairo_pattern_destroy(pattern);
        cairo_surface_flush(target);
        outputs[i] = reinterpret_cast<cairo_surface_t*>(target);
    }
    assert(Pixel(outputs[0], 3, 3) == 0xffff0000u);
    assert(Pixel(outputs[0], 12, 3) == 0xff00ff00u);
    assert(Pixel(outputs[0], 3, 12) == 0xff0000ffu);
    assert(Pixel(outputs[0], 19, 3) == 0xffff0000u);
    assert(Pixel(outputs[0], 7, 3) == 0xffff0000u);
    assert(Pixel(outputs[1], 7, 3) != 0xffff0000u && Pixel(outputs[1], 7, 3) != 0xff00ff00u);
    assert(!Same(outputs[0], outputs[1]));
    for (auto* output : outputs) aps5_cairo_surface_destroy(output);
    cairo_surface_destroy(checker);
}

void Raster() {
    for (int radial = 0; radial < 2; ++radial) {
        for (int extend : {CAIRO_EXTEND_NONE, CAIRO_EXTEND_REPEAT, CAIRO_EXTEND_REFLECT, CAIRO_EXTEND_PAD}) {
            for (int transformed = 0; transformed < 2; ++transformed) {
                const Setup setup{radial == 1, extend, CAIRO_FILTER_GOOD, transformed == 1};
                cairo_surface_t* actual = PaintGuest(setup);
                cairo_surface_t* expected = PaintReference(setup);
                assert(Same(actual, expected));
                aps5_cairo_surface_destroy(expected);
                aps5_cairo_surface_destroy(actual);
            }
        }
    }

    cairo_surface_t* pad = PaintGuest({false, CAIRO_EXTEND_PAD, CAIRO_FILTER_GOOD, false});
    assert(Pixel(pad, 2, 5) == 0xffff0000u);
    assert(Pixel(pad, 60, 5) == 0xff0000ffu);
    cairo_surface_t* none = PaintGuest({false, CAIRO_EXTEND_NONE, CAIRO_FILTER_GOOD, false});
    assert(Pixel(none, 2, 5) == 0);
    assert(Pixel(none, 60, 5) == 0);
    assert(Pixel(none, 32, 5) == Pixel(pad, 32, 5) && Pixel(pad, 32, 5) != 0);
    cairo_surface_t* repeat = PaintGuest({false, CAIRO_EXTEND_REPEAT, CAIRO_FILTER_GOOD, false});
    assert(Pixel(repeat, 2, 5) == Pixel(repeat, 34, 5));
    cairo_surface_t* reflect = PaintGuest({false, CAIRO_EXTEND_REFLECT, CAIRO_FILTER_GOOD, false});
    assert(Pixel(reflect, 12, 5) == Pixel(reflect, 19, 5));
    assert(Pixel(reflect, 12, 5) != Pixel(repeat, 12, 5));
    cairo_surface_t* moved = PaintGuest({false, CAIRO_EXTEND_PAD, CAIRO_FILTER_GOOD, true});
    assert(!Same(moved, pad));
    for (auto* surface : {pad, none, repeat, reflect, moved}) aps5_cairo_surface_destroy(surface);

    SurfaceFilter();
}

void Require(bool value) {
    if (!value) {
        std::fputs("guard page setup failed\n", stderr);
        std::abort();
    }
}

template <typename Fn>
void ExpectRejected(Fn&& fn) {
    errno = 71;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw && errno == 71);
}

struct GuardedTail {
    unsigned char* base = nullptr;
    std::size_t page = 0;

    GuardedTail() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        const BOOL guarded = VirtualProtect(base + page, page, PAGE_NOACCESS, &previous);
        Require(guarded != 0);
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        const int guarded = mprotect(base + page, page, PROT_NONE);
        Require(guarded == 0);
#endif
    }

    ~GuardedTail() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, page * 2);
#endif
    }

    const CairoMatrix* EndingShortBy(std::size_t missing) const {
        return reinterpret_cast<const CairoMatrix*>(base + page - sizeof(CairoMatrix) + missing);
    }
};

void Pointers() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 0);
    const CairoMatrix scale{2, 0, 0, 2, 0, 0};
    cairo_pattern_set_matrix(pattern, &scale);
    ExpectRejected([&] { cairo_pattern_set_matrix(pattern, nullptr); });
    ExpectRejected([&] { cairo_pattern_set_matrix(pattern, reinterpret_cast<const CairoMatrix*>(std::uintptr_t{0x10})); });
    GuardedTail guard;
    ExpectRejected([&] { cairo_pattern_set_matrix(pattern, guard.EndingShortBy(sizeof(double))); });
    const CairoMatrix identity{1, 0, 0, 1, 0, 0};
    std::memcpy(guard.base + guard.page - sizeof(CairoMatrix), &identity, sizeof(identity));
    cairo_pattern_set_matrix(pattern, guard.EndingShortBy(0));
    cairo_matrix_t actual{};
    aps5_cairo_pattern_get_matrix(Backend(pattern), &actual);
    Near(actual.xx, 1);
    Near(actual.yy, 1);
    assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
    ExpectRejected([&] { cairo_pattern_set_matrix(pattern, guard.EndingShortBy(sizeof(double))); });
    aps5_cairo_pattern_get_matrix(Backend(pattern), &actual);
    Near(actual.xx, 1);
    cairo_pattern_destroy(pattern);
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _cairo_mutex_initialize();
#endif
    assert(argc == 2);
    const std::string_view mode(argv[1]);
    if (mode == "gradients") Gradients();
    else if (mode == "state") State();
    else if (mode == "lifetime") Lifetime();
    else if (mode == "raster") Raster();
    else if (mode == "pointers") Pointers();
    else return 1;
    aps5_cairo_debug_reset_static_data();
}
