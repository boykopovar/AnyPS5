#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

namespace {
enum EcReason {
    BufferTooSmall = 100,
    InvalidEncoding = 102,
    PointIsNotOnCurve = 107,
    InvalidCompressedPoint = 110,
};

struct Integer {
    mbedtls_mpi value;

    Integer() { mbedtls_mpi_init(&value); }
    ~Integer() { mbedtls_mpi_free(&value); }
    Integer(const Integer&) = delete;
    Integer& operator=(const Integer&) = delete;
};

void Check(int result) {
    if (result != 0) NotImplemented_nid_no_patch(__func__);
}

int Error(int reason) {
    LibreSslPutError_nid_no_patch(16, 0xfff, reason);
    return 0;
}

void Multiply(mbedtls_mpi* result, const mbedtls_mpi* a, const mbedtls_mpi* b, const mbedtls_mpi* prime) {
    Check(mbedtls_mpi_mul_mpi(result, a, b));
    Check(mbedtls_mpi_mod_mpi(result, result, prime));
}

bool SquareRoot(mbedtls_mpi* root, const mbedtls_mpi* value, const mbedtls_mpi* prime) {
    if (mbedtls_mpi_cmp_int(value, 0) == 0) {
        Check(mbedtls_mpi_lset(root, 0));
        return true;
    }
    Integer exponent, test;
    Check(mbedtls_mpi_sub_int(&exponent.value, prime, 1));
    Check(mbedtls_mpi_shift_r(&exponent.value, 1));
    Check(mbedtls_mpi_exp_mod(&test.value, value, &exponent.value, prime, nullptr));
    if (mbedtls_mpi_cmp_int(&test.value, 1) != 0) return false;
    if (mbedtls_mpi_get_bit(prime, 1)) {
        Check(mbedtls_mpi_add_int(&exponent.value, prime, 1));
        Check(mbedtls_mpi_shift_r(&exponent.value, 2));
        Check(mbedtls_mpi_exp_mod(root, value, &exponent.value, prime, nullptr));
        return true;
    }
    Integer odd, nonresidue, c, t, b;
    Check(mbedtls_mpi_sub_int(&odd.value, prime, 1));
    size_t powers = mbedtls_mpi_lsb(&odd.value);
    Check(mbedtls_mpi_shift_r(&odd.value, powers));
    Check(mbedtls_mpi_lset(&nonresidue.value, 2));
    for (;;) {
        Check(mbedtls_mpi_exp_mod(&test.value, &nonresidue.value, &exponent.value, prime, nullptr));
        if (mbedtls_mpi_cmp_int(&test.value, 1) != 0) break;
        Check(mbedtls_mpi_add_int(&nonresidue.value, &nonresidue.value, 1));
    }
    Check(mbedtls_mpi_exp_mod(&c.value, &nonresidue.value, &odd.value, prime, nullptr));
    Check(mbedtls_mpi_exp_mod(&t.value, value, &odd.value, prime, nullptr));
    Check(mbedtls_mpi_add_int(&exponent.value, &odd.value, 1));
    Check(mbedtls_mpi_shift_r(&exponent.value, 1));
    Check(mbedtls_mpi_exp_mod(root, value, &exponent.value, prime, nullptr));
    while (mbedtls_mpi_cmp_int(&t.value, 1) != 0) {
        Check(mbedtls_mpi_copy(&test.value, &t.value));
        size_t i = 0;
        do {
            Multiply(&test.value, &test.value, &test.value, prime);
            ++i;
        } while (i < powers && mbedtls_mpi_cmp_int(&test.value, 1) != 0);
        if (i >= powers) NotImplemented_nid_no_patch(__func__);
        Check(mbedtls_mpi_copy(&b.value, &c.value));
        for (size_t j = 0; j < powers - i - 1; ++j) Multiply(&b.value, &b.value, &b.value, prime);
        Multiply(root, root, &b.value, prime);
        Multiply(&c.value, &b.value, &b.value, prime);
        Multiply(&t.value, &t.value, &c.value, prime);
        powers = i;
    }
    return true;
}

bool DecodeY(const mbedtls_ecp_group& group, mbedtls_ecp_point& point, int parity) {
    Integer rhs, linear;
    Multiply(&rhs.value, &point.MBEDTLS_PRIVATE(X), &point.MBEDTLS_PRIVATE(X), &group.P);
    Multiply(&rhs.value, &rhs.value, &point.MBEDTLS_PRIVATE(X), &group.P);
    if (group.A.MBEDTLS_PRIVATE(p) == nullptr) {
        Check(mbedtls_mpi_mul_int(&linear.value, &point.MBEDTLS_PRIVATE(X), 3));
        Check(mbedtls_mpi_sub_mpi(&rhs.value, &rhs.value, &linear.value));
    } else {
        Multiply(&linear.value, &group.A, &point.MBEDTLS_PRIVATE(X), &group.P);
        Check(mbedtls_mpi_add_mpi(&rhs.value, &rhs.value, &linear.value));
    }
    Check(mbedtls_mpi_add_mpi(&rhs.value, &rhs.value, &group.B));
    Check(mbedtls_mpi_mod_mpi(&rhs.value, &rhs.value, &group.P));
    if (!SquareRoot(&point.MBEDTLS_PRIVATE(Y), &rhs.value, &group.P)) return false;
    if (mbedtls_mpi_get_bit(&point.MBEDTLS_PRIVATE(Y), 0) != parity) {
        if (mbedtls_mpi_cmp_int(&point.MBEDTLS_PRIVATE(Y), 0) == 0) NotImplemented_nid_no_patch(__func__);
        Check(mbedtls_mpi_sub_mpi(&point.MBEDTLS_PRIVATE(Y), &group.P, &point.MBEDTLS_PRIVATE(Y)));
    }
    return true;
}
}

extern "C" {
int APS5_VABI EC_POINT_oct2point_nid_postfix(const EcGroup* group, EcPoint* point, const unsigned char* buffer, size_t length, void*) {
    if (group == nullptr || point == nullptr) APS5_INVALID_ARG_EX;
    if (group->curve.nid != point->curve.nid) NotImplemented_nid_no_patch(__func__);
    if (length == 0) return Error(BufferTooSmall);
    if (buffer == nullptr) APS5_INVALID_ARG_EX;
    const unsigned form = buffer[0] & ~1u;
    const int parity = buffer[0] & 1;
    if ((form != 0 && form != 2 && form != 4 && form != 6) || ((form == 0 || form == 4) && parity)) return Error(InvalidEncoding);
    if (form == 0) {
        if (length != 1) return Error(InvalidEncoding);
        Check(mbedtls_mpi_lset(&point->point.MBEDTLS_PRIVATE(Z), 0));
        return 1;
    }
    const size_t fieldLength = mbedtls_mpi_size(&group->group.P);
    if (length != 1 + (form == 2 ? fieldLength : 2 * fieldLength)) return Error(InvalidEncoding);
    EcPoint decoded(group->curve);
    Check(mbedtls_mpi_read_binary(&decoded.point.MBEDTLS_PRIVATE(X), buffer + 1, fieldLength));
    if (mbedtls_mpi_cmp_mpi(&decoded.point.MBEDTLS_PRIVATE(X), &group->group.P) >= 0) return Error(InvalidEncoding);
    if (form == 2) {
        ERR_clear_error_nid_postfix();
        if (!DecodeY(group->group, decoded.point, parity)) return Error(InvalidCompressedPoint);
    } else {
        Check(mbedtls_mpi_read_binary(&decoded.point.MBEDTLS_PRIVATE(Y), buffer + 1 + fieldLength, fieldLength));
        if (mbedtls_mpi_cmp_mpi(&decoded.point.MBEDTLS_PRIVATE(Y), &group->group.P) >= 0) return Error(InvalidEncoding);
        if (form == 6 && mbedtls_mpi_get_bit(&decoded.point.MBEDTLS_PRIVATE(Y), 0) != parity) return Error(InvalidEncoding);
    }
    Check(mbedtls_mpi_lset(&decoded.point.MBEDTLS_PRIVATE(Z), 1));
    mbedtls_mpi_swap(&point->point.MBEDTLS_PRIVATE(X), &decoded.point.MBEDTLS_PRIVATE(X));
    mbedtls_mpi_swap(&point->point.MBEDTLS_PRIVATE(Y), &decoded.point.MBEDTLS_PRIVATE(Y));
    mbedtls_mpi_swap(&point->point.MBEDTLS_PRIVATE(Z), &decoded.point.MBEDTLS_PRIVATE(Z));
    const int validation = mbedtls_ecp_check_pubkey(&group->group, &point->point);
    if (validation == MBEDTLS_ERR_ECP_INVALID_KEY) return Error(PointIsNotOnCurve);
    Check(validation);
    return 1;
}
}
