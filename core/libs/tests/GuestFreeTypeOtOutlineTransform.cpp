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

void Embolden() {
    Shape trueType({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {3});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&trueType.outline, 65) == 0, "a TrueType contour emboldens");
    Require(trueType.Is({{0, 0}, {0, 704}, {704, 704}, {704, 0}}), "the strength is halved and the box grows up and right");

    Shape postScript({{0, 0}, {640, 0}, {640, 640}, {0, 640}}, {3});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&postScript.outline, 64) == 0, "a PostScript contour emboldens");
    Require(postScript.Is({{0, 0}, {704, 0}, {704, 704}, {0, 704}}), "a PostScript contour shifts along the other normal");

    Shape xy({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {3});
    Require(sceFreeTypeOtOutlineEmboldenXY_nid_no_patch(&xy.outline, 128, -32) == 0, "FT_Outline_EmboldenXY takes separate strengths");
    Require(xy.Is({{0, 0}, {0, 608}, {768, 608}, {768, 0}}), "a negative strength thins that axis");

    Shape nested({{0, 0}, {0, 640}, {640, 640}, {640, 0}, {128, 128}, {512, 128}, {512, 512}, {128, 512}}, {3, 7});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&nested.outline, 40) == 0, "nested contours embolden");
    Require(nested.Is({{0, 0}, {0, 680}, {680, 680}, {680, 0}, {168, 168}, {512, 168}, {512, 512}, {168, 512}}), "the inner contour shrinks");

    Shape triangle({{0, 0}, {320, 1280}, {640, 0}}, {2});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&triangle.outline, 96) == 0, "a sharp corner emboldens");
    Require(triangle.Is({{-14, 0}, {368, 1527}, {750, 0}}), "corner shifts follow the 2.4.11 bisector");

    Shape flat({{0, 0}, {100, 0}, {200, 0}}, {2});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&flat.outline, 128) == 0x06 && flat.Is({{0, 0}, {100, 0}, {200, 0}}),
            "a zero-area contour is Invalid_Argument");
    flat.outline.n_contours = 0;
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&flat.outline, 128) == 0, "an outline without contours succeeds");
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(nullptr, 128) == 0x06, "a null outline is Invalid_Argument");

    Shape weak({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {3});
    weak.outline.points = nullptr;
    Require(sceFreeTypeOtOutlineEmboldenXY_nid_no_patch(&weak.outline, 1, -1) == 0, "strengths that halve to zero return before reading the outline");

    Shape outside({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {4});
    Require(Throws([&] { sceFreeTypeOtOutlineEmbolden_nid_no_patch(&outside.outline, 128); }) && outside.Is({{0, 0}, {0, 640}, {640, 640}, {640, 0}}),
            "a contour end past the points throws unchanged");
    Shape huge({{0, 0}, {0, 0x40000000}, {0x40000000, 0x40000000}, {0x40000000, 0}}, {3});
    Require(Throws([&] { sceFreeTypeOtOutlineEmbolden_nid_no_patch(&huge.outline, 128); }) && huge.Is({{0, 0}, {0, 0x40000000}, {0x40000000, 0x40000000}, {0x40000000, 0}}),
            "overflowing products throw unchanged");
}

}

int main() {
    Transform();
    Embolden();
    std::puts("Guest FreeType Ot outline transform tests passed");
    return 0;
}
