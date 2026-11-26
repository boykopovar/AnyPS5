#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr unsigned char IntegerType = 2;
constexpr unsigned char BitStringType = 3;
constexpr unsigned char OctetStringType = 4;
constexpr unsigned char NullType = 5;
constexpr unsigned char ObjectType = 6;
constexpr unsigned char BooleanType = 1;

void AppendLength(std::vector<unsigned char>& out, std::size_t length) {
    if (length < 128) {
        out.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)]{};
    std::size_t count = 0;
    while (length != 0) {
        bytes[sizeof(bytes) - ++count] = static_cast<unsigned char>(length);
        length >>= 8;
    }
    out.push_back(static_cast<unsigned char>(0x80 | count));
    out.insert(out.end(), bytes + sizeof(bytes) - count, bytes + sizeof(bytes));
}

void Append(std::vector<unsigned char>& out, unsigned char tag, const unsigned char* data, std::size_t length) {
    out.push_back(tag);
    AppendLength(out, length);
    out.insert(out.end(), data, data + length);
}

void AppendObject(std::vector<unsigned char>& out, const LibreSslAsn1Object* object) {
    if (!object || !object->data || object->length <= 0)
        NotImplemented_nid_no_patch("i2d_X509_PUBKEY: an algorithm without an encoded object");
    Append(out, ObjectType, object->data, static_cast<std::size_t>(object->length));
}

void AppendType(std::vector<unsigned char>& out, const LibreSslAsn1TypeValue* value) {
    if (!value) return;
    switch (value->type) {
    case NullType:
        Append(out, NullType, nullptr, 0);
        return;
    case ObjectType:
        AppendObject(out, static_cast<const LibreSslAsn1Object*>(value->value.pointer));
        return;
    case BooleanType:
        out.push_back(BooleanType);
        out.push_back(1);
        out.push_back(value->value.boolean ? 0xff : 0);
        return;
    case IntegerType:
    case BitStringType:
    case OctetStringType: {
        const auto* string = static_cast<const Asn1String*>(value->value.pointer);
        if (!string || string->length < 0) NotImplemented_nid_no_patch("i2d_X509_PUBKEY: malformed parameter");
        if (value->type == BitStringType) {
            std::vector<unsigned char> content(1, 0);
            content.insert(content.end(), string->data, string->data + string->length);
            Append(out, BitStringType, content.data(), content.size());
        } else {
            Append(out, static_cast<unsigned char>(value->type), string->data, static_cast<std::size_t>(string->length));
        }
        return;
    }
    default:
        NotImplemented_nid_no_patch("i2d_X509_PUBKEY: an unsupported algorithm parameter");
    }
}

}

extern "C" {

int APS5_VABI i2d_X509_PUBKEY_nid_postfix(const LibreSslX509Pubkey* key, unsigned char** output) {
    if (!key) return 0;
    if (!key->algor) throw std::invalid_argument("i2d_X509_PUBKEY: null algorithm");
    if (!key->publicKey || key->publicKey->length < 0) throw std::invalid_argument("i2d_X509_PUBKEY: malformed public key");
    std::vector<unsigned char> algorithm;
    AppendObject(algorithm, static_cast<const LibreSslAsn1Object*>(key->algor->algorithm));
    AppendType(algorithm, static_cast<const LibreSslAsn1TypeValue*>(key->algor->parameter));
    std::vector<unsigned char> spki;
    std::vector<unsigned char> algorithmSequence{0x30};
    AppendLength(algorithmSequence, algorithm.size());
    algorithmSequence.insert(algorithmSequence.end(), algorithm.begin(), algorithm.end());
    spki.insert(spki.end(), algorithmSequence.begin(), algorithmSequence.end());
    std::vector<unsigned char> bits(1, 0);
    bits.insert(bits.end(), key->publicKey->data, key->publicKey->data + key->publicKey->length);
    Append(spki, BitStringType, bits.data(), bits.size());
    std::vector<unsigned char> encoded{0x30};
    AppendLength(encoded, spki.size());
    encoded.insert(encoded.end(), spki.begin(), spki.end());
    const int length = static_cast<int>(encoded.size());
    if (!output) return length;
    unsigned char* target = nullptr;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&target, output, sizeof(target));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("i2d_X509_PUBKEY: unreadable output pointer");
    unsigned char* next = nullptr;
    if (target) {
        if (!GuestCopyOut_nid_no_patch(target, encoded.data(), encoded.size()))
            throw std::invalid_argument("i2d_X509_PUBKEY: unwritable output buffer");
        next = target + encoded.size();
    } else {
        auto* allocated = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
        if (!allocated) return -1;
        std::memcpy(allocated, encoded.data(), encoded.size());
        next = allocated;
    }
    if (!GuestCopyOut_nid_no_patch(output, &next, sizeof(next)))
        throw std::invalid_argument("i2d_X509_PUBKEY: unwritable output pointer");
    return length;
}

}
