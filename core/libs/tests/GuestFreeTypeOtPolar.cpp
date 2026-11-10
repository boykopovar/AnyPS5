#include "prx/libSceFreeTypeOt/include/Polar.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

using FreeType::GuestVector;

constexpr std::int64_t Degree = 1 << 16;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot polar: %s\n", message);
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

std::int64_t Length(std::int64_t x, std::int64_t y) {
    GuestVector vector{x, y};
    return sceFreeTypeOtVectorLength_nid_no_patch(&vector);
}

bool Polar(std::int64_t x, std::int64_t y, std::int64_t length, std::int64_t angle) {
    GuestVector vector{x, y};
    std::int64_t gotLength = -1;
    std::int64_t gotAngle = -1;
    sceFreeTypeOtVectorPolarize_nid_no_patch(&vector, &gotLength, &gotAngle);
    return gotLength == length && gotAngle == angle && vector.x == x && vector.y == y;
}

bool Rotates(GuestVector vector, std::int64_t angle, GuestVector expected) {
    sceFreeTypeOtVectorRotate_nid_no_patch(&vector, angle);
    return vector.x == expected.x && vector.y == expected.y;
}

void Lengths() {
    Require(Length(3 * Degree, 4 * Degree) == 5 * Degree, "a 3-4-5 vector has length 5");
    Require(Length(-300, 400) == 500 && Length(-5, -12) == 13 && Length(1, 1) == 1, "small vectors round to the CORDIC length");
    Require(Length(0x12345, -0x6789) == 79136 && Length(100000000, 3) == 100000002, "the 2.4.11 CORDIC lengths are kept");
    Require(Length(0, -7) == 7 && Length(-9, 0) == 9 && Length(0, 0) == 0, "an axis vector returns the absolute component");
    Require(Length(0, std::numeric_limits<std::int64_t>::max()) == std::numeric_limits<std::int64_t>::max(), "the axis shortcut takes any magnitude");
    Require(Throws([] { sceFreeTypeOtVectorLength_nid_no_patch(nullptr); }), "a null vector throws");
}

void Polarize() {
    Require(Polar(3 * Degree, 4 * Degree, 5 * Degree, 3481920), "a 3-4-5 vector has its CORDIC angle");
    Require(Polar(-64, 0, 64, 180 * Degree) && Polar(0, -64, 64, -90 * Degree), "axis vectors have exact angles");
    Require(Polar(-100, 100, 141, 135 * Degree) && Polar(1000, -1, 1000, -3744), "the length is truncated and the angle padded to 32");
    GuestVector zero{0, 0};
    std::int64_t length = 11;
    std::int64_t angle = 22;
    sceFreeTypeOtVectorPolarize_nid_no_patch(&zero, &length, &angle);
    Require(length == 11 && angle == 22, "a zero vector leaves both outputs");
    sceFreeTypeOtVectorPolarize_nid_no_patch(&zero, nullptr, nullptr);
    GuestVector vector{3, 4};
    Require(Throws([&] { sceFreeTypeOtVectorPolarize_nid_no_patch(&vector, &length, nullptr); }) && length == 11, "a null angle throws before writing");
    Require(Throws([&] { sceFreeTypeOtVectorPolarize_nid_no_patch(nullptr, &length, &angle); }), "a null vector throws");
}

void Rotate() {
    Require(Rotates({Degree, 0}, 90 * Degree, {0, Degree}), "a quarter turn is exact");
    Require(Rotates({Degree, 0}, 30 * Degree, {56756, 32768}), "a 30 degree turn uses the CORDIC");
    Require(Rotates({100, 200}, -45 * Degree, {212, 71}) && Rotates({-640, 64}, 180 * Degree, {640, -64}), "turns round each component");
    Require(Rotates({64, 64}, -270 * Degree, {-64, 64}), "negative turns past a half turn wrap");
    Require(Rotates({Degree, 0}, 30 * Degree + 360 * Degree * 1000000000LL, {56756, 32768}), "whole turns are reduced without looping");
    Require(Rotates({5 * Degree, 0}, 1, {5 * Degree, 0}), "a tiny turn keeps the vector");
    Require(Rotates({7, -9}, 0, {7, -9}) && Rotates({0, 0}, 45 * Degree, {0, 0}), "a zero angle or vector is unchanged");
    Require(Rotates({0x7FFFFFFFFFFFLL, 3}, 0, {0x7FFFFFFFFFFFLL, 3}), "a zero angle takes any magnitude");
    Require(Throws([] { sceFreeTypeOtVectorRotate_nid_no_patch(nullptr, Degree); }), "a null vector throws");

    GuestVector vector{-1, -1};
    sceFreeTypeOtVectorFromPolar_nid_no_patch(&vector, Degree, 30 * Degree);
    Require(vector.x == 56756 && vector.y == 32768, "FT_Vector_From_Polar rotates the length");
    sceFreeTypeOtVectorFromPolar_nid_no_patch(&vector, -5, 0);
    Require(vector.x == -5 && vector.y == 0, "FT_Vector_From_Polar with no angle stores the length");
    Require(Throws([] { sceFreeTypeOtVectorFromPolar_nid_no_patch(nullptr, Degree, 0); }), "FT_Vector_From_Polar throws for a null vector");
}

}

int main() {
    Lengths();
    Polarize();
    Rotate();
    std::puts("Guest FreeType Ot polar tests passed");
    return 0;
}
