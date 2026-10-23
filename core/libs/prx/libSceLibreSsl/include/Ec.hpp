#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"

struct NamedCurve {
    int nid;
    mbedtls_ecp_group_id id;
};

struct EcGroup {
    explicit EcGroup(const NamedCurve& named);
    ~EcGroup();

    EcGroup(const EcGroup&) = delete;
    EcGroup& operator=(const EcGroup&) = delete;

    const NamedCurve curve;
    mbedtls_ecp_group group;
};

struct EcPoint {
    explicit EcPoint(const NamedCurve& named);
    ~EcPoint();

    EcPoint(const EcPoint&) = delete;
    EcPoint& operator=(const EcPoint&) = delete;

    const NamedCurve curve;
    mbedtls_ecp_point point;
};

struct EcPrivateKey {
    explicit EcPrivateKey(const NamedCurve& named);
    ~EcPrivateKey();

    EcPrivateKey(const EcPrivateKey&) = delete;
    EcPrivateKey& operator=(const EcPrivateKey&) = delete;

    const NamedCurve curve;
    mbedtls_mpi value;
};

struct EcKey {
    EcKey() = default;
    ~EcKey();

    EcKey(const EcKey&) = delete;
    EcKey& operator=(const EcKey&) = delete;

    const void* method = nullptr;
    std::int32_t version = 0;
    EcGroup* group = nullptr;
    EcPoint* publicKey = nullptr;
    EcPrivateKey* privateKey = nullptr;
    std::uint32_t encodingFlags = 0;
    std::int32_t conversionForm = 0;
    std::int32_t references = 1;
    std::int32_t flags = 0;
    void* extraData = nullptr;
};

static_assert(std::is_standard_layout_v<EcKey>);
static_assert(sizeof(EcKey) == 64);
static_assert(offsetof(EcKey, references) == 48);
