#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EC_POINT_set_affine_coordinates_GFp check failed at line %u\n", location.line());
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

Bignum* Number(unsigned char value) {
    Bignum* number = BN_new_nid_postfix();
    Require(number != nullptr);
    Require(BN_bin2bn_nid_postfix(&value, 1, number) == number);
    return number;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(409);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    EcPoint* otherPoint = EC_POINT_new_nid_postfix(other);
    Bignum* x = Number(1);
    Bignum* y = Number(2);
    Require(group != nullptr && other != nullptr && point != nullptr && otherPoint != nullptr);

    Bignum* gx = nullptr;
    Bignum* gy = nullptr;
    Require(BN_hex2bn_nid_postfix(&gx, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296") > 0);
    Require(BN_hex2bn_nid_postfix(&gy, "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5") > 0);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, gx, gy, nullptr) == 1);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, y, nullptr) == 0);

    gx->neg = 1;
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, gx, gy, nullptr) == 0);
    mbedtls_mpi generator;
    mbedtls_mpi expected;
    mbedtls_mpi_init(&generator);
    mbedtls_mpi_init(&expected);
    Require(mbedtls_mpi_read_string(&generator, 16, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296") == 0);
    Require(mbedtls_mpi_sub_mpi(&expected, &group->group.P, &generator) == 0);
    Require(mbedtls_mpi_cmp_mpi(&point->point.MBEDTLS_PRIVATE(X), &expected) == 0);
    mbedtls_mpi_free(&generator);
    mbedtls_mpi_free(&expected);
    gx->neg = 0;

    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, nullptr, y, nullptr) == 0);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, nullptr, nullptr) == 0);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, otherPoint, x, y, nullptr) == 0);
    RequireThrows([&] { EC_POINT_set_affine_coordinates_GFp_nid_postfix(nullptr, point, x, y, nullptr); });
    RequireThrows([&] { EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, nullptr, x, y, nullptr); });

    BN_free_nid_postfix(gx);
    BN_free_nid_postfix(gy);
    BN_free_nid_postfix(x);
    BN_free_nid_postfix(y);
    EC_POINT_free_nid_postfix(point);
    EC_POINT_free_nid_postfix(otherPoint);
    EC_GROUP_free_nid_postfix(group);
    EC_GROUP_free_nid_postfix(other);
    std::puts("EC_POINT_set_affine_coordinates_GFp tests passed");
}
