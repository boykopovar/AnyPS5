#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EC_POINT_get_affine_coordinates_GFp check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(716);
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(716);
    Require(group != nullptr && key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    const EcPoint* point = EC_KEY_get0_public_key_nid_postfix(key);
    Require(point != nullptr);

    Bignum* x = BN_new_nid_postfix();
    Bignum* y = BN_new_nid_postfix();
    Require(x != nullptr && y != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, x, y, nullptr) == 1);
    Require(BN_num_bits_nid_postfix(x) > 0);
    Require(BN_num_bits_nid_postfix(y) > 0);

    Bignum* onlyX = BN_new_nid_postfix();
    Require(onlyX != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, onlyX, nullptr, nullptr) == 1);
    Require(BN_num_bits_nid_postfix(onlyX) == BN_num_bits_nid_postfix(x));
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, nullptr, nullptr, nullptr) == 1);

    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(409);
    EcPoint* otherPoint = EC_POINT_new_nid_postfix(other);
    Require(other != nullptr && otherPoint != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, otherPoint, x, y, nullptr) == 0);

    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    Require(infinity != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, infinity, x, y, nullptr) == 0);
    RequireThrows([&] { EC_POINT_get_affine_coordinates_GFp_nid_postfix(nullptr, point, x, y, nullptr); });
    RequireThrows([&] { EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, nullptr, x, y, nullptr); });

    BN_free_nid_postfix(x);
    BN_free_nid_postfix(y);
    BN_free_nid_postfix(onlyX);
    EC_POINT_free_nid_postfix(infinity);
    EC_POINT_free_nid_postfix(otherPoint);
    EC_GROUP_free_nid_postfix(other);
    EC_KEY_free_nid_postfix(key);
    EC_GROUP_free_nid_postfix(group);
    std::puts("EC_POINT_get_affine_coordinates_GFp tests passed");
}
