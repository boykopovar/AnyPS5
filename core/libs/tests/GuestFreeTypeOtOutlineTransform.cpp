#include "prx/libSceFreeTypeOt/include/OutlineTransform.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {

using FreeType::GuestMatrix;
using FreeType::GuestOutline;
using FreeType::GuestVector;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot outline transform: %s\n", message);
    std::exit(1);
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct Shape {
    std::vector<GuestVector> points;
    std::vector<std::uint16_t> contours;
    GuestOutline outline{};

    Shape(std::initializer_list<GuestVector> initialPoints, std::initializer_list<std::uint16_t> initialContours)
        : points(initialPoints), contours(initialContours) {
        outline.n_points = static_cast<std::uint16_t>(points.size());
        outline.n_contours = static_cast<std::uint16_t>(contours.size());
        outline.points = points.data();
        outline.contours = contours.data();
    }

    bool Is(std::initializer_list<GuestVector> expected) const {
        if (expected.size() != points.size()) return false;
        std::size_t i = 0;
        for (const GuestVector& point : expected) {
            if (points[i].x != point.x || points[i].y != point.y) return false;
            ++i;
        }
        return true;
    }
};

void Transform() {
    Shape shape({{64, 0}, {0, 128}, {-32, -17}, {1000, -3}}, {3});
    const GuestMatrix shear{0x10000, 0x8000, -0x4000, 0x20000};
    sceFreeTypeOtOutlineTransform_nid_no_patch(&shape.outline, &shear);
    Require(shape.Is({{64, -16}, {64, 256}, {-41, -26}, {998, -256}}), "each point is multiplied with FT_MulFix rounding");

    Shape untouched({{5, 6}}, {0});
    sceFreeTypeOtOutlineTransform_nid_no_patch(&untouched.outline, nullptr);
    sceFreeTypeOtOutlineTransform_nid_no_patch(nullptr, &shear);
    untouched.outline.n_points = 0x8000;
    sceFreeTypeOtOutlineTransform_nid_no_patch(&untouched.outline, &shear);
    Require(untouched.Is({{5, 6}}), "a null argument or a negative short point count is a no-op");

    Shape wide({{1, 2}, {0x80000000LL, 0}}, {1});
    Require(Throws([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&wide.outline, &shear); }) && wide.Is({{1, 2}, {0x80000000LL, 0}}),
            "a coordinate above 0x7FFFFFFF throws before any point is written");
    const GuestMatrix large{0x80000000LL, 0, 0, 0x10000};
    Shape small({{1, 2}}, {0});
    Require(Throws([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&small.outline, &large); }) && small.Is({{1, 2}}), "a matrix entry above 0x7FFFFFFF throws");
    small.outline.points = nullptr;
    Require(Throws([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&small.outline, &shear); }), "null points with a positive count throw");
}

}

int main() {
    Transform();
    std::puts("Guest FreeType Ot outline transform tests passed");
    return 0;
}
