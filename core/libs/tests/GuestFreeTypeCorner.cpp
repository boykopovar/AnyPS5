#include "prx/libSceFreeType/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType corner: %s\n", message);
    std::exit(1);
}

}  // namespace

int main() {
    Require(ft_corner_orientation_nid_postfix(1, 0, 0, 1) == 1, "counter-clockwise 90-degree turn is +1");
    Require(ft_corner_orientation_nid_postfix(0, 1, 1, 0) == -1, "clockwise 90-degree turn is -1");
    Require(ft_corner_orientation_nid_postfix(1, 2, 2, 4) == 0, "collinear parallel vectors are 0");
    Require(ft_corner_orientation_nid_postfix(1, 0, -1, 0) == 0, "collinear opposite vectors are 0");
    Require(ft_corner_orientation_nid_postfix(0, 0, 5, 5) == 0, "zero input vector is 0");
    Require(ft_corner_orientation_nid_postfix(5, 5, 0, 0) == 0, "zero output vector is 0");
    Require(ft_corner_orientation_nid_postfix(1000000000LL, 0, 0, 1000000000LL) == 1, "large counter-clockwise coordinates");
    Require(ft_corner_orientation_nid_postfix(0, 1000000000LL, 1000000000LL, 0) == -1, "large clockwise coordinates");

    Require(ft_corner_is_flat_nid_postfix(100, 0, 100, 0) == 1, "parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(1000, 1, 1000, -1) == 1, "nearly parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, 0, 100) == 0, "perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, -100, 0) == 0, "reversing vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100000, 0, 1, 10) == 1, "dominant in vector is flat");
    Require(ft_corner_is_flat_nid_postfix(1, 10, 100000, 0) == 1, "dominant out vector is flat");
    Require(ft_corner_is_flat_nid_postfix(0, 0, 0, 0) == 0, "zero vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(-100, -50, -200, -100) == 1, "negative coordinates flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 1, 0) == 1, "INT64_MAX boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MIN, 0, -1, 0) == 1, "INT64_MIN boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, 1, 1) == 1, "large positive diagonal boundary");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, INT64_MAX, INT64_MAX) == 1, "identical full-range vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 0, INT64_MAX) == 0, "full-range perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 1, 1, INT64_MAX) == 0, "full-range near-perpendicular vectors are not flat");

    std::puts("Guest FreeType corner tests passed");
    return 0;
}
