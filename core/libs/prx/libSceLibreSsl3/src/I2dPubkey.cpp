#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ErrLibEc = 16;
constexpr int ErrLibX509 = 11;
constexpr int ErrLibAsn1 = 13;
constexpr int AnyFunction = 0xfff;

constexpr int ErrRMallocFailure = 65;
constexpr int ErrREcLib = 16;
constexpr int ErrRPassedNullParameter = 67;
constexpr int Asn1RTooLong = 155;
constexpr int EcRInvalidForm = 104;
constexpr int EcRMissingParameters = 124;
constexpr int X509RUnsupportedAlgorithm = 111;
constexpr int X509RPublicKeyEncodeError = 126;
constexpr int X509RMethodNotSupported = 124;

constexpr int RsaType = 6;
constexpr int HmacType = 855;
constexpr int EcType = 408;
constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;

constexpr std::array<unsigned char, 9> RsaOid{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
constexpr std::array<unsigned char, 7> EcPublicKeyOid{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
constexpr std::array<unsigned char, 8> P256Oid{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
constexpr std::array<unsigned char, 5> P384Oid{0x2b, 0x81, 0x04, 0x00, 0x22};

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason);
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 128) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> bytes{};
    std::size_t count = 0;
    while (length) {
        bytes[bytes.size() - ++count] = static_cast<unsigned char>(length);
        length >>= 8;
    }
    output.push_back(static_cast<unsigned char>(0x80 | count));
    output.insert(output.end(), bytes.end() - static_cast<std::ptrdiff_t>(count), bytes.end());
}

std::vector<unsigned char> Wrap(unsigned char tag, std::span<const unsigned char> content) {
    std::vector<unsigned char> output;
    output.reserve(content.size() + 1 + sizeof(std::size_t) + 1);
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
    return output;
}

void Append(std::vector<unsigned char>& output, const std::vector<unsigned char>& value) {
    output.insert(output.end(), value.begin(), value.end());
}

std::vector<unsigned char> EncodeInteger(std::span<const unsigned char> value) {
    while (value.size() > 1 && value.front() == 0) value = value.subspan(1);
    std::vector<unsigned char> content(value.begin(), value.end());
    if (content.empty()) content.push_back(0);
    if (content.front() & 0x80) content.insert(content.begin(), 0);
    return Wrap(0x02, content);
}

bool EncodeBn(const void* value, std::vector<unsigned char>& bytes) {
    if (!value) return false;
    const auto* bn = static_cast<const Bignum*>(value);
    const int length = BN_num_bits_nid_postfix(bn);
    if (length < 0) return false;
    bytes.resize(static_cast<std::size_t>((length + 7) / 8));
    if (!bytes.empty() && BN_bn2bin_nid_postfix(bn, bytes.data()) != static_cast<int>(bytes.size())) return false;
    return true;
}

bool EncodeRsa(const LibreSsl3Rsa* rsa, std::vector<unsigned char>& output) {
    if (!rsa) return false;
    const void* n = nullptr;
    const void* e = nullptr;
    const void* d = nullptr;
    RSA_get0_key_nid_postfix(rsa, &n, &e, &d);
    std::vector<unsigned char> key;
    const std::array<const void*, 2> components{n, e};
    for (const void* component : components) {
        std::vector<unsigned char> bytes;
        if (!EncodeBn(component, bytes)) return false;
        Append(key, EncodeInteger(bytes));
    }
    std::vector<unsigned char> algorithm;
    Append(algorithm, Wrap(0x06, RsaOid));
    Append(algorithm, Wrap(0x05, {}));

    std::vector<unsigned char> bits;
    bits.push_back(0);
    Append(bits, Wrap(0x30, key));

    std::vector<unsigned char> body;
    Append(body, Wrap(0x30, algorithm));
    Append(body, Wrap(0x03, bits));
    output = Wrap(0x30, body);
    return true;
}

bool EncodeEc(const EcKey* ec, std::vector<unsigned char>& output) {
    if (!ec || !ec->group) {
        PutError(ErrLibEc, EcRMissingParameters);
        PutError(ErrLibEc, ErrREcLib);
        return false;
    }
    const int curve = ec->group->curve.nid;
    if (curve != Secp256r1 && curve != Secp384r1)
        NotImplemented_nid_no_patch("i2d_PUBKEY of an EC key on a curve other than secp256r1 or secp384r1");
    if (!ec->publicKey) {
        PutError(ErrLibEc, ErrRPassedNullParameter);
        return false;
    }
    const std::size_t length = EC_POINT_point2oct_nid_postfix(
        ec->group, ec->publicKey, ec->conversionForm, nullptr, 0, nullptr);
    if (length == 0) {
        PutError(ErrLibEc, EcRInvalidForm);
        return false;
    }
    std::vector<unsigned char> point(1 + length);
    point[0] = 0;
    if (EC_POINT_point2oct_nid_postfix(
            ec->group, ec->publicKey, ec->conversionForm, point.data() + 1, length, nullptr) != length) {
        PutError(ErrLibEc, ErrREcLib);
        return false;
    }
    const std::span<const unsigned char> curveOid =
        curve == Secp256r1 ? std::span<const unsigned char>(P256Oid) : std::span<const unsigned char>(P384Oid);
    std::vector<unsigned char> algorithm;
    Append(algorithm, Wrap(0x06, EcPublicKeyOid));
    Append(algorithm, Wrap(0x06, curveOid));

    std::vector<unsigned char> body;
    Append(body, Wrap(0x30, algorithm));
    Append(body, Wrap(0x03, point));
    output = Wrap(0x30, body);
    return true;
}

bool EncodeSpki(const LibreSsl3EvpPkey* key, std::vector<unsigned char>& output) {
    if (key->type == RsaType) return EncodeRsa(static_cast<const LibreSsl3Rsa*>(key->key), output);
    if (key->type == EcType) return EncodeEc(static_cast<const EcKey*>(key->key), output);
    if (key->type == HmacType) {
        PutError(ErrLibX509, X509RMethodNotSupported);
        return false;
    }
    NotImplemented_nid_no_patch("i2d_PUBKEY of a key that is neither RSA nor EC");
}

}

extern "C" {

int APS5_VABI i2d_PUBKEY_nid_postfix(LibreSsl3EvpPkey* key, unsigned char** output) {
    if (!key) return 0;
    if (!key->method && key->type != EcType) {
        PutError(ErrLibX509, X509RUnsupportedAlgorithm);
        return -1;
    }
    std::vector<unsigned char> encoded;
    if (!EncodeSpki(key, encoded)) {
        PutError(ErrLibX509, X509RPublicKeyEncodeError);
        return -1;
    }
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        PutError(ErrLibAsn1, Asn1RTooLong);
        return -1;
    }
    const int length = static_cast<int>(encoded.size());
    if (!output) return length;
    unsigned char* target = nullptr;
    if (!GuestCopyIn_nid_no_patch(&target, output, sizeof(target)))
        throw std::invalid_argument("i2d_PUBKEY: unreadable output pointer");
    unsigned char* next = nullptr;
    unsigned char* allocated = nullptr;
    if (target) {
        if (!GuestCopyOut_nid_no_patch(target, encoded.data(), encoded.size()))
            throw std::invalid_argument("i2d_PUBKEY: unwritable output buffer");
        next = target + encoded.size();
    } else {
        allocated = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
        if (!allocated) {
            PutError(ErrLibX509, ErrRMallocFailure);
            return -1;
        }
        std::memcpy(allocated, encoded.data(), encoded.size());
        next = allocated;
    }
    if (!GuestCopyOut_nid_no_patch(output, &next, sizeof(next))) {
        if (allocated) free_nid_postfix(allocated);
        throw std::invalid_argument("i2d_PUBKEY: unwritable output pointer");
    }
    return length;
}

}
