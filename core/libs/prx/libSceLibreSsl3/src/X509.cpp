#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
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

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
T CopyIn(const T* source, const char* function) {
    T value{};
    if (!CopyGuestInPreservingErrno(value, source))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
int RetainGuest(T* object, const char* function) {
    std::int32_t* const references = &object->references;
    std::int32_t current{};
    if (!CopyGuestInPreservingErrno(current, references) || !GuestWritable_nid_no_patch(references, sizeof(current)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return Retain_nid_no_patch(*references);
}

} // namespace

extern "C" {

int APS5_VABI X509_up_ref_nid_postfix(LibreSsl3X509* certificate) {
    if (!certificate)
        throw std::invalid_argument("X509_up_ref requires a certificate");
    return RetainGuest(certificate, __func__);
}

std::int64_t APS5_VABI X509_get_version_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate)
        throw std::invalid_argument("X509_get_version requires certificate info");
    const auto* const info = CopyIn(certificate, __func__).certInfo;
    if (!info)
        throw std::invalid_argument("X509_get_version requires certificate info");
    return IntegerValue(CopyIn(&info->version, __func__));
}

int APS5_VABI X509_set_version_nid_postfix(LibreSsl3X509* certificate, std::int64_t version) {
    if (!certificate) return 0;
    if (version < 0 || version > 2) return 0;
    auto* const info = CopyIn(certificate, __func__).certInfo;
    if (!info)
        throw std::invalid_argument("X509_set_version requires certificate info");
    auto* integer = CopyIn(&info->version, __func__);
    if (!GuestWritable_nid_no_patch(&info->version, sizeof(info->version)) || !GuestWritable_nid_no_patch(&info->encModified, sizeof(info->encModified)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!integer) {
        integer = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
        if (!integer) return 0;
        integer->type = IntegerType;
        if (!CopyGuestOutPreservingErrno(&info->version, integer))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!CopyGuestOutPreservingErrno(&info->encModified, std::int32_t{1}))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return ASN1_INTEGER_set_nid_postfix(integer, version);
}

}
