#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

bool SetCoordinate(mbedtls_mpi& coordinate, const Bignum& value, const mbedtls_mpi& modulus) {
    const int bits = BN_num_bits_nid_postfix(&value);
    if (bits < 0) return false;
    if (bits == 0) return mbedtls_mpi_lset(&coordinate, 0) == 0;
    std::vector<unsigned char> bytes((static_cast<std::size_t>(bits) + 7) / 8);
    if (BN_bn2bin_nid_postfix(&value, bytes.data()) < 0) return false;
    mbedtls_mpi raw;
    mbedtls_mpi_init(&raw);
    int result = mbedtls_mpi_read_binary(&raw, bytes.data(), bytes.size());
    if (result == 0) result = mbedtls_mpi_mod_mpi(&coordinate, &raw, &modulus);
    if (result == 0 && value.neg && mbedtls_mpi_cmp_int(&coordinate, 0) != 0)
        result = mbedtls_mpi_sub_mpi(&coordinate, &modulus, &coordinate);
    mbedtls_mpi_free(&raw);
    return result == 0;
}

}

extern "C" int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix(const EcGroup* group, EcPoint* point, const Bignum* x, const Bignum* y, BignumContext*) {
    if (!group || !point) throw std::invalid_argument("EC_POINT_set_affine_coordinates_GFp requires a group and a point");
    if (!x || !y) return 0;
    if (point->curve.nid != group->curve.nid) return 0;
    if (!SetCoordinate(point->point.MBEDTLS_PRIVATE(X), *x, group->group.P)) return 0;
    if (!SetCoordinate(point->point.MBEDTLS_PRIVATE(Y), *y, group->group.P)) return 0;
    if (mbedtls_mpi_lset(&point->point.MBEDTLS_PRIVATE(Z), 1) != 0) return 0;
    return mbedtls_ecp_check_pubkey(&group->group, &point->point) == 0 ? 1 : 0;
}
