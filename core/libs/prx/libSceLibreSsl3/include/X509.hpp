#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"

struct LibreSsl3X509CertInfo {
    Asn1String* version;
    std::array<std::uint8_t, 88> fields;
    std::int32_t encModified;
    std::int32_t padding;
};

struct LibreSsl3X509 {
    LibreSsl3X509CertInfo* certInfo;
    void* sigAlg;
    void* signature;
    std::int32_t valid;
    std::int32_t references;
};

static_assert(sizeof(LibreSsl3X509CertInfo) == 104);
static_assert(offsetof(LibreSsl3X509CertInfo, encModified) == 96);
static_assert(sizeof(LibreSsl3X509) == 32);
static_assert(offsetof(LibreSsl3X509, references) == 28);
