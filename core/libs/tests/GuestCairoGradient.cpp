#include "prx/libcairo/include/Pattern.hpp"
#include "prx/libcairo/include/SurfacePattern.hpp"

#include <CairoBackend.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(&cairo_pattern_create_linear), CairoPattern* (APS5_VABI*)(double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_create_radial),
                             CairoPattern* (APS5_VABI*)(double, double, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_set_matrix), void (APS5_VABI*)(CairoPattern*, const CairoMatrix*)>);

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cairo gradient check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

cairo_pattern_t* Native(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

struct Stop {
    double offset, red, green, blue, alpha;

    bool operator==(const Stop& other) const {
        return offset == other.offset && red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
    }
};

struct State {
    int status;
    int type;
    std::vector<double> geometry;
    std::vector<Stop> stops;
    int extend;
    int filter;
    cairo_matrix_t matrix;

    bool operator==(const State& other) const {
        return status == other.status && type == other.type && geometry == other.geometry && stops == other.stops
            && extend == other.extend && filter == other.filter
            && std::memcmp(&matrix, &other.matrix, sizeof(matrix)) == 0;
    }
};

State Read(cairo_pattern_t* pattern) {
    State state{};
    state.status = aps5_cairo_pattern_status(pattern);
    state.type = aps5_cairo_pattern_get_type(pattern);
    double values[6] = {};
    if (aps5_cairo_pattern_get_linear_points(pattern, &values[0], &values[1], &values[2], &values[3]) == CAIRO_STATUS_SUCCESS)
        state.geometry.assign(values, values + 4);
    if (aps5_cairo_pattern_get_radial_circles(pattern, &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == CAIRO_STATUS_SUCCESS)
        state.geometry.assign(values, values + 6);
    int count = 0;
    if (aps5_cairo_pattern_get_color_stop_count(pattern, &count) == CAIRO_STATUS_SUCCESS) {
        for (int index = 0; index < count; ++index) {
            Stop stop{};
            Require(aps5_cairo_pattern_get_color_stop_rgba(pattern, index, &stop.offset, &stop.red, &stop.green, &stop.blue, &stop.alpha) == CAIRO_STATUS_SUCCESS);
            state.stops.push_back(stop);
        }
    }
    state.extend = aps5_cairo_pattern_get_extend(pattern);
    state.filter = aps5_cairo_pattern_get_filter(pattern);
    aps5_cairo_pattern_get_matrix(pattern, &state.matrix);
    return state;
}

struct Geometry {
    bool radial;
    double values[6];
};

const Geometry Geometries[] = {
    {false, {0, 0, 16, 0}},
    {false, {-3.5, 2.25, 9, -11}},
    {false, {4, 4, 4, 4}},
    {false, {1e6, -1e6, -1e6, 1e6}},
    {true, {8, 8, 0, 8, 8, 8}},
    {true, {2, 3, 1, 12, 9, 5.5}},
    {true, {0, 0, 4, 0, 0, 4}},
    {true, {0, 0, -1, 0, 0, 2}},
};

struct StopInput {
    double offset, red, green, blue, alpha;
};

const StopInput Stops[] = {
    {0.5, 1, 0, 0, 1},
    {0, 0, 0, 1, 0.5},
    {1, 0, 1, 0, 1},
    {0.5, 0.25, 0.25, 0.25, 0.25},
    {-0.75, 2, -1, 0.5, 1.5},
    {1.5, 0.1, 0.2, 0.3, 0.4},
    {0.25, 0.125, 0.375, 0.625, 0.875},
};

CairoPattern* CreateGuest(const Geometry& geometry) {
    const double* v = geometry.values;
    return geometry.radial ? cairo_pattern_create_radial(v[0], v[1], v[2], v[3], v[4], v[5])
                           : cairo_pattern_create_linear(v[0], v[1], v[2], v[3]);
}

cairo_pattern_t* CreateNative(const Geometry& geometry) {
    const double* v = geometry.values;
    return geometry.radial ? aps5_cairo_pattern_create_radial(v[0], v[1], v[2], v[3], v[4], v[5])
                           : aps5_cairo_pattern_create_linear(v[0], v[1], v[2], v[3]);
}

void CheckMatchesNative() {
    const CairoMatrix matrices[] = {
        {1, 0, 0, 1, 0, 0},
        {2, 0.5, -0.25, 3, 7, -9},
        {1, 2, 2, 4, 0, 0},
        {std::numeric_limits<double>::infinity(), 0, 0, 1, 0, 0},
    };
    for (const Geometry& geometry : Geometries) {
        for (const CairoMatrix& matrix : matrices) {
            for (int extend : {0, 1, 2, 3, 77}) {
                for (int filter : {0, 1, 2, 3, 4, 5}) {
                    CairoPattern* guest = CreateGuest(geometry);
                    cairo_pattern_t* native = CreateNative(geometry);
                    Require(guest != nullptr && native != nullptr);
                    Require(Read(Native(guest)) == Read(native));
                    for (const StopInput& stop : Stops) {
                        cairo_pattern_add_color_stop_rgba(guest, stop.offset, stop.red, stop.green, stop.blue, stop.alpha);
                        aps5_cairo_pattern_add_color_stop_rgba(native, stop.offset, stop.red, stop.green, stop.blue, stop.alpha);
                        Require(Read(Native(guest)) == Read(native));
                    }
                    cairo_pattern_set_extend(guest, extend);
                    aps5_cairo_pattern_set_extend(native, static_cast<cairo_extend_t>(extend));
                    cairo_pattern_set_filter(guest, filter);
                    aps5_cairo_pattern_set_filter(native, static_cast<cairo_filter_t>(filter));
                    cairo_pattern_set_matrix(guest, &matrix);
                    cairo_matrix_t nativeMatrix;
                    std::memcpy(&nativeMatrix, &matrix, sizeof(nativeMatrix));
                    aps5_cairo_pattern_set_matrix(native, &nativeMatrix);
                    State guestState = Read(Native(guest));
                    Require(guestState == Read(native));
                    Require(guestState.status != CAIRO_STATUS_SUCCESS || guestState.stops.size() == 7);
                    Require(aps5_cairo_pattern_get_reference_count(Native(guest)) == 1);
                    cairo_pattern_destroy(guest);
                    aps5_cairo_pattern_destroy(native);
                }
            }
        }
    }
}

void CheckLinearValues() {
    CairoPattern* pattern = cairo_pattern_create_linear(1, 2, 3, 4);
    State state = Read(Native(pattern));
    Require(state.status == CAIRO_STATUS_SUCCESS && state.type == CAIRO_PATTERN_TYPE_LINEAR);
    Require(state.geometry == (std::vector<double>{1, 2, 3, 4}));
    Require(state.stops.empty() && state.extend == CAIRO_EXTEND_PAD && state.filter == CAIRO_FILTER_GOOD);

    cairo_pattern_add_color_stop_rgba(pattern, 0.75, 1, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 0.25, 0, 0, 1, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 0.75, 0, 1, 0, 0.5);
    cairo_pattern_add_color_stop_rgba(pattern, -1, 2, -2, 0.5, 9);
    state = Read(Native(pattern));
    Require(state.stops.size() == 4);
    Require(state.stops[0] == (Stop{0, 1, 0, 0.5, 1}));
    Require(state.stops[1] == (Stop{0.25, 0, 0, 1, 1}));
    Require(state.stops[2] == (Stop{0.75, 1, 0, 0, 1}));
    Require(state.stops[3] == (Stop{0.75, 0, 1, 0, 0.5}));

    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REFLECT);
    cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
    const CairoMatrix scale{2, 0, 0, 0.5, 3, -4};
    cairo_pattern_set_matrix(pattern, &scale);
    state = Read(Native(pattern));
    Require(state.status == CAIRO_STATUS_SUCCESS);
    Require(state.extend == CAIRO_EXTEND_REFLECT && state.filter == CAIRO_FILTER_NEAREST);
    Require(state.matrix.xx == 2 && state.matrix.yy == 0.5 && state.matrix.x0 == 3 && state.matrix.y0 == -4);

    const CairoMatrix singular{1, 2, 2, 4, 0, 0};
    cairo_pattern_set_matrix(pattern, &singular);
    Require(aps5_cairo_pattern_status(Native(pattern)) == CAIRO_STATUS_INVALID_MATRIX);
    cairo_pattern_destroy(pattern);
}

void CheckRadialValues() {
    CairoPattern* pattern = cairo_pattern_create_radial(1, 2, 3, 4, 5, 6);
    State state = Read(Native(pattern));
    Require(state.status == CAIRO_STATUS_SUCCESS && state.type == CAIRO_PATTERN_TYPE_RADIAL);
    Require(state.geometry == (std::vector<double>{1, 2, 3, 4, 5, 6}));
    Require(state.extend == CAIRO_EXTEND_PAD);
    cairo_pattern_destroy(pattern);
}

void CheckStopsOnOtherPatterns() {
    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_pattern_add_color_stop_rgba(mesh, 0.5, 1, 1, 1, 1);
    Require(aps5_cairo_pattern_status(Native(mesh)) == CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
    cairo_pattern_destroy(mesh);

    CairoPattern* errored = cairo_pattern_create_for_surface(nullptr);
    Require(aps5_cairo_pattern_status(Native(errored)) == CAIRO_STATUS_NULL_POINTER);
    cairo_pattern_add_color_stop_rgba(errored, 0.5, 1, 1, 1, 1);
    cairo_pattern_set_extend(errored, CAIRO_EXTEND_REPEAT);
    cairo_pattern_set_filter(errored, CAIRO_FILTER_FAST);
    const CairoMatrix identity{1, 0, 0, 1, 0, 0};
    cairo_pattern_set_matrix(errored, &identity);
    Require(aps5_cairo_pattern_status(Native(errored)) == CAIRO_STATUS_NULL_POINTER);
    cairo_pattern_destroy(errored);
}

void CheckDestroyReleasesOneReference() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 1);
    Require(aps5_cairo_pattern_reference(Native(pattern)) == Native(pattern));
    Require(aps5_cairo_pattern_get_reference_count(Native(pattern)) == 2);
    cairo_pattern_destroy(pattern);
    Require(aps5_cairo_pattern_get_reference_count(Native(pattern)) == 1);
    Require(aps5_cairo_pattern_status(Native(pattern)) == CAIRO_STATUS_SUCCESS);
    cairo_pattern_destroy(pattern);
    cairo_pattern_destroy(nullptr);
}

void CheckUnreadableMatrixThrows() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 1, 1);
    bool threw = false;
    try {
        cairo_pattern_set_matrix(pattern, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    State state = Read(Native(pattern));
    Require(state.status == CAIRO_STATUS_SUCCESS && state.matrix.xx == 1 && state.matrix.yx == 0);
    cairo_pattern_destroy(pattern);
}

std::vector<unsigned char> Paint(CairoPattern* guestPattern, cairo_pattern_t* nativePattern) {
    constexpr int Size = 24;
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    cairo_t* context = aps5_cairo_create(surface);
    aps5_cairo_set_source(context, guestPattern ? Native(guestPattern) : nativePattern);
    aps5_cairo_paint(context);
    aps5_cairo_surface_flush(surface);
    const unsigned char* data = aps5_cairo_image_surface_get_data(surface);
    std::vector<unsigned char> pixels(data, data + aps5_cairo_image_surface_get_stride(surface) * Size);
    aps5_cairo_destroy(context);
    aps5_cairo_surface_destroy(surface);
    return pixels;
}

std::uint32_t Pixel(const std::vector<unsigned char>& pixels, int x, int y) {
    std::uint32_t value = 0;
    std::memcpy(&value, pixels.data() + y * 24 * 4 + x * 4, sizeof(value));
    return value;
}

void CheckPaintedLinearGradient() {
    CairoPattern* pattern = cairo_pattern_create_linear(0, 0, 24, 0);
    cairo_pattern_add_color_stop_rgba(pattern, 0, 1, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(pattern, 1, 0, 0, 1, 1);
    std::vector<unsigned char> pixels = Paint(pattern, nullptr);
    Require(Pixel(pixels, 0, 5) == Pixel(pixels, 0, 17));
    std::uint32_t left = Pixel(pixels, 0, 0);
    std::uint32_t right = Pixel(pixels, 23, 0);
    Require((left >> 24) == 0xff && (right >> 24) == 0xff);
    Require(((left >> 16) & 0xff) > 0xf0 && (left & 0xff) < 0x10);
    Require(((right >> 16) & 0xff) < 0x10 && (right & 0xff) > 0xf0);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
    const CairoMatrix shift{1, 0, 0, 1, -12, 0};
    cairo_pattern_set_matrix(pattern, &shift);
    pixels = Paint(pattern, nullptr);
    Require(Pixel(pixels, 6, 3) == 0);
    Require((Pixel(pixels, 18, 3) >> 24) == 0xff);
    cairo_pattern_destroy(pattern);
}

void CheckPaintMatchesNative() {
    for (const Geometry& geometry : Geometries) {
        for (int extend : {0, 1, 2, 3}) {
            CairoPattern* guest = CreateGuest(geometry);
            cairo_pattern_t* native = CreateNative(geometry);
            for (const StopInput& stop : Stops) {
                cairo_pattern_add_color_stop_rgba(guest, stop.offset, stop.red, stop.green, stop.blue, stop.alpha);
                aps5_cairo_pattern_add_color_stop_rgba(native, stop.offset, stop.red, stop.green, stop.blue, stop.alpha);
            }
            cairo_pattern_set_extend(guest, extend);
            aps5_cairo_pattern_set_extend(native, static_cast<cairo_extend_t>(extend));
            const CairoMatrix matrix{0.5, 0.25, -0.25, 0.75, 2, 1};
            cairo_matrix_t nativeMatrix;
            std::memcpy(&nativeMatrix, &matrix, sizeof(nativeMatrix));
            cairo_pattern_set_matrix(guest, &matrix);
            aps5_cairo_pattern_set_matrix(native, &nativeMatrix);
            Require(Paint(guest, nullptr) == Paint(nullptr, native));
            cairo_pattern_destroy(guest);
            aps5_cairo_pattern_destroy(native);
        }
    }
}

}

int main() {
    CheckMatchesNative();
    CheckLinearValues();
    CheckRadialValues();
    CheckStopsOnOtherPatterns();
    CheckDestroyReleasesOneReference();
    CheckUnreadableMatrixThrows();
    CheckPaintMatchesNative();
    CheckPaintedLinearGradient();
    std::puts("cairo gradient tests passed");
    return 0;
}
