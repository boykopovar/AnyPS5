#pragma once

#include <cstdint>
#include <memory>
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

struct EcKey {
    std::unique_ptr<EcGroup> group;
    std::int32_t references = 1;
};
