#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int EcLibrary = 16;
constexpr int AnyFunction = 0xfff;
constexpr int ErrRPassedNullParameter = 67;
constexpr int EcRIncompatibleObjects = 101;
constexpr int EcRPointAtInfinity = 106;
constexpr int EcRPointIsNotOnCurve = 107;
constexpr unsigned char UncompressedPoint = 0x04;

struct Mpi {
    Mpi() {
        mbedtls_mpi_init(&value);
    }

    ~Mpi() {
        mbedtls_mpi_free(&value);
    }

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    mbedtls_mpi value;
};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(EcLibrary, AnyFunction, reason);
}

std::size_t FieldSize(const EcGroup& group) {
    return mbedtls_mpi_size(&group.group.P);
}

bool SameCurve(const EcGroup& group, const EcPoint& point) {
    return group.curve.nid == point.curve.nid;
}

std::vector<unsigned char> Magnitude(const LibreSsl3Bignum* number, const char* caller) {
    const int bits = BN_num_bits_nid_postfix(reinterpret_cast<const Bignum*>(number));
    if (bits < 0) NotImplemented_nid_no_patch(caller);
    const int length = (bits + 7) / 8;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (length != 0 &&
        BN_bn2bin_nid_postfix(reinterpret_cast<const Bignum*>(number), bytes.data()) != length)
        NotImplemented_nid_no_patch(caller);
    return bytes;
}

std::vector<unsigned char> Coordinate(const LibreSsl3Bignum* number, const EcGroup& group, const char* caller) {
    LibreSsl3Bignum record{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&record, number, sizeof(record));
    errno = savedErrno;
    if (!readable) NotImplemented_nid_no_patch(caller);
    const std::vector<unsigned char> magnitude = Magnitude(number, caller);
    Mpi value;
    if (mbedtls_mpi_read_binary(&value.value, magnitude.data(), magnitude.size()) != 0)
        NotImplemented_nid_no_patch(caller);
    if (mbedtls_mpi_mod_mpi(&value.value, &value.value, &group.group.P) != 0)
        NotImplemented_nid_no_patch(caller);
    if (record.negative && mbedtls_mpi_cmp_int(&value.value, 0) != 0) {
        Mpi negated;
        if (mbedtls_mpi_sub_mpi(&negated.value, &group.group.P, &value.value) != 0)
            NotImplemented_nid_no_patch(caller);
        if (mbedtls_mpi_copy(&value.value, &negated.value) != 0) NotImplemented_nid_no_patch(caller);
    }
    const std::size_t size = FieldSize(group);
    std::vector<unsigned char> encoded(size);
    if (size != 0 && mbedtls_mpi_write_binary(&value.value, encoded.data(), size) != 0)
        NotImplemented_nid_no_patch(caller);
    return encoded;
}

}

extern "C" {

int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix(const EcGroup* group, const EcPoint* point,
    LibreSsl3Bignum* x, LibreSsl3Bignum* y, void*) {
    if (!group || !point) throw std::invalid_argument("EC_POINT_get_affine_coordinates_GFp requires a group and a point");
    if (!SameCurve(*group, *point)) {
        PutError(EcRIncompatibleObjects);
        return 0;
    }
    if (mbedtls_ecp_is_zero(const_cast<mbedtls_ecp_point*>(&point->point)) == 1) {
        PutError(EcRPointAtInfinity);
        return 0;
    }
    const std::size_t size = FieldSize(*group);
    std::vector<unsigned char> encoded(1 + 2 * size);
    std::size_t written = 0;
    if (mbedtls_ecp_point_write_binary(&group->group, &point->point, MBEDTLS_ECP_PF_UNCOMPRESSED,
            &written, encoded.data(), encoded.size()) != 0 ||
        written != encoded.size())
        return 0;
    if (x && !BN_bin2bn_nid_postfix(encoded.data() + 1, static_cast<int>(size),
                 reinterpret_cast<Bignum*>(x)))
        return 0;
    if (y && !BN_bin2bn_nid_postfix(encoded.data() + 1 + size, static_cast<int>(size),
                 reinterpret_cast<Bignum*>(y)))
        return 0;
    return 1;
}

int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix(const EcGroup* group, EcPoint* point,
    const LibreSsl3Bignum* x, const LibreSsl3Bignum* y, void*) {
    if (!group || !point) throw std::invalid_argument("EC_POINT_set_affine_coordinates_GFp requires a group and a point");
    if (!SameCurve(*group, *point)) {
        PutError(EcRIncompatibleObjects);
        return 0;
    }
    if (!x || !y) {
        PutError(ErrRPassedNullParameter);
        return 0;
    }
    const std::size_t size = FieldSize(*group);
    const std::vector<unsigned char> xb = Coordinate(x, *group, __func__);
    const std::vector<unsigned char> yb = Coordinate(y, *group, __func__);
    std::vector<unsigned char> encoded(1 + 2 * size, 0);
    encoded[0] = UncompressedPoint;
    std::memcpy(encoded.data() + 1, xb.data(), size);
    std::memcpy(encoded.data() + 1 + size, yb.data(), size);
    if (mbedtls_ecp_point_read_binary(&group->group, &point->point, encoded.data(), encoded.size()) != 0)
        return 0;
    if (mbedtls_ecp_check_pubkey(&group->group, &point->point) != 0) {
        PutError(EcRPointIsNotOnCurve);
        return 0;
    }
    return 1;
}

}
