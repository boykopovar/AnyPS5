#include "prx/libcairo/include/InventoryExports.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

static_assert(sizeof(CairoRectangleInt) == 16);
static_assert(std::is_standard_layout_v<CairoRectangleInt>);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Cairo region check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static bool Equal(const CairoRectangleInt& a, const CairoRectangleInt& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

static void Verify(CairoRegion* region, const std::vector<CairoRectangleInt>& expected) {
    Require(cairo_region_num_rectangles(region) == static_cast<int>(expected.size()));
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CairoRectangleInt rectangle{-1, -1, -1, -1};
        cairo_region_get_rectangle(region, static_cast<int>(i), &rectangle);
        Require(Equal(rectangle, expected[i]));
    }
}

static void Union(CairoRegion* region, const CairoRectangleInt& rectangle) {
    Require(cairo_region_union_rectangle(region, &rectangle) == 0);
}

static void TestLifecycle() {
    CairoRegion* region = cairo_region_create();
    Require(region != nullptr);
    Verify(region, {});
    Require(cairo_region_reference(nullptr) == nullptr);
    cairo_region_destroy(nullptr);
    Require(cairo_region_reference(region) == region);
    cairo_region_destroy(region);
    Union(region, {5, 7, 4, 6});
    Verify(region, {{5, 7, 4, 6}});
    CairoRectangleInt invalid{99, 99, 99, 99};
    cairo_region_get_rectangle(region, -1, &invalid);
    Require(Equal(invalid, {0, 0, 0, 0}));
    cairo_region_get_rectangle(region, 1, &invalid);
    Require(Equal(invalid, {0, 0, 0, 0}));
    cairo_region_get_rectangle(region, 0, nullptr);
    Require(cairo_region_num_rectangles(nullptr) == 0);
    Require(cairo_region_union_rectangle(nullptr, &invalid) == 7);
    Require(cairo_region_union_rectangle(region, nullptr) == 7);
    cairo_region_destroy(region);
}

static void TestGeometry() {
    CairoRegion* region = cairo_region_create();
    Union(region, {0, 0, 10, 10});
    Union(region, {5, 5, 10, 10});
    Verify(region, {{0, 0, 10, 5}, {0, 5, 15, 5}, {5, 10, 10, 5}});
    Union(region, {0, 0, 10, 10});
    Union(region, {5, 5, 0, 5});
    Union(region, {5, 5, -2, 4});
    Verify(region, {{0, 0, 10, 5}, {0, 5, 15, 5}, {5, 10, 10, 5}});
    cairo_region_destroy(region);

    region = cairo_region_create();
    Union(region, {0, 0, 4, 4});
    Union(region, {4, 0, 3, 4});
    Union(region, {0, 4, 7, 2});
    Verify(region, {{0, 0, 7, 6}});
    Union(region, {7, 6, 3, 3});
    Verify(region, {{0, 0, 7, 6}, {7, 6, 3, 3}});
    cairo_region_destroy(region);

    region = cairo_region_create();
    Union(region, {0, 0, 10, 2});
    Union(region, {0, 8, 10, 2});
    Union(region, {0, 2, 2, 6});
    Union(region, {8, 2, 2, 6});
    Verify(region, {{0, 0, 10, 2}, {0, 2, 2, 6}, {8, 2, 2, 6}, {0, 8, 10, 2}});
    cairo_region_destroy(region);

    region = cairo_region_create();
    Union(region, {10, 0, 2, 2});
    Union(region, {-12, -10, 5, 5});
    Union(region, {-4, 0, 2, 2});
    Verify(region, {{-12, -10, 5, 5}, {-4, 0, 2, 2}, {10, 0, 2, 2}});
    cairo_region_destroy(region);
}

static void TestBounds() {
    CairoRegion* region = cairo_region_create();
    const int maximum = std::numeric_limits<std::int32_t>::max();
    const int minimum = std::numeric_limits<std::int32_t>::min();
    Union(region, {maximum - 2, maximum - 2, 2, 2});
    Union(region, {minimum, minimum, 2, 2});
    Verify(region, {{minimum, minimum, 2, 2}, {maximum - 2, maximum - 2, 2, 2}});
    const CairoRectangleInt overflow{maximum, 0, 2, 2};
    Require(cairo_region_union_rectangle(region, &overflow) == 32);
    Require(cairo_region_num_rectangles(region) == 0);
    CairoRectangleInt rectangle{1, 1, 1, 1};
    cairo_region_get_rectangle(region, 0, &rectangle);
    Require(Equal(rectangle, {0, 0, 0, 0}));
    Require(cairo_region_union_rectangle(region, &rectangle) == 32);
    cairo_region_destroy(region);

    region = cairo_region_create();
    Union(region, {minimum, 0, maximum, 1});
    const CairoRectangleInt tooWide{-1, 0, 2, 1};
    Require(cairo_region_union_rectangle(region, &tooWide) == 32);
    Require(cairo_region_num_rectangles(region) == 0);
    cairo_region_destroy(region);
}

static void TestRandomUnions() {
    constexpr int grid = 24;
    std::array<std::array<bool, grid>, grid> covered{};
    CairoRegion* region = cairo_region_create();
    std::uint32_t state = 192837465u;
    for (int iteration = 0; iteration < 250; ++iteration) {
        auto draw = [&state](int bound) {
            state = state * 1664525u + 1013904223u;
            return static_cast<int>(state % static_cast<std::uint32_t>(bound));
        };
        CairoRectangleInt added{draw(grid) - 12, draw(grid) - 12, draw(9), draw(9)};
        Union(region, added);
        for (int y = 0; y < grid; ++y)
            for (int x = 0; x < grid; ++x)
                if (x - 12 >= added.x && x - 12 < added.x + added.width &&
                    y - 12 >= added.y && y - 12 < added.y + added.height)
                    covered[y][x] = true;

        std::vector<CairoRectangleInt> rectangles;
        const int count = cairo_region_num_rectangles(region);
        for (int i = 0; i < count; ++i) {
            CairoRectangleInt box{};
            cairo_region_get_rectangle(region, i, &box);
            Require(box.width > 0 && box.height > 0);
            if (i > 0) {
                const auto& previous = rectangles.back();
                Require(previous.y <= box.y);
                if (previous.y == box.y) {
                    Require(previous.height == box.height);
                    Require(previous.x + previous.width < box.x);
                } else {
                    Require(previous.y + previous.height <= box.y);
                }
            }
            rectangles.push_back(box);
        }
        for (int y = 0; y < grid; ++y) {
            for (int x = 0; x < grid; ++x) {
                int hits = 0;
                for (const auto& box : rectangles)
                    hits += x - 12 >= box.x && x - 12 < box.x + box.width &&
                            y - 12 >= box.y && y - 12 < box.y + box.height;
                Require(hits <= 1);
                Require((hits == 1) == covered[y][x]);
            }
        }
    }
    cairo_region_destroy(region);
}

int main() {
    TestLifecycle();
    TestGeometry();
    TestBounds();
    TestRandomUnions();
}
