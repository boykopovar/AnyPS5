#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslAsn1String {
    int length;
    int type;
    unsigned char* data;
    std::int64_t flags;
};

struct LibreSslOcspResponse {
    LibreSslAsn1String* responseStatus;
    void* responseBytes;
};

struct LibreSslOcspRevokedInfo {
    LibreSslAsn1String* revocationTime;
    LibreSslAsn1String* revocationReason;
};

struct LibreSslOcspCertStatus {
    int type;
    void* value;
};

struct LibreSslOcspSingleResponse {
    void* certId;
    LibreSslOcspCertStatus* certStatus;
    LibreSslAsn1String* thisUpdate;
    LibreSslAsn1String* nextUpdate;
    void* singleExtensions;
};

static_assert(sizeof(LibreSslAsn1String) == 24);
static_assert(offsetof(LibreSslAsn1String, type) == 4);
static_assert(offsetof(LibreSslAsn1String, data) == 8);
static_assert(sizeof(LibreSslOcspResponse) == 16);
static_assert(sizeof(LibreSslOcspRevokedInfo) == 16);
static_assert(sizeof(LibreSslOcspCertStatus) == 16);
static_assert(offsetof(LibreSslOcspCertStatus, value) == 8);
static_assert(sizeof(LibreSslOcspSingleResponse) == 40);
static_assert(offsetof(LibreSslOcspSingleResponse, certStatus) == 8);
static_assert(offsetof(LibreSslOcspSingleResponse, thisUpdate) == 16);
static_assert(offsetof(LibreSslOcspSingleResponse, nextUpdate) == 24);

namespace {

constexpr int Asn1Enumerated = 10;
constexpr int Asn1NegativeEnumerated = 0x100 | Asn1Enumerated;
constexpr int CertStatusRevoked = 1;

std::int64_t EnumeratedValue(const LibreSslAsn1String* value) {
    if (!value) return 0;
    const bool negative = value->type == Asn1NegativeEnumerated;
    if (!negative && value->type != Asn1Enumerated) return -1;
    if (value->length > static_cast<int>(sizeof(std::int64_t))) return -1;
    if (!value->data) return 0;
    std::uint64_t result = 0;
    for (int index = 0; index < value->length; ++index) result = (result << 8) | value->data[index];
    return static_cast<std::int64_t>(negative ? 0 - result : result);
}

}

extern "C" {

int APS5_VABI OCSP_response_status_nid_postfix(const LibreSslOcspResponse* response) {
    if (!response) throw std::invalid_argument(std::string(__func__) + ": null response");
    return static_cast<int>(EnumeratedValue(response->responseStatus));
}

int APS5_VABI OCSP_single_get0_status_nid_postfix(const LibreSslOcspSingleResponse* single, int* reason, LibreSslAsn1String** revocationTime,
    LibreSslAsn1String** thisUpdate, LibreSslAsn1String** nextUpdate) {
    if (!single) return -1;
    const LibreSslOcspCertStatus* const status = single->certStatus;
    if (!status) throw std::invalid_argument(std::string(__func__) + ": null certificate status");
    if (status->type == CertStatusRevoked && (revocationTime || reason)) {
        const auto* const revoked = static_cast<const LibreSslOcspRevokedInfo*>(status->value);
        if (!revoked) throw std::invalid_argument(std::string(__func__) + ": null revocation information");
        if (revocationTime) *revocationTime = revoked->revocationTime;
        if (reason) *reason = revoked->revocationReason ? static_cast<int>(EnumeratedValue(revoked->revocationReason)) : -1;
    }
    if (thisUpdate) *thisUpdate = single->thisUpdate;
    if (nextUpdate) *nextUpdate = single->nextUpdate;
    return status->type;
}

}
