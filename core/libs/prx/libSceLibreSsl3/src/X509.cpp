#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int TooLarge = 223;
constexpr int TooSmall = 224;
constexpr int WrongIntegerType = 225;
constexpr std::int32_t IntegerType = 2;
constexpr std::int32_t NegativeIntegerType = 0x102;

std::int64_t Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
    return -1;
}

std::int64_t IntegerValue(const Asn1String* integer) {
    if (!integer) return 0;
    if (integer->length < 0) return -1;
    if (integer->type != IntegerType && integer->type != NegativeIntegerType) return Asn1Error(WrongIntegerType);
    if (integer->length > 0 && !integer->data)
        throw std::invalid_argument("X509_get_version requires integer data");
    std::uint64_t magnitude = 0;
    for (std::int32_t index = 0; index < integer->length; ++index) {
        if (magnitude > std::numeric_limits<std::uint64_t>::max() >> 8) return Asn1Error(TooLarge);
        magnitude = (magnitude << 8) | integer->data[index];
    }
    const auto largest = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (integer->type == NegativeIntegerType) {
        if (magnitude > largest + 1) return Asn1Error(TooSmall);
        return static_cast<std::int64_t>(0 - magnitude);
    }
    if (magnitude > largest) return Asn1Error(TooLarge);
    return static_cast<std::int64_t>(magnitude);
}

} // namespace

extern "C" {

int APS5_VABI X509_up_ref_nid_postfix(LibreSsl3X509* certificate) {
    if (!certificate)
        throw std::invalid_argument("X509_up_ref requires a certificate");
    return Retain_nid_no_patch(certificate->references);
}

std::int64_t APS5_VABI X509_get_version_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate || !certificate->certInfo)
        throw std::invalid_argument("X509_get_version requires certificate info");
    return IntegerValue(certificate->certInfo->version);
}

}
