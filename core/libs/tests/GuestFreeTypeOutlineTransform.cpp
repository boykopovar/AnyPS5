#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_Embolden_nid_postfix(GuestOutline*, std::int64_t);
int APS5_VABI FT_Outline_EmboldenXY_nid_postfix(GuestOutline*, std::int64_t, std::int64_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline transform check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Shape {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

static Shape Square(bool clockwise) {
    Shape shape{{{0, 0}, {640, 0}, {640, 640}, {0, 640}}, {1, 1, 1, 1}, {3}};
    if (clockwise) std::swap(shape.points[1], shape.points[3]);
    return shape;
}

static Shape Frame() {
    return {{{0, 0}, {960, 0}, {960, 960}, {0, 960}, {240, 240}, {240, 720}, {720, 720}, {720, 240}}, {1, 1, 1, 1, 1, 1, 1, 1}, {3, 7}};
}

static Shape Curved() {
    return {{{0, 0}, {320, 0}, {640, 320}, {320, 640}, {0, 640}}, {1, 0, 1, 0, 1}, {4}};
}

struct Pair {
    std::vector<GuestVector> guestPoints;
    Shape shape;
    GuestOutline guest{};
    FT_Outline native{};
    std::vector<FT_Vector> nativePoints;

    explicit Pair(Shape input) : shape(std::move(input)), nativePoints(shape.points) {
        for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
        const auto points = static_cast<std::uint16_t>(shape.points.size());
        const auto contours = static_cast<std::uint16_t>(shape.contours.size());
        guest = {contours, points, guestPoints.data(), shape.tags.data(), shape.contours.data(), 0};
        native = {static_cast<unsigned short>(contours), static_cast<unsigned short>(points), nativePoints.data(), shape.tags.data(), shape.contours.data(), 0};
    }

    void Matches() const {
        Require(guest.n_points == native.n_points && guest.n_contours == native.n_contours);
        for (std::size_t index = 0; index < nativePoints.size(); ++index) {
            Require(guestPoints[index].x == nativePoints[index].x && guestPoints[index].y == nativePoints[index].y);
        }
    }
};

static void TestEmbolden() {
    for (const auto& make : {+[] { return Square(false); }, +[] { return Square(true); }, +[] { return Frame(); }, +[] { return Curved(); }}) {
        for (const std::int64_t strength : {0, 1, 32, 64, 128, 512, -32, -64}) {
            Pair pair(make());
            const int guestError = FT_Outline_Embolden_nid_postfix(&pair.guest, strength);
            const int nativeError = FT_Outline_Embolden(&pair.native, static_cast<FT_Pos>(strength));
            Require(guestError == nativeError);
            pair.Matches();
        }
    }
    Require(FT_Outline_Embolden_nid_postfix(nullptr, 64) == FT_Outline_Embolden(nullptr, 64));
    GuestOutline empty{};
    FT_Outline nativeEmpty{};
    Require(FT_Outline_Embolden_nid_postfix(&empty, 64) == FT_Outline_Embolden(&nativeEmpty, 64));
}

static void TestEmboldenXY() {
    for (const auto& make : {+[] { return Square(false); }, +[] { return Frame(); }, +[] { return Curved(); }}) {
        for (const std::int64_t x : {0, 32, 64, -32}) {
            for (const std::int64_t y : {0, 64, 128, -64}) {
                Pair pair(make());
                const int guestError = FT_Outline_EmboldenXY_nid_postfix(&pair.guest, x, y);
                const int nativeError = FT_Outline_EmboldenXY(&pair.native, static_cast<FT_Pos>(x), static_cast<FT_Pos>(y));
                Require(guestError == nativeError);
                pair.Matches();
            }
        }
    }
    Require(FT_Outline_EmboldenXY_nid_postfix(nullptr, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32));
}

int main() {
    TestEmbolden();
    TestEmboldenXY();
}
