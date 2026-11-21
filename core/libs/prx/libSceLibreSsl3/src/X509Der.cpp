#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int TypeOther = -3;
constexpr int TypeBoolean = 1;
constexpr int TypeBitString = 3;
constexpr int TypeOctetString = 4;
constexpr int TypeNull = 5;
constexpr int TypeObject = 6;
constexpr int TypeUtf8String = 12;
constexpr int TypeSequence = 16;
constexpr int TypeSet = 17;
constexpr int TypeNumericString = 18;
constexpr int TypeGeneralString = 27;
constexpr int TypeUniversalString = 28;
constexpr int TypeBmpString = 30;
constexpr unsigned char Constructed = 0x20;
constexpr std::int64_t BitsLeftFlag = 0x08;

struct CertificateFields {
    const LibreSsl3CertificateInfo* certInfo;
    const LibreSsl3X509Algor* sigAlg;
    const LibreSsl3Asn1String* signature;
};

struct Encoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

static_assert(sizeof(Encoding) == sizeof(LibreSsl3CertificateInfo::encoding));
static_assert(offsetof(LibreSsl3CertificateInfo, encoding) == 80);
static_assert(offsetof(LibreSsl3Certificate, sigAlg) == 8);
static_assert(sizeof(LibreSslAsn1Object) == 40);
static_assert(offsetof(LibreSslAsn1Object, data) == 24);

template <typename T>
T CopyIn(const T* source, int line) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch("i2d_X509", __FILE__, line);
    return value;
}

void AppendGuestBytes(std::vector<unsigned char>& output, const unsigned char* source, std::int64_t length, const char* what) {
    if (length < 0) throw std::invalid_argument(std::string("i2d_X509: negative ") + what + " length");
    if (length == 0) return;
    if (!source) throw std::invalid_argument(std::string("i2d_X509: null ") + what + " data");
    const auto size = static_cast<std::size_t>(length);
    const auto offset = output.size();
    output.resize(offset + size);
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(output.data() + offset, source, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch("i2d_X509", __FILE__, __LINE__);
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(length)];
    int count = 0;
    for (std::size_t rest = length; rest != 0; rest >>= 8) bytes[count++] = static_cast<unsigned char>(rest);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count > 0) output.push_back(bytes[--count]);
}

void AppendElement(std::vector<unsigned char>& output, int tag, bool constructed, const std::vector<unsigned char>& content) {
    output.push_back(static_cast<unsigned char>(tag | (constructed ? Constructed : 0)));
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

void AppendObject(std::vector<unsigned char>& output, const void* guestObject) {
    if (!guestObject) return;
    const auto object = CopyIn(static_cast<const LibreSslAsn1Object*>(guestObject), __LINE__);
    std::vector<unsigned char> content;
    AppendGuestBytes(content, object.data, object.length, "object");
    AppendElement(output, TypeObject, false, content);
}

std::vector<unsigned char> BitStringContent(const LibreSsl3Asn1String& bits) {
    if (bits.length < 0) throw std::invalid_argument("i2d_X509: negative signature length");
    if (bits.length == std::numeric_limits<int>::max()) return {};
    std::vector<unsigned char> data;
    AppendGuestBytes(data, bits.data, bits.length, "signature");
    std::size_t used = data.size();
    int unused = 0;
    if ((bits.flags & BitsLeftFlag) != 0) {
        unused = static_cast<int>(bits.flags & 0x07);
    } else {
        while (used > 0 && data[used - 1] == 0) --used;
        if (used > 0) {
            const unsigned char last = data[used - 1];
            while (((last >> unused) & 1) == 0) ++unused;
        }
    }
    std::vector<unsigned char> content;
    content.reserve(data.size() + 1);
    content.push_back(static_cast<unsigned char>(unused));
    content.insert(content.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(used));
    if (used > 0) content.back() &= static_cast<unsigned char>(0xff << unused);
    content.resize(data.size() + 1, 0);
    return content;
}

void AppendBitString(std::vector<unsigned char>& output, const LibreSsl3Asn1String* guestBits) {
    if (!guestBits) return;
    AppendElement(output, TypeBitString, false, BitStringContent(CopyIn(guestBits, __LINE__)));
}

bool IsStringType(int type) {
    return type == TypeOctetString || type == TypeUtf8String || (type >= TypeNumericString && type <= TypeUniversalString) || type == TypeBmpString;
}

void AppendAny(std::vector<unsigned char>& output, const LibreSsl3Asn1Type* guestParameter) {
    if (!guestParameter) return;
    const auto parameter = CopyIn(guestParameter, __LINE__);
    switch (parameter.type) {
    case TypeNull:
        AppendElement(output, TypeNull, false, {});
        return;
    case TypeObject:
        if (!parameter.value) throw std::invalid_argument("i2d_X509: null object parameter");
        AppendObject(output, parameter.value);
        return;
    case TypeBoolean: {
        const auto value = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(parameter.value));
        if (value == -1) return;
        AppendElement(output, TypeBoolean, false, {static_cast<unsigned char>(value)});
        return;
    }
    case TypeBitString:
        if (!parameter.value) throw std::invalid_argument("i2d_X509: null bit string parameter");
        AppendBitString(output, static_cast<const LibreSsl3Asn1String*>(parameter.value));
        return;
    default:
        break;
    }
    if (parameter.type != TypeSequence && parameter.type != TypeSet && parameter.type != TypeOther && !IsStringType(parameter.type))
        NotImplementedAt_nid_no_patch(("i2d_X509: algorithm parameter of ASN.1 type " + std::to_string(parameter.type)).c_str(), __FILE__, __LINE__);
    if (!parameter.value) throw std::invalid_argument("i2d_X509: null string parameter");
    const auto string = CopyIn(static_cast<const LibreSsl3Asn1String*>(parameter.value), __LINE__);
    std::vector<unsigned char> content;
    AppendGuestBytes(content, string.data, string.length, "parameter");
    if (parameter.type == TypeSequence || parameter.type == TypeSet || parameter.type == TypeOther)
        output.insert(output.end(), content.begin(), content.end());
    else
        AppendElement(output, parameter.type, false, content);
}

void AppendAlgorithm(std::vector<unsigned char>& output, const LibreSsl3X509Algor* guestAlgorithm) {
    if (!guestAlgorithm) return;
    const auto algorithm = CopyIn(guestAlgorithm, __LINE__);
    std::vector<unsigned char> content;
    AppendObject(content, algorithm.algorithm);
    AppendAny(content, algorithm.parameter);
    AppendElement(output, TypeSequence, true, content);
}

void AppendCertificateInfo(std::vector<unsigned char>& output, const LibreSsl3CertificateInfo* guestInfo) {
    if (!guestInfo) return;
    const auto encoding = CopyIn(reinterpret_cast<const Encoding*>(guestInfo->encoding), __LINE__);
    if (encoding.modified != 0)
        NotImplementedAt_nid_no_patch("i2d_X509: re-encoding a modified certificate info", __FILE__, __LINE__);
    AppendGuestBytes(output, encoding.bytes, encoding.length, "cached certificate info");
}

std::vector<unsigned char> EncodeCertificate(const LibreSsl3Certificate* guestCertificate) {
    const auto certificate = CopyIn(reinterpret_cast<const CertificateFields*>(guestCertificate), __LINE__);
    std::vector<unsigned char> content;
    AppendCertificateInfo(content, certificate.certInfo);
    AppendAlgorithm(content, certificate.sigAlg);
    AppendBitString(content, certificate.signature);
    std::vector<unsigned char> encoded;
    AppendElement(encoded, TypeSequence, true, content);
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::overflow_error("i2d_X509: encoding exceeds INT_MAX bytes");
    return encoded;
}

}

extern "C" int APS5_VABI i2d_X509_nid_postfix(const LibreSsl3Certificate* certificate, unsigned char** output) {
    if (!certificate) return 0;
    const auto encoded = EncodeCertificate(certificate);
    const auto length = static_cast<int>(encoded.size());
    if (!output) return length;
    unsigned char* target = CopyIn(output, __LINE__);
    unsigned char* allocated = nullptr;
    if (!target) {
        allocated = static_cast<unsigned char*>(calloc_nid_postfix(1, encoded.size()));
        if (!allocated) return -1;
        std::memcpy(allocated, encoded.data(), encoded.size());
    } else {
        const int savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(target, encoded.data(), encoded.size());
        errno = savedErrno;
        if (!written) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    unsigned char* const next = allocated ? allocated : target + encoded.size();
    const int savedErrno = errno;
    const bool stored = GuestCopyOut_nid_no_patch(output, &next, sizeof(next));
    errno = savedErrno;
    if (!stored) {
        if (allocated) free_nid_postfix(allocated);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return length;
}
