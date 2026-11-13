#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_STROKER_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType stroker check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct Shape {
    std::vector<GuestVector> points;
    std::vector<unsigned char> tags;
    std::vector<std::uint16_t> contours;
    std::vector<FT_Vector> nativePoints;

    Shape(std::vector<GuestVector> guestPoints, std::vector<unsigned char> pointTags, std::vector<std::uint16_t> contourEnds)
        : points(std::move(guestPoints)), tags(std::move(pointTags)), contours(std::move(contourEnds)) {
        for (const GuestVector& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    }

    GuestOutline Guest() {
        return {static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(points.size()), points.data(), tags.data(), contours.data(), 0};
    }

    FT_Outline Native() {
        return {static_cast<unsigned short>(contours.size()), static_cast<unsigned short>(points.size()), nativePoints.data(), tags.data(), contours.data(), 0};
    }
};

static Shape SquareOutline() {
    return Shape({{0, 0}, {640, 0}, {640, 640}, {0, 640}}, {1, 1, 1, 1}, {3});
}

static std::vector<Shape> Shapes() {
    std::vector<Shape> shapes;
    shapes.push_back(SquareOutline());
    shapes.push_back(Shape({{0, 320}, {320, 640}, {640, 320}, {320, 0}}, {0, 1, 0, 1}, {3}));
    shapes.push_back(Shape({{-321, 5}, {320, 641}, {639, 320}, {-2, -7}}, {0, 0, 0, 0}, {3}));
    shapes.push_back(Shape({{0, 0}, {0, 640}, {640, 640}, {640, 0}, {640, -640}, {0, -640}}, {1, 2, 2, 1, 2, 2}, {5}));
    shapes.push_back(Shape({{5, 5}, {0, 0}, {640, 0}, {640, 640}, {0, 640}, {100, 100}, {200, 300}, {300, 100}},
                           {1, 0x09, 1, 0x11, 1, 0x05, 0, 1}, {0, 4, 7}));
    return shapes;
}

struct Target {
    std::uint16_t prefix;
    std::vector<GuestVector> guestPoints;
    std::vector<FT_Vector> nativePoints;
    std::vector<unsigned char> guestTags;
    std::vector<unsigned char> nativeTags;
    std::vector<std::uint16_t> guestContours;
    std::vector<std::uint16_t> nativeContours;
    GuestOutline guest{};
    FT_Outline native{};

    Target(std::uint16_t prefixPoints, std::size_t points, std::size_t contours)
        : prefix(prefixPoints), guestPoints(prefixPoints + points), nativePoints(guestPoints.size()), guestTags(guestPoints.size()),
          nativeTags(guestPoints.size()), guestContours(contours + 1), nativeContours(contours + 1) {
        for (std::size_t index = 0; index < guestPoints.size(); ++index) {
            guestPoints[index] = Point(index);
            nativePoints[index] = {static_cast<FT_Pos>(guestPoints[index].x), static_cast<FT_Pos>(guestPoints[index].y)};
            guestTags[index] = nativeTags[index] = Tag(index);
        }
        for (std::size_t index = 0; index < guestContours.size(); ++index) guestContours[index] = nativeContours[index] = Contour(index);
        guest = {1, prefix, guestPoints.data(), guestTags.data(), guestContours.data(), 0};
        native = {1, prefix, nativePoints.data(), nativeTags.data(), nativeContours.data(), 0};
    }

    GuestVector Point(std::size_t index) const {
        if (index >= prefix) return {-77, 99};
        return {static_cast<std::int64_t>(index % 640), -static_cast<std::int64_t>(index % 320) - 2};
    }

    unsigned char Tag(std::size_t index) const {
        return index < prefix ? 1 : 0xa5;
    }

    std::uint16_t Contour(std::size_t index) const {
        return index == 0 ? static_cast<std::uint16_t>(prefix - 1) : 0xbeef;
    }

    bool Matches() const {
        bool same = guest.n_points == native.n_points && guest.n_contours == native.n_contours && guest.flags == native.flags;
        for (std::size_t index = 0; index < guestPoints.size(); ++index) {
            same = same && guestPoints[index].x == nativePoints[index].x && guestPoints[index].y == nativePoints[index].y &&
                guestTags[index] == nativeTags[index];
        }
        for (std::size_t index = 0; index < guestContours.size(); ++index) same = same && guestContours[index] == nativeContours[index];
        return same;
    }

    bool Pristine() const {
        bool same = guest.n_points == prefix && guest.n_contours == 1 && native.n_points == prefix && native.n_contours == 1;
        for (std::size_t index = 0; index < guestPoints.size(); ++index) {
            const GuestVector point = Point(index);
            same = same && guestPoints[index].x == point.x && guestPoints[index].y == point.y && guestTags[index] == Tag(index);
            same = same && nativePoints[index].x == point.x && nativePoints[index].y == point.y && nativeTags[index] == Tag(index);
        }
        for (std::size_t index = 0; index < guestContours.size(); ++index) {
            same = same && guestContours[index] == Contour(index) && nativeContours[index] == Contour(index);
        }
        return same;
    }
};

struct Pair {
    void* guest = nullptr;
    FT_Stroker native = nullptr;

    Pair(GuestLibrary& library, FT_Library nativeLibrary) {
        Require(FT_Stroker_New_nid_postfix(&library, &guest) == 0);
        Require(FT_Stroker_New(nativeLibrary, &native) == 0);
        FT_Stroker_Set(static_cast<FT_Stroker>(guest), 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Stroker_Set(native, 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
    }

    ~Pair() {
        FT_Stroker_Done_nid_postfix(guest);
        FT_Stroker_Done(native);
    }

    void CountsMatch() const {
        std::uint32_t points = 11;
        std::uint32_t contours = 13;
        FT_UInt nativePoints = 11;
        FT_UInt nativeContours = 13;
        Require(FT_Stroker_GetCounts_nid_postfix(guest, &points, &contours) == FT_Stroker_GetCounts(native, &nativePoints, &nativeContours));
        Require(points == nativePoints && contours == nativeContours);
    }

    void BorderCountsMatch() const {
        for (const std::int32_t border : {0, 1}) {
            std::uint32_t points = 11;
            std::uint32_t contours = 13;
            FT_UInt nativePoints = 11;
            FT_UInt nativeContours = 13;
            Require(FT_Stroker_GetBorderCounts_nid_postfix(guest, border, &points, &contours) ==
                    FT_Stroker_GetBorderCounts(native, static_cast<FT_StrokerBorder>(border), &nativePoints, &nativeContours));
            Require(points == nativePoints && contours == nativeContours);
        }
    }

    int Parse(GuestOutline outline, FT_Outline nativeOutline, std::uint8_t opened) const {
        const int error = FT_Stroker_ParseOutline_nid_postfix(guest, &outline, opened);
        Require(error == FT_Stroker_ParseOutline(native, &nativeOutline, opened));
        return error;
    }

    void Square(std::uint8_t open, bool finish = true) const {
        const GuestVector corners[] = {{0, 0}, {640, 0}, {640, 640}, {0, 640}};
        FT_Vector nativeStart{0, 0};
        Require(FT_Stroker_BeginSubPath_nid_postfix(guest, &corners[0], open) == FT_Stroker_BeginSubPath(native, &nativeStart, open));
        for (int index = 1; index < 4; ++index) {
            FT_Vector nativePoint{static_cast<FT_Pos>(corners[index].x), static_cast<FT_Pos>(corners[index].y)};
            Require(FT_Stroker_LineTo_nid_postfix(guest, &corners[index]) == FT_Stroker_LineTo(native, &nativePoint));
        }
        if (!finish) return;
        Require(FT_Stroker_EndSubPath_nid_postfix(guest) == FT_Stroker_EndSubPath(native));
    }
};

static void TestNew(GuestLibrary& library, FT_Library native) {
    void* guest = reinterpret_cast<void*>(1);
    FT_Stroker expected = nullptr;
    Require(FT_Stroker_New_nid_postfix(nullptr, &guest) == FT_Stroker_New(nullptr, &expected));
    Require(guest == nullptr && expected == nullptr);
    Require(FT_Stroker_New_nid_postfix(&library, nullptr) == FT_Stroker_New(native, nullptr));
    Require(FT_Stroker_New_nid_postfix(&library, &guest) == FT_Stroker_New(native, &expected));
    Require(guest != nullptr && expected != nullptr);
    FT_Stroker_Done(static_cast<FT_Stroker>(guest));
    FT_Stroker_Done(expected);
}

static void TestDone(GuestLibrary& library, FT_Library native) {
    for (int round = 0; round < 3; ++round) {
        void* guest = nullptr;
        FT_Stroker expected = nullptr;
        Require(FT_Stroker_New_nid_postfix(&library, &guest) == 0 && FT_Stroker_New(native, &expected) == 0);
        FT_Stroker_Done_nid_postfix(guest);
        FT_Stroker_Done(expected);
    }
    FT_Stroker_Done_nid_postfix(nullptr);
    FT_Stroker_Done(nullptr);
}

static void TestGetCounts(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    pair.CountsMatch();
    std::uint32_t points = 7;
    std::uint32_t contours = 9;
    FT_UInt nativePoints = 7;
    FT_UInt nativeContours = 9;
    Require(FT_Stroker_GetCounts_nid_postfix(nullptr, &points, &contours) == FT_Stroker_GetCounts(nullptr, &nativePoints, &nativeContours));
    Require(points == nativePoints && contours == nativeContours);
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, nullptr, nullptr) == FT_Stroker_GetCounts(pair.native, nullptr, nullptr));
}

static void TestBeginSubPath(GuestLibrary& library, FT_Library native) {
    for (const std::uint8_t open : {0, 1}) {
        Pair pair(library, native);
        const GuestVector start{64, 128};
        FT_Vector nativeStart{64, 128};
        Require(FT_Stroker_BeginSubPath_nid_postfix(pair.guest, &start, open) == FT_Stroker_BeginSubPath(pair.native, &nativeStart, open));
        pair.CountsMatch();
    }
    Pair pair(library, native);
    const GuestVector start{0, 0};
    FT_Vector nativeStart{0, 0};
    Require(FT_Stroker_BeginSubPath_nid_postfix(nullptr, &start, 0) == FT_Stroker_BeginSubPath(nullptr, &nativeStart, 0));
    Require(FT_Stroker_BeginSubPath_nid_postfix(pair.guest, nullptr, 0) == FT_Stroker_BeginSubPath(pair.native, nullptr, 0));
}

static void TestLineTo(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    const GuestVector start{0, 0};
    FT_Vector nativeStart{0, 0};
    Require(FT_Stroker_BeginSubPath_nid_postfix(pair.guest, &start, 1) == FT_Stroker_BeginSubPath(pair.native, &nativeStart, 1));
    for (const GuestVector point : {GuestVector{640, 0}, GuestVector{640, 640}, GuestVector{0, 640}}) {
        FT_Vector nativePoint{static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)};
        Require(FT_Stroker_LineTo_nid_postfix(pair.guest, &point) == FT_Stroker_LineTo(pair.native, &nativePoint));
        pair.CountsMatch();
    }
    const GuestVector point{1, 1};
    FT_Vector nativePoint{1, 1};
    Require(FT_Stroker_LineTo_nid_postfix(nullptr, &point) == FT_Stroker_LineTo(nullptr, &nativePoint));
    Require(FT_Stroker_LineTo_nid_postfix(pair.guest, nullptr) == FT_Stroker_LineTo(pair.native, nullptr));
}

static void TestEndSubPath(GuestLibrary& library, FT_Library native) {
    for (const std::uint8_t open : {0, 1}) {
        Pair pair(library, native);
        pair.Square(open, false);
        Require(FT_Stroker_EndSubPath_nid_postfix(pair.guest) == FT_Stroker_EndSubPath(pair.native));
        pair.CountsMatch();
    }
    Require(FT_Stroker_EndSubPath_nid_postfix(nullptr) == FT_Stroker_EndSubPath(nullptr));
}

static void TestSet(GuestLibrary& library, FT_Library native) {
    for (const std::int64_t radius : {0, 16, 64, 200}) {
        for (const std::int32_t cap : {0, 1, 2}) {
            for (const std::int32_t join : {0, 1, 2, 3}) {
                Pair pair(library, native);
                FT_Stroker_Set_nid_postfix(pair.guest, radius, cap, join, 0x20000);
                FT_Stroker_Set(pair.native, static_cast<FT_Fixed>(radius), static_cast<FT_Stroker_LineCap>(cap), static_cast<FT_Stroker_LineJoin>(join), 0x20000);
                pair.Square(0);
                pair.Square(1);
                pair.CountsMatch();
            }
        }
    }
}

static void TestRewind(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    FT_Stroker_Set_nid_postfix(pair.guest, 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Stroker_Set(pair.native, 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    pair.Square(0);
    pair.CountsMatch();
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points != 0 && contours != 0);
    FT_Stroker_Rewind_nid_postfix(pair.guest);
    FT_Stroker_Rewind(pair.native);
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points == 0 && contours == 0);
    pair.CountsMatch();
    FT_Stroker_Rewind_nid_postfix(nullptr);
}

static void TestGetBorderCounts(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    FT_Stroker_Set_nid_postfix(pair.guest, 64, FT_STROKER_LINECAP_SQUARE, FT_STROKER_LINEJOIN_BEVEL, 0);
    FT_Stroker_Set(pair.native, 64, FT_STROKER_LINECAP_SQUARE, FT_STROKER_LINEJOIN_BEVEL, 0);
    pair.Square(0);
    for (const std::int32_t border : {0, 1, 2, -1}) {
        std::uint32_t points = 5;
        std::uint32_t contours = 6;
        FT_UInt nativePoints = 5;
        FT_UInt nativeContours = 6;
        Require(FT_Stroker_GetBorderCounts_nid_postfix(pair.guest, border, &points, &contours) ==
                FT_Stroker_GetBorderCounts(pair.native, static_cast<FT_StrokerBorder>(border), &nativePoints, &nativeContours));
        Require(points == nativePoints && contours == nativeContours);
    }
    Require(FT_Stroker_GetBorderCounts_nid_postfix(nullptr, 0, nullptr, nullptr) == FT_Stroker_GetBorderCounts(nullptr, FT_STROKER_BORDER_LEFT, nullptr, nullptr));
}

static void TestConicTo(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    FT_Stroker_Set_nid_postfix(pair.guest, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Stroker_Set(pair.native, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    const GuestVector start{0, 0};
    FT_Vector nativeStart{0, 0};
    Require(FT_Stroker_BeginSubPath_nid_postfix(pair.guest, &start, 1) == FT_Stroker_BeginSubPath(pair.native, &nativeStart, 1));
    const GuestVector control{320, 640};
    const GuestVector to{640, 0};
    FT_Vector nativeControl{320, 640};
    FT_Vector nativeTo{640, 0};
    Require(FT_Stroker_ConicTo_nid_postfix(pair.guest, &control, &to) == FT_Stroker_ConicTo(pair.native, &nativeControl, &nativeTo));
    Require(FT_Stroker_EndSubPath_nid_postfix(pair.guest) == FT_Stroker_EndSubPath(pair.native));
    pair.CountsMatch();
    Require(FT_Stroker_ConicTo_nid_postfix(pair.guest, nullptr, &to) == FT_Stroker_ConicTo(pair.native, nullptr, &nativeTo));
    Require(FT_Stroker_ConicTo_nid_postfix(nullptr, &control, &to) == FT_Stroker_ConicTo(nullptr, &nativeControl, &nativeTo));
}

static void TestCubicTo(GuestLibrary& library, FT_Library native) {
    Pair pair(library, native);
    FT_Stroker_Set_nid_postfix(pair.guest, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Stroker_Set(pair.native, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    const GuestVector start{0, 0};
    FT_Vector nativeStart{0, 0};
    Require(FT_Stroker_BeginSubPath_nid_postfix(pair.guest, &start, 1) == FT_Stroker_BeginSubPath(pair.native, &nativeStart, 1));
    const GuestVector first{0, 640};
    const GuestVector second{640, 640};
    const GuestVector to{640, 0};
    FT_Vector nativeFirst{0, 640};
    FT_Vector nativeSecond{640, 640};
    FT_Vector nativeTo{640, 0};
    Require(FT_Stroker_CubicTo_nid_postfix(pair.guest, &first, &second, &to) == FT_Stroker_CubicTo(pair.native, &nativeFirst, &nativeSecond, &nativeTo));
    Require(FT_Stroker_EndSubPath_nid_postfix(pair.guest) == FT_Stroker_EndSubPath(pair.native));
    pair.CountsMatch();
    Require(FT_Stroker_CubicTo_nid_postfix(pair.guest, &first, nullptr, &to) == FT_Stroker_CubicTo(pair.native, &nativeFirst, nullptr, &nativeTo));
    Require(FT_Stroker_CubicTo_nid_postfix(nullptr, &first, &second, &to) == FT_Stroker_CubicTo(nullptr, &nativeFirst, &nativeSecond, &nativeTo));
}

static int ParseFresh(GuestLibrary& library, FT_Library native, Shape& shape, std::uint8_t opened) {
    Pair pair(library, native);
    const int error = pair.Parse(shape.Guest(), shape.Native(), opened);
    pair.CountsMatch();
    pair.BorderCountsMatch();
    return error;
}

static void TestParseOutline(GuestLibrary& library, FT_Library native) {
    for (Shape& shape : Shapes()) {
        for (const std::uint8_t opened : {0, 1}) {
            for (const std::int32_t join : {0, 1, 2}) {
                Pair pair(library, native);
                FT_Stroker_Set_nid_postfix(pair.guest, 48, FT_STROKER_LINECAP_ROUND, join, 0x20000);
                FT_Stroker_Set(pair.native, 48, FT_STROKER_LINECAP_ROUND, static_cast<FT_Stroker_LineJoin>(join), 0x20000);
                pair.Square(1);
                Require(pair.Parse(shape.Guest(), shape.Native(), opened) == 0);
                pair.CountsMatch();
                pair.BorderCountsMatch();
                std::uint32_t points = 0;
                std::uint32_t contours = 0;
                Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points != 0 && contours != 0);
            }
        }
    }
    std::vector<Shape> invalid;
    invalid.push_back(Shape({{0, 0}, {320, 640}, {640, 0}}, {1, 2, 1}, {2}));
    invalid.push_back(Shape({{0, 0}, {320, 640}, {640, 0}}, {1, 0, 2}, {2}));
    invalid.push_back(Shape({{0, 0}, {640, 0}, {640, 640}, {0, 640}, {320, 320}, {0, 0}}, {1, 1, 1, 1, 2, 1}, {3, 5}));
    Shape cubicStart({{0, 0}, {320, 640}, {640, 0}}, {2, 2, 1}, {2});
    for (const std::uint8_t opened : {0, 1}) {
        for (Shape& shape : invalid) Require(ParseFresh(library, native, shape, opened) == FT_Err_Invalid_Outline);
        ParseFresh(library, native, cubicStart, opened);
    }
    Pair pair(library, native);
    pair.Square(0);
    Shape square = SquareOutline();
    const GuestOutline outline = square.Guest();
    FT_Outline nativeOutline = square.Native();
    Require(FT_Stroker_ParseOutline_nid_postfix(pair.guest, nullptr, 0) == FT_Err_Invalid_Outline);
    Require(FT_Stroker_ParseOutline(pair.native, nullptr, 0) == FT_Err_Invalid_Outline);
    Require(FT_Stroker_ParseOutline_nid_postfix(nullptr, nullptr, 1) == FT_Err_Invalid_Outline);
    Require(FT_Stroker_ParseOutline_nid_postfix(nullptr, &outline, 0) == FT_Err_Invalid_Argument);
    Require(FT_Stroker_ParseOutline(nullptr, &nativeOutline, 0) == FT_Err_Invalid_Argument);
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points != 0 && contours != 0);
    pair.CountsMatch();
    Shape empty({}, {}, {});
    Require(pair.Parse(empty.Guest(), empty.Native(), 0) == 0);
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points == 0 && contours == 0);
    pair.CountsMatch();
}

static void ParseAtHostLimit(GuestLibrary& library, FT_Library native, GuestOutline outline, FT_Outline nativeOutline, bool forwarded) {
    Pair pair(library, native);
    pair.Square(0);
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points != 0);
    if (forwarded) {
        Require(pair.Parse(outline, nativeOutline, 0) == 0);
        pair.CountsMatch();
    } else {
        Require(Throws([&] { FT_Stroker_ParseOutline_nid_postfix(pair.guest, &outline, 0); }));
        std::uint32_t keptPoints = 0;
        std::uint32_t keptContours = 0;
        Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &keptPoints, &keptContours) == 0 && keptPoints == points && keptContours == contours);
    }
}

static void TestParseOutlineHostLimits(GuestLibrary& library, FT_Library native) {
    constexpr std::int64_t top = std::numeric_limits<std::int32_t>::max();
    constexpr std::int64_t bottom = std::numeric_limits<std::int32_t>::min();
    Shape aboveX({{0, 0}, {640, 0}, {640, 640}, {top + 1, 640}}, {1, 1, 1, 1}, {3});
    Shape belowY({{0, 0}, {640, bottom - 1}, {640, 640}, {0, 640}}, {1, 1, 1, 1}, {3});
    ParseAtHostLimit(library, native, aboveX.Guest(), aboveX.Native(), false);
    ParseAtHostLimit(library, native, belowY.Guest(), belowY.Native(), false);
    const GuestOutline wide = aboveX.Guest();
    Require(FT_Stroker_ParseOutline_nid_postfix(nullptr, &wide, 0) == FT_Err_Invalid_Argument);
    Shape skipped({{top + 1, bottom - 1}, {0, 0}, {640, 0}, {640, 640}, {0, 640}}, {1, 1, 1, 1, 1}, {0, 4});
    ParseAtHostLimit(library, native, skipped.Guest(), skipped.Native(), SharedPoints);
    if constexpr (SharedPoints) {
        Shape edge({{0, 0}, {top, 0}, {top, 640}, {0, bottom}}, {1, 1, 1, 1}, {3});
        ParseAtHostLimit(library, native, edge.Guest(), edge.Native(), true);
    }
}

static void TestParseOutlineArrays(GuestLibrary& library, FT_Library native) {
    Shape square = SquareOutline();
    const GuestOutline valid = square.Guest();
    const FT_Outline nativeValid = square.Native();
    GuestOutline noContours = valid;
    noContours.contours = nullptr;
    GuestOutline noPoints = valid;
    noPoints.points = nullptr;
    GuestOutline noTags = valid;
    noTags.tags = nullptr;
    GuestOutline endAtCount = valid;
    endAtCount.n_points = 3;
    GuestOutline endPastCount = valid;
    endPastCount.n_points = 2;
    for (const GuestOutline& outline : {noContours, noPoints, noTags, endAtCount, endPastCount}) {
        ParseAtHostLimit(library, native, outline, nativeValid, false);
        Require(FT_Stroker_ParseOutline_nid_postfix(nullptr, &outline, 0) == FT_Err_Invalid_Argument);
    }
    std::uint16_t end = 0;
    ParseAtHostLimit(library, native, GuestOutline{1, 1, nullptr, nullptr, &end, 0}, FT_Outline{1, 1, nullptr, nullptr, &end, 0}, false);
    ParseAtHostLimit(library, native, GuestOutline{0, 4, nullptr, nullptr, nullptr, 0}, FT_Outline{0, 4, nullptr, nullptr, nullptr, 0}, true);
}

static void TestExportBorder(GuestLibrary& library, FT_Library native) {
    std::size_t exported = 0;
    for (Shape& shape : Shapes()) {
        for (const std::uint8_t opened : {0, 1}) {
            for (const std::int32_t border : {0, 1}) {
                Pair pair(library, native);
                Require(pair.Parse(shape.Guest(), shape.Native(), opened) == 0);
                std::uint32_t points = 0;
                std::uint32_t contours = 0;
                FT_UInt nativePoints = 0;
                FT_UInt nativeContours = 0;
                Require(FT_Stroker_GetBorderCounts_nid_postfix(pair.guest, border, &points, &contours) == 0);
                Require(FT_Stroker_GetBorderCounts(pair.native, static_cast<FT_StrokerBorder>(border), &nativePoints, &nativeContours) == 0);
                Require(points == nativePoints && contours == nativeContours);
                Target target(3, points, contours);
                if constexpr (SharedPoints) {
                    FT_Stroker_ExportBorder_nid_postfix(pair.guest, border, &target.guest);
                    FT_Stroker_ExportBorder(pair.native, static_cast<FT_StrokerBorder>(border), &target.native);
                    Require(target.Matches() && target.guest.n_points == 3 + points && target.guest.n_contours == 1 + contours);
                } else {
                    Require(Throws([&] { FT_Stroker_ExportBorder_nid_postfix(pair.guest, border, &target.guest); }) && target.Pristine());
                }
                exported += points;
            }
        }
    }
    Require(exported != 0);
}

static void TestExport(GuestLibrary& library, FT_Library native) {
    for (Shape& shape : Shapes()) {
        for (const std::uint8_t opened : {0, 1}) {
            Pair pair(library, native);
            Require(pair.Parse(shape.Guest(), shape.Native(), opened) == 0);
            std::uint32_t points = 0;
            std::uint32_t contours = 0;
            Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points != 0 && contours != 0);
            pair.CountsMatch();
            Target target(3, points, contours);
            if constexpr (SharedPoints) {
                FT_Stroker_Export_nid_postfix(pair.guest, &target.guest);
                FT_Stroker_Export(pair.native, &target.native);
                Require(target.Matches() && target.guest.n_points == 3 + points && target.guest.n_contours == 1 + contours);
            } else {
                Require(Throws([&] { FT_Stroker_Export_nid_postfix(pair.guest, &target.guest); }) && target.Pristine());
            }
        }
    }
    if constexpr (SharedPoints) {
        Shape square = SquareOutline();
        Pair pair(library, native);
        Require(pair.Parse(square.Guest(), square.Native(), 0) == 0);
        std::uint32_t points = 0;
        std::uint32_t contours = 0;
        Require(FT_Stroker_GetCounts_nid_postfix(pair.guest, &points, &contours) == 0 && points > 6);
        pair.CountsMatch();
        Target target(65530, points, contours);
        FT_Stroker_Export_nid_postfix(pair.guest, &target.guest);
        FT_Stroker_Export(pair.native, &target.native);
        Require(target.Matches() && target.guest.n_points == static_cast<std::uint16_t>(65530 + points) && target.guest.n_points < 65530);
    }
}

static void TestExportUnchanged(GuestLibrary& library, FT_Library native) {
    Shape square = SquareOutline();
    Pair sizing(library, native);
    Require(sizing.Parse(square.Guest(), square.Native(), 0) == 0);
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(sizing.guest, &points, &contours) == 0 && points != 0 && contours != 0);
    Pair pair(library, native);
    Require(pair.Parse(square.Guest(), square.Native(), 0) == 0);
    Target target(3, points, contours);
    if constexpr (SharedPoints) {
        FT_Stroker_Export_nid_postfix(pair.guest, &target.guest);
        FT_Stroker_Export(pair.native, &target.native);
        for (const std::int32_t border : {0, 1}) {
            FT_Stroker_ExportBorder_nid_postfix(pair.guest, border, &target.guest);
            FT_Stroker_ExportBorder(pair.native, static_cast<FT_StrokerBorder>(border), &target.native);
        }
    } else {
        Require(Throws([&] { FT_Stroker_Export_nid_postfix(pair.guest, &target.guest); }));
        Require(Throws([&] { FT_Stroker_ExportBorder_nid_postfix(pair.guest, 0, &target.guest); }));
    }
    Require(target.Pristine());
    pair.CountsMatch();
    for (const std::int32_t border : {2, -1}) {
        FT_Stroker_ExportBorder_nid_postfix(pair.guest, border, &target.guest);
        FT_Stroker_ExportBorder(pair.native, static_cast<FT_StrokerBorder>(border), &target.native);
    }
    FT_Stroker_ExportBorder_nid_postfix(nullptr, 0, &target.guest);
    FT_Stroker_ExportBorder(nullptr, FT_STROKER_BORDER_LEFT, &target.native);
    FT_Stroker_ExportBorder_nid_postfix(pair.guest, 1, nullptr);
    FT_Stroker_ExportBorder(pair.native, FT_STROKER_BORDER_RIGHT, nullptr);
    FT_Stroker_Export_nid_postfix(nullptr, &target.guest);
    FT_Stroker_Export(nullptr, &target.native);
    FT_Stroker_Export_nid_postfix(pair.guest, nullptr);
    FT_Stroker_Export(pair.native, nullptr);
    Require(target.Pristine());
    if constexpr (SharedPoints) {
        FT_Stroker_Export_nid_postfix(pair.guest, &target.guest);
        FT_Stroker_Export(pair.native, &target.native);
        Require(target.Matches() && target.guest.n_points == 3 + points && target.guest.n_contours == 1 + contours);
        FT_Stroker_Rewind_nid_postfix(pair.guest);
        FT_Stroker_Rewind(pair.native);
        Target rewound(3, points, contours);
        FT_Stroker_Export_nid_postfix(pair.guest, &rewound.guest);
        FT_Stroker_Export(pair.native, &rewound.native);
        Require(rewound.Pristine());
    } else {
        Require(Throws([&] { FT_Stroker_ExportBorder_nid_postfix(pair.guest, 1, &target.guest); }) && target.Pristine());
    }
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    TestNew(library, native);
    TestDone(library, native);
    TestGetCounts(library, native);
    TestBeginSubPath(library, native);
    TestLineTo(library, native);
    TestEndSubPath(library, native);
    TestSet(library, native);
    TestRewind(library, native);
    TestGetBorderCounts(library, native);
    TestConicTo(library, native);
    TestCubicTo(library, native);
    TestParseOutline(library, native);
    TestParseOutlineHostLimits(library, native);
    TestParseOutlineArrays(library, native);
    TestExportBorder(library, native);
    TestExport(library, native);
    TestExportUnchanged(library, native);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
