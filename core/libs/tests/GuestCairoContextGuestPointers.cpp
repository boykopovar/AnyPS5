#include "prx/libcairo/include/Path.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(std::is_same_v<decltype(&cairo_transform), void (APS5_VABI*)(CairoContext*, const CairoMatrix*)>);
static_assert(std::is_same_v<decltype(&cairo_set_matrix), void (APS5_VABI*)(CairoContext*, const CairoMatrix*)>);
static_assert(std::is_same_v<decltype(&cairo_get_matrix), void (APS5_VABI*)(CairoContext*, CairoMatrix*)>);
static_assert(std::is_same_v<decltype(&cairo_user_to_device), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_user_to_device_distance), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_device_to_user), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_device_to_user_distance), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_set_dash), void (APS5_VABI*)(CairoContext*, const double*, int, double)>);
static_assert(std::is_same_v<decltype(&cairo_get_dash), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_get_current_point), void (APS5_VABI*)(CairoContext*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_path_extents), void (APS5_VABI*)(CairoContext*, double*, double*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_fill_extents), void (APS5_VABI*)(CairoContext*, double*, double*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_stroke_extents), void (APS5_VABI*)(CairoContext*, double*, double*, double*, double*)>);
static_assert(std::is_same_v<decltype(&cairo_clip_extents), void (APS5_VABI*)(CairoContext*, double*, double*, double*, double*)>);

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Cairo context guest pointer test failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(condition) Require((condition), __LINE__)

bool Close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * (1.0 + std::abs(a) + std::abs(b));
}

struct ReadOnlyPage {
    void* address = nullptr;
    std::size_t size = 0;

    explicit ReadOnlyPage(const void* data, std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        CHECK(address != nullptr);
        std::memcpy(address, data, bytes);
        DWORD previous = 0;
        CHECK(VirtualProtect(address, size, PAGE_READONLY, &previous) != 0);
#else
        const long page = sysconf(_SC_PAGESIZE);
        CHECK(page > 0);
        size = static_cast<std::size_t>(page);
        address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(address != MAP_FAILED);
        std::memcpy(address, data, bytes);
        CHECK(mprotect(address, size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!address) return;
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, size);
#endif
    }
};

template <typename Fn>
void ExpectThrowsPreservingErrno(Fn&& fn, int line) {
    errno = 71;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, line);
}

#define EXPECT_REJECTED(...) ExpectThrowsPreservingErrno([&] { __VA_ARGS__; }, __LINE__)

} // namespace

int main() {
    CairoSurface* surface = cairo_image_surface_create(0, 128, 128);
    CHECK(surface != nullptr);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CHECK(context != nullptr);

    const std::array<double, 6> seeded{2.0, 0.0, 0.0, 3.0, 10.0, -5.0};
    const ReadOnlyPage roPage(seeded.data(), sizeof(seeded));
    auto* roMatrix = static_cast<CairoMatrix*>(roPage.address);
    auto* roDoubles = static_cast<double*>(roPage.address);

    const auto* badMatrixIn = reinterpret_cast<const CairoMatrix*>(std::uintptr_t{0x10});
    auto* badMatrixOut = reinterpret_cast<CairoMatrix*>(std::uintptr_t{0x10});
    auto* badDouble = reinterpret_cast<double*>(std::uintptr_t{0x10});

    EXPECT_REJECTED(cairo_set_matrix(context, nullptr));
    EXPECT_REJECTED(cairo_set_matrix(context, badMatrixIn));
    EXPECT_REJECTED(cairo_transform(context, nullptr));
    EXPECT_REJECTED(cairo_transform(context, badMatrixIn));
    EXPECT_REJECTED(cairo_get_matrix(context, nullptr));
    EXPECT_REJECTED(cairo_get_matrix(context, badMatrixOut));
    EXPECT_REJECTED(cairo_get_matrix(context, roMatrix));

    const CairoMatrix scale{2.0, 0.0, 0.0, 3.0, 4.0, -5.0};
    cairo_set_matrix(context, &scale);
    CairoMatrix translate{1.0, 0.0, 0.0, 1.0, 1.0, 2.0};
    cairo_transform(context, &translate);
    CairoMatrix actual{};
    cairo_get_matrix(context, &actual);
    CHECK(Close(actual.xx, 2.0) && Close(actual.yy, 3.0) && Close(actual.x0, 6.0) && Close(actual.y0, 1.0));

    double x = 3.0;
    double y = 4.0;
    cairo_user_to_device(context, &x, &y);
    CHECK(Close(x, 12.0) && Close(y, 13.0));
    cairo_device_to_user(context, &x, &y);
    CHECK(Close(x, 3.0) && Close(y, 4.0));

    double dx = 5.0;
    double dy = -2.0;
    cairo_user_to_device_distance(context, &dx, &dy);
    CHECK(Close(dx, 10.0) && Close(dy, -6.0));
    cairo_device_to_user_distance(context, &dx, &dy);
    CHECK(Close(dx, 5.0) && Close(dy, -2.0));

    EXPECT_REJECTED(cairo_set_dash(context, nullptr, 2, 0.5));
    EXPECT_REJECTED(cairo_set_dash(context, badDouble, 2, 0.5));
    const std::array<double, 2> dashPattern{4.0, 2.0};
    cairo_set_dash(context, dashPattern.data(), 2, 1.5);
    CHECK(cairo_get_dash_count(context) == 2);

    std::array<double, 2> outDashes{99.0, 99.0};
    double outOffset = 99.0;
    EXPECT_REJECTED(cairo_get_dash(context, badDouble, &outOffset));
    EXPECT_REJECTED(cairo_get_dash(context, roDoubles, &outOffset));
    CHECK(Close(outOffset, 99.0));
    EXPECT_REJECTED(cairo_get_dash(context, outDashes.data(), roDoubles));
    CHECK(Close(outDashes[0], 99.0) && Close(outDashes[1], 99.0));
    cairo_get_dash(context, outDashes.data(), &outOffset);
    CHECK(Close(outDashes[0], 4.0) && Close(outDashes[1], 2.0) && Close(outOffset, 1.5));

    cairo_identity_matrix(context);
    cairo_new_path(context);
    cairo_move_to(context, 10.0, 20.0);
    cairo_line_to(context, 30.0, 50.0);

    double px = 77.0;
    double py = 88.0;
    EXPECT_REJECTED(cairo_get_current_point(context, &px, roDoubles));
    EXPECT_REJECTED(cairo_get_current_point(context, badDouble, &py));
    CHECK(Close(px, 77.0) && Close(py, 88.0));
    cairo_get_current_point(context, &px, nullptr);
    CHECK(Close(px, 30.0));
    cairo_get_current_point(context, nullptr, &py);
    CHECK(Close(py, 50.0));

    double x1 = 91.0;
    double y1 = 92.0;
    double x2 = 93.0;
    double y2 = 94.0;
    EXPECT_REJECTED(cairo_path_extents(context, &x1, &y1, &x2, roDoubles));
    EXPECT_REJECTED(cairo_path_extents(nullptr, &x1, nullptr, nullptr, roDoubles));
    EXPECT_REJECTED(cairo_fill_extents(context, &x1, &y1, &x2, badDouble));
    EXPECT_REJECTED(cairo_fill_extents(nullptr, &x1, nullptr, nullptr, roDoubles));
    EXPECT_REJECTED(cairo_stroke_extents(context, &x1, &y1, &x2, roDoubles));
    EXPECT_REJECTED(cairo_stroke_extents(nullptr, &x1, nullptr, nullptr, badDouble));
    EXPECT_REJECTED(cairo_clip_extents(context, &x1, &y1, &x2, roDoubles));
    EXPECT_REJECTED(cairo_clip_extents(nullptr, &x1, nullptr, nullptr, roDoubles));
    CHECK(Close(x1, 91.0) && Close(y1, 92.0) && Close(x2, 93.0) && Close(y2, 94.0));

    cairo_path_extents(context, &x1, &y1, &x2, &y2);
    CHECK(Close(x1, 10.0) && Close(y1, 20.0) && Close(x2, 30.0) && Close(y2, 50.0));
    cairo_stroke_extents(context, &x1, &y1, &x2, &y2);
    CHECK(x1 < 10.0 && y1 < 20.0 && x2 > 30.0 && y2 > 50.0);
    cairo_clip_extents(context, &x1, &y1, &x2, &y2);
    CHECK(Close(x1, 0.0) && Close(y1, 0.0) && Close(x2, 128.0) && Close(y2, 128.0));

    cairo_destroy(context);
    std::puts("guest Cairo context guest pointer tests passed");
}
