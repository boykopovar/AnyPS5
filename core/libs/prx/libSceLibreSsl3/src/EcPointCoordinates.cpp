#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <vector>

namespace {

constexpr int PointConversionUncompressed = 4;

bool WriteCoordinate(const Bignum* value, int degree, unsigned char* out, std::size_t fieldSize) {
    const int bits = BN_num_bits_nid_postfix(value);
    if (bits != 0 && value->neg != 0) return false;
    if (bits > degree) return false;
    const std::size_t length = (static_cast<std::size_t>(bits) + 7) / 8;
    return BN_bn2bin_nid_postfix(value, out + fieldSize - length) == static_cast<int>(length);
}

} // namespace

extern "C" {

int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix(const EcGroup* group, const EcPoint* point, Bignum* x, Bignum* y, BignumContext*) {
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, PointConversionUncompressed, nullptr, 0, nullptr);
    if (size <= 1) return 0;
    std::vector<unsigned char> encoded(size);
    if (EC_POINT_point2oct_nid_postfix(group, point, PointConversionUncompressed, encoded.data(), encoded.size(), nullptr) != size) return 0;
    const int fieldSize = static_cast<int>((size - 1) / 2);
    if (x != nullptr && BN_bin2bn_nid_postfix(encoded.data() + 1, fieldSize, x) == nullptr) return 0;
    if (y != nullptr && BN_bin2bn_nid_postfix(encoded.data() + 1 + fieldSize, fieldSize, y) == nullptr) return 0;
    return 1;
}

int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix(const EcGroup* group, EcPoint* point, const Bignum* x, const Bignum* y, BignumContext*) {
    if (x == nullptr || y == nullptr) return 0;
    const int degree = EC_GROUP_get_degree_nid_postfix(group);
    if (degree <= 0) return 0;
    const std::size_t fieldSize = (static_cast<std::size_t>(degree) + 7) / 8;
    std::vector<unsigned char> encoded(1 + 2 * fieldSize, 0);
    encoded[0] = PointConversionUncompressed;
    if (!WriteCoordinate(x, degree, encoded.data() + 1, fieldSize)) return 0;
    if (!WriteCoordinate(y, degree, encoded.data() + 1 + fieldSize, fieldSize)) return 0;
    return EC_POINT_oct2point_nid_postfix(group, point, encoded.data(), encoded.size(), nullptr) == 1 ? 1 : 0;
}

}
