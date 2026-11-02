#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_STROKER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType stroker check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

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
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
