#include "prx/libSceFreeTypeOt/include/OutlineTransform.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
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

template <typename TCall> bool Invalid(TCall call) {
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument&) {
        return errno == 71;
    }
    return false;
}

template <typename TCall> bool Overflows(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("signed 64-bit overflow") != std::string::npos;
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

    Shape small({{1, 2}}, {0});
    small.outline.points = nullptr;
    Require(Throws([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&small.outline, &shear); }), "null points with a positive count throw");

    Shape guarded({{1, 2}}, {0});
    const auto* unmappedOutline = reinterpret_cast<const GuestOutline*>(std::uintptr_t{0x10});
    const auto* unmappedMatrix = reinterpret_cast<const GuestMatrix*>(std::uintptr_t{0x10});
    Require(Invalid([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(unmappedOutline, &shear); }), "an unmapped outline is invalid");
    Require(Invalid([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&guarded.outline, unmappedMatrix); }), "an unmapped matrix is invalid");
    Shape empty({{7, 8}}, {0});
    empty.outline.n_points = 0;
    sceFreeTypeOtOutlineTransform_nid_no_patch(&empty.outline, unmappedMatrix);
    Require(empty.Is({{7, 8}}), "an outline without points never reads the matrix");
    guarded.outline.points = reinterpret_cast<GuestVector*>(std::uintptr_t{0x10});
    Require(Invalid([&] { sceFreeTypeOtOutlineTransform_nid_no_patch(&guarded.outline, &shear); }), "unmapped points are invalid");
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

    Shape unmapped({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {3});
    errno = 71;
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(reinterpret_cast<GuestOutline*>(std::uintptr_t{0x10}), 128) == 0x06 && errno == 71,
            "an unmapped outline is Invalid_Argument");
    unmapped.outline.points = reinterpret_cast<GuestVector*>(std::uintptr_t{0x10});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&unmapped.outline, 128) == 0x06 && errno == 71, "unmapped points are Invalid_Argument");
    unmapped.outline.points = unmapped.points.data();
    unmapped.outline.contours = reinterpret_cast<std::uint16_t*>(std::uintptr_t{0x10});
    Require(sceFreeTypeOtOutlineEmbolden_nid_no_patch(&unmapped.outline, 128) == 0x06 && errno == 71 && unmapped.Is({{0, 0}, {0, 640}, {640, 640}, {640, 0}}),
            "unmapped contours are Invalid_Argument and leave the points unchanged");

    Shape outside({{0, 0}, {0, 640}, {640, 640}, {640, 0}}, {4});
    Require(Throws([&] { sceFreeTypeOtOutlineEmbolden_nid_no_patch(&outside.outline, 128); }) && outside.Is({{0, 0}, {0, 640}, {640, 640}, {640, 0}}),
            "a contour end past the points throws unchanged");
    Shape huge({{0, 0}, {0, 0x40000000}, {0x40000000, 0x40000000}, {0x40000000, 0}}, {3});
    Require(Overflows([&] { sceFreeTypeOtOutlineEmbolden_nid_no_patch(&huge.outline, 128); }) && huge.Is({{0, 0}, {0, 0x40000000}, {0x40000000, 0x40000000}, {0x40000000, 0}}),
            "an overflowing product throws its own error unchanged");
}

}

int main() {
    Transform();
    Embolden();
    std::puts("Guest FreeType Ot outline transform tests passed");
    return 0;
}
