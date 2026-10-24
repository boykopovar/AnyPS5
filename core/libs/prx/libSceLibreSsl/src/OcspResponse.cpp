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

static_assert(sizeof(LibreSslAsn1String) == 24);
static_assert(offsetof(LibreSslAsn1String, type) == 4);
static_assert(offsetof(LibreSslAsn1String, data) == 8);
static_assert(sizeof(LibreSslOcspResponse) == 16);

namespace {

constexpr int Asn1Enumerated = 10;
constexpr int Asn1NegativeEnumerated = 0x100 | Asn1Enumerated;

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

}
