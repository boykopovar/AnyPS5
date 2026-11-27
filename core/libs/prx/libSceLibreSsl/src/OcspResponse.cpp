#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslOcspResponse) == 16);
static_assert(sizeof(LibreSslOcspRevokedInfo) == 16);
static_assert(offsetof(LibreSslOcspRevokedInfo, revocationTime) == 0);
static_assert(offsetof(LibreSslOcspRevokedInfo, revocationReason) == 8);
static_assert(sizeof(LibreSslOcspCertStatus) == 16);
static_assert(offsetof(LibreSslOcspCertStatus, type) == 0);
static_assert(offsetof(LibreSslOcspCertStatus, value) == 8);
static_assert(sizeof(LibreSslOcspSingleResponse) == 40);
static_assert(offsetof(LibreSslOcspSingleResponse, certStatus) == 8);
static_assert(offsetof(LibreSslOcspSingleResponse, thisUpdate) == 16);
static_assert(offsetof(LibreSslOcspSingleResponse, nextUpdate) == 24);

namespace {

constexpr int Asn1Enumerated = 10;
constexpr int Asn1NegativeEnumerated = 0x100 | Asn1Enumerated;
constexpr int CertStatusRevoked = 1;

bool CopyGuestBytesInPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    return CopyGuestBytesInPreservingErrno(&destination, source, sizeof(T));
}

template <typename TField, typename TStruct>
bool CopyGuestFieldInPreservingErrno(TField& destination, const TStruct* source, std::size_t offset) {
    return CopyGuestBytesInPreservingErrno(
        &destination, reinterpret_cast<const std::byte*>(source) + offset, sizeof(TField));
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

std::int64_t EnumeratedValue(const Asn1String* value, const char* function) {
    if (!value) return 0;
    Asn1String guestValue{};
    if (!CopyGuestInPreservingErrno(guestValue, value))
        NotImplemented_nid_no_patch(function);
    const bool negative = guestValue.type == Asn1NegativeEnumerated;
    if (!negative && guestValue.type != Asn1Enumerated) return -1;
    if (guestValue.length > static_cast<int>(sizeof(std::int64_t))) return -1;
    if (!guestValue.data || guestValue.length <= 0) return 0;
    unsigned char bytes[sizeof(std::int64_t)]{};
    if (!CopyGuestBytesInPreservingErrno(bytes, guestValue.data, static_cast<std::size_t>(guestValue.length)))
        NotImplemented_nid_no_patch(function);
    std::uint64_t result = 0;
    for (int index = 0; index < guestValue.length; ++index) result = (result << 8) | bytes[index];
    return static_cast<std::int64_t>(negative ? 0 - result : result);
}

}

extern "C" {

int APS5_VABI OCSP_response_status_nid_postfix(const LibreSslOcspResponse* response) {
    if (!response) throw std::invalid_argument(std::string(__func__) + ": null response");
    LibreSslOcspResponse guestResponse{};
    if (!CopyGuestInPreservingErrno(guestResponse, response))
        NotImplemented_nid_no_patch(__func__);
    return static_cast<int>(EnumeratedValue(guestResponse.responseStatus, __func__));
}

int APS5_VABI OCSP_single_get0_status_nid_postfix(const LibreSslOcspSingleResponse* single, int* reason, Asn1String** revocationTime,
    Asn1String** thisUpdate, Asn1String** nextUpdate) {
    if (!single) return -1;
    decltype(LibreSslOcspSingleResponse::certStatus) certStatus = nullptr;
    if (!CopyGuestFieldInPreservingErrno(certStatus, single, offsetof(LibreSslOcspSingleResponse, certStatus)))
        NotImplemented_nid_no_patch(__func__);
    if (!certStatus) throw std::invalid_argument(std::string(__func__) + ": null certificate status");
    decltype(LibreSslOcspCertStatus::type) statusType = 0;
    if (!CopyGuestFieldInPreservingErrno(statusType, certStatus, offsetof(LibreSslOcspCertStatus, type)))
        NotImplemented_nid_no_patch(__func__);
    const bool writeRevoked = (statusType == CertStatusRevoked);
    Asn1String* revocationTimeValue = nullptr;
    int reasonValue = -1;
    if (writeRevoked && (revocationTime || reason)) {
        decltype(LibreSslOcspCertStatus::value) statusValue = nullptr;
        if (!CopyGuestFieldInPreservingErrno(statusValue, certStatus, offsetof(LibreSslOcspCertStatus, value)))
            NotImplemented_nid_no_patch(__func__);
        const auto* const revoked = static_cast<const LibreSslOcspRevokedInfo*>(statusValue);
        if (!revoked) throw std::invalid_argument(std::string(__func__) + ": null revocation information");
        if (revocationTime &&
            !CopyGuestFieldInPreservingErrno(revocationTimeValue, revoked, offsetof(LibreSslOcspRevokedInfo, revocationTime)))
            NotImplemented_nid_no_patch(__func__);
        if (reason) {
            Asn1String* revocationReason = nullptr;
            if (!CopyGuestFieldInPreservingErrno(revocationReason, revoked, offsetof(LibreSslOcspRevokedInfo, revocationReason)))
                NotImplemented_nid_no_patch(__func__);
            reasonValue = revocationReason ? static_cast<int>(EnumeratedValue(revocationReason, __func__)) : -1;
        }
    }
    Asn1String* thisUpdateValue = nullptr;
    if (thisUpdate && !CopyGuestFieldInPreservingErrno(thisUpdateValue, single, offsetof(LibreSslOcspSingleResponse, thisUpdate)))
        NotImplemented_nid_no_patch(__func__);
    Asn1String* nextUpdateValue = nullptr;
    if (nextUpdate && !CopyGuestFieldInPreservingErrno(nextUpdateValue, single, offsetof(LibreSslOcspSingleResponse, nextUpdate)))
        NotImplemented_nid_no_patch(__func__);
    Asn1String* previousRevocationTime = nullptr;
    const bool hadRevocationTime = writeRevoked && revocationTime && CopyGuestInPreservingErrno(previousRevocationTime, revocationTime);
    if (writeRevoked && revocationTime && !CopyGuestOutPreservingErrno(revocationTime, revocationTimeValue))
        NotImplemented_nid_no_patch(__func__);
    int previousReason = 0;
    const bool hadReason = writeRevoked && reason && CopyGuestInPreservingErrno(previousReason, reason);
    if (writeRevoked && reason && !CopyGuestOutPreservingErrno(reason, reasonValue)) {
        if (hadRevocationTime) CopyGuestOutPreservingErrno(revocationTime, previousRevocationTime);
        NotImplemented_nid_no_patch(__func__);
    }
    Asn1String* previousThisUpdate = nullptr;
    const bool hadThisUpdate = thisUpdate && CopyGuestInPreservingErrno(previousThisUpdate, thisUpdate);
    if (thisUpdate && !CopyGuestOutPreservingErrno(thisUpdate, thisUpdateValue)) {
        if (hadReason) CopyGuestOutPreservingErrno(reason, previousReason);
        if (hadRevocationTime) CopyGuestOutPreservingErrno(revocationTime, previousRevocationTime);
        NotImplemented_nid_no_patch(__func__);
    }
    if (nextUpdate && !CopyGuestOutPreservingErrno(nextUpdate, nextUpdateValue)) {
        if (hadThisUpdate) CopyGuestOutPreservingErrno(thisUpdate, previousThisUpdate);
        if (hadReason) CopyGuestOutPreservingErrno(reason, previousReason);
        if (hadRevocationTime) CopyGuestOutPreservingErrno(revocationTime, previousRevocationTime);
        NotImplemented_nid_no_patch(__func__);
    }
    return statusType;
}

}
