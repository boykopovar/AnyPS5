#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include "prx/libc/include/General.hpp"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"

namespace {
struct NamedCurve {
    int nid;
    mbedtls_ecp_group_id id;
};

constexpr NamedCurve NamedCurves[] = {
    {409, MBEDTLS_ECP_DP_SECP192R1},
    {415, MBEDTLS_ECP_DP_SECP256R1},
    {711, MBEDTLS_ECP_DP_SECP192K1},
    {712, MBEDTLS_ECP_DP_SECP224K1},
    {713, MBEDTLS_ECP_DP_SECP224R1},
    {714, MBEDTLS_ECP_DP_SECP256K1},
    {715, MBEDTLS_ECP_DP_SECP384R1},
    {716, MBEDTLS_ECP_DP_SECP521R1},
    {928, MBEDTLS_ECP_DP_BP256R1},
    {932, MBEDTLS_ECP_DP_BP384R1},
    {934, MBEDTLS_ECP_DP_BP512R1},
};

constexpr std::pair<int, int> CurvesMissingFromMbedtls[] = {
    {410, 414}, {684, 690}, {693, 696}, {699, 710}, {717, 745}, {749, 750},
    {839, 844}, {922, 927}, {929, 931}, {933, 933}, {935, 936}, {943, 944},
};

struct EcGroup {
    explicit EcGroup(const NamedCurve& named) : curve(named) {
        mbedtls_ecp_group_init(&group);
        if (mbedtls_ecp_group_load(&group, named.id) != 0) {
            mbedtls_ecp_group_free(&group);
            throw std::runtime_error("EC_GROUP: mbedtls failed to load curve " + std::to_string(named.nid));
        }
    }

    ~EcGroup() {
        mbedtls_ecp_group_free(&group);
    }

    EcGroup(const EcGroup&) = delete;
    EcGroup& operator=(const EcGroup&) = delete;

    const NamedCurve curve;
    mbedtls_ecp_group group;
};

struct EcKey {
    std::unique_ptr<EcGroup> group;
};

std::unique_ptr<EcGroup> NewGroup(int nid) {
    for (const NamedCurve& curve : NamedCurves) {
        if (curve.nid == nid) return std::make_unique<EcGroup>(curve);
    }
    for (const auto& [first, last] : CurvesMissingFromMbedtls) {
        if (nid >= first && nid <= last) NotImplemented_nid_no_patch("EC_GROUP_new_by_curve_name: a LibreSSL curve that mbedtls does not provide");
    }
    return nullptr;
}

int DerLengthSize(int length) {
    int size = 1;
    if (length > 127) {
        for (; length > 0; length >>= 8) ++size;
    }
    return size;
}

}

extern "C" {

EcGroup* APS5_VABI EC_GROUP_new_by_curve_name_nid_postfix(int nid) {
    return NewGroup(nid).release();
}

void APS5_VABI EC_GROUP_free_nid_postfix(EcGroup* group) {
    delete group;
}

int APS5_VABI EC_GROUP_get_degree_nid_postfix(const EcGroup* group) {
    if (group == nullptr) APS5_INVALID_ARG_EX;
    return static_cast<int>(group->group.pbits);
}

EcKey* APS5_VABI EC_KEY_new_nid_postfix(void) {
    return new EcKey;
}

EcKey* APS5_VABI EC_KEY_new_by_curve_name_nid_postfix(int nid) {
    std::unique_ptr<EcGroup> group = NewGroup(nid);
    if (!group) return nullptr;
    return new EcKey{std::move(group)};
}

void APS5_VABI EC_KEY_free_nid_postfix(EcKey* key) {
    delete key;
}

const EcGroup* APS5_VABI EC_KEY_get0_group_nid_postfix(const EcKey* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    return key->group.get();
}

int APS5_VABI EC_KEY_set_group_nid_postfix(EcKey* key, const EcGroup* group) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    key->group = group ? std::make_unique<EcGroup>(group->curve) : nullptr;
    return key->group ? 1 : 0;
}

int APS5_VABI ECDSA_size_nid_postfix(const EcKey* key) {
    if (key == nullptr || !key->group) return 0;
    const int orderBytes = static_cast<int>((mbedtls_mpi_bitlen(&key->group->group.N) + 7) / 8);
    const int integerSize = 1 + DerLengthSize(orderBytes + 1) + orderBytes + 1;
    return 1 + DerLengthSize(2 * integerSize) + 2 * integerSize;
}

}
