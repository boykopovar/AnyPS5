#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

extern "C" int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix(const EcGroup* group, const EcPoint* point, Bignum* x, Bignum* y, BignumContext*) {
    if (!group || !point) throw std::invalid_argument("EC_POINT_get_affine_coordinates_GFp requires a group and a point");
    if (point->curve.nid != group->curve.nid) return 0;
    std::array<unsigned char, 256> encoded {};
    std::size_t length = 0;
    if (mbedtls_ecp_point_write_binary(&group->group, &point->point, MBEDTLS_ECP_PF_UNCOMPRESSED, &length, encoded.data(), encoded.size()) != 0) return 0;
    if (length < 3 || length % 2 == 0) return 0;
    const std::size_t coordinate = (length - 1) / 2;
    if (x && !BN_bin2bn_nid_postfix(encoded.data() + 1, static_cast<int>(coordinate), x)) return 0;
    if (y && !BN_bin2bn_nid_postfix(encoded.data() + 1 + coordinate, static_cast<int>(coordinate), y)) return 0;
    return 1;
}
