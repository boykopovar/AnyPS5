#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>


using Bytes = std::vector<unsigned char>;

struct Curve {
    int nid;
    std::size_t fieldSize;
};

static constexpr Curve Curves[] = {
    {409, 24}, {415, 32}, {711, 24}, {712, 28}, {713, 28}, {714, 32},
    {715, 48}, {716, 66}, {928, 32}, {932, 48}, {934, 64},
};

static constexpr int Compressed = 2;
static constexpr int Uncompressed = 4;
static constexpr int Hybrid = 6;

static constexpr unsigned char P256Generator[] = {
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40,
    0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2,
    0x96, 0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e,
    0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51,
    0xf5,
};

static constexpr unsigned char P256NegatedGenerator[] = {
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40,
    0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2,
    0x96, 0xb0, 0x1c, 0xbd, 0x1c, 0x01, 0xe5, 0x80, 0x65, 0x71, 0x18, 0x14, 0xb5, 0x83, 0xf0, 0x61,
    0xe9, 0xd4, 0x31, 0xcc, 0xa9, 0x94, 0xce, 0xa1, 0x31, 0x34, 0x49, 0xbf, 0x97, 0xc8, 0x40, 0xae,
    0x0a,
};

static constexpr unsigned char P256Prime[] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static constexpr unsigned char P256Order[] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51,
};

static constexpr unsigned char P256RfcPublicKey[] = {
    0x04, 0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74, 0xc6, 0x35, 0x6d,
    0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c, 0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f,
    0xb6, 0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc,
    0x64, 0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51, 0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22,
    0x99,
};

static constexpr unsigned char P256RfcR[] = {
    0xef, 0xd4, 0x8b, 0x2a, 0xac, 0xb6, 0xa8, 0xfd, 0x11, 0x40, 0xdd, 0x9c, 0xd4, 0x5e, 0x81, 0xd6,
    0x9d, 0x2c, 0x87, 0x7b, 0x56, 0xaa, 0xf9, 0x91, 0xc3, 0x4d, 0x0e, 0xa8, 0x4e, 0xaf, 0x37, 0x16,
};

static constexpr unsigned char P256RfcS[] = {
    0xf7, 0xcb, 0x1c, 0x94, 0x2d, 0x65, 0x7c, 0x41, 0xd4, 0x36, 0xc7, 0xa1, 0xb6, 0xe2, 0x9f, 0x65,
    0xf3, 0xe9, 0x00, 0xdb, 0xb9, 0xaf, 0xf4, 0x06, 0x4d, 0xc4, 0xab, 0x2f, 0x84, 0x3a, 0xcd, 0xa8,
};

static constexpr unsigned char SampleDigest[] = {
    0xaf, 0x2b, 0xdb, 0xe1, 0xaa, 0x9b, 0x6e, 0xc1, 0xe2, 0xad, 0xe1, 0xd6, 0x94, 0xf4, 0x1f, 0xc7,
    0x1a, 0x83, 0x1d, 0x02, 0x68, 0xe9, 0x89, 0x15, 0x62, 0x11, 0x3d, 0x8a, 0x62, 0xad, 0xd1, 0xbf,
};

static constexpr unsigned char Secp224k1PublicKey[] = {
    0x04, 0xc4, 0x6b, 0xb3, 0x6c, 0x27, 0x1d, 0x43, 0x66, 0x6b, 0x34, 0xb1, 0xe9, 0x72, 0xdf, 0x22,
    0x1e, 0xe8, 0x95, 0x29, 0x92, 0xb5, 0xcb, 0xaf, 0x1c, 0xa9, 0xce, 0x0a, 0xce, 0xf6, 0x9c, 0xef,
    0x1f, 0x97, 0x7a, 0x36, 0x61, 0xea, 0xd7, 0xcd, 0xa9, 0x6e, 0x8e, 0x8a, 0xdf, 0x7c, 0xda, 0x60,
    0x7e, 0x7b, 0x46, 0xc7, 0x89, 0x34, 0xfe, 0xda, 0x1e,
};

static constexpr unsigned char Secp224k1Signature[] = {
    0x30, 0x3d, 0x02, 0x1d, 0x00, 0xed, 0x55, 0x7c, 0x87, 0x16, 0x19, 0x82, 0xbb, 0x8d, 0x67, 0x0f,
    0x6e, 0x6d, 0xd3, 0x26, 0x74, 0xcc, 0xe6, 0x3b, 0x10, 0xb3, 0xa7, 0x48, 0x7e, 0xa0, 0x65, 0x1e,
    0x14, 0x02, 0x1c, 0x35, 0x8e, 0x56, 0x2a, 0x83, 0xc8, 0xf9, 0x01, 0x3f, 0x29, 0x74, 0x6c, 0x3c,
    0x97, 0xbb, 0x30, 0xd8, 0x83, 0xe3, 0x4a, 0x77, 0xc0, 0x6c, 0xf4, 0x85, 0x90, 0x73, 0x32,
};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

template<std::size_t N> static Bytes ToBytes(const unsigned char (&data)[N]) {
    return Bytes(data, data + N);
}

static Bytes Concat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& part : parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}

static Bytes DerElement(unsigned char tag, const Bytes& content) {
    Bytes out{tag};
    if (content.size() < 128) {
        out.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 256) {
        out.push_back(0x81);
        out.push_back(static_cast<unsigned char>(content.size()));
    } else {
        out.push_back(0x82);
        out.push_back(static_cast<unsigned char>(content.size() >> 8));
        out.push_back(static_cast<unsigned char>(content.size()));
    }
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

static Bytes Signature(const Bytes& r, const Bytes& s) {
    return DerElement(0x30, Concat({DerElement(0x02, r), DerElement(0x02, s)}));
}

static Bytes Encode(const EcGroup* group, const EcPoint* point, int form) {
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, form, nullptr, 0, nullptr);
    Bytes out(size);
    Require(size != 0 && EC_POINT_point2oct_nid_postfix(group, point, form, out.data(), out.size(), nullptr) == size, "EC_POINT_point2oct");
    return out;
}

static EcPoint* Decode(const EcGroup* group, const Bytes& encoded) {
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr, "EC_POINT_new");
    Require(EC_POINT_oct2point_nid_postfix(group, point, encoded.data(), encoded.size(), nullptr) == 1, "EC_POINT_oct2point");
    return point;
}

static int Verify(const unsigned char* digest, int digestLength, const Bytes& signature, EcKey* key) {
    return ECDSA_verify_nid_postfix(0, digest, digestLength, signature.data(), static_cast<int>(signature.size()), key);
}

static EcKey* PublicKeyOnly(int nid, const Bytes& encoded) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    EcPoint* point = Decode(EC_KEY_get0_group_nid_postfix(key), encoded);
    Require(EC_KEY_set_public_key_nid_postfix(key, point) == 1, "EC_KEY_set_public_key");
    EC_POINT_free_nid_postfix(point);
    return key;
}

static void CheckPointEncoding() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    unsigned char buffer[80] = {};
    Require(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, nullptr, 0, nullptr) == 1, "a new point is at infinity");
    Require(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 0, nullptr) == 0, "infinity into an empty buffer");
    buffer[0] = 0xaa;
    Require(EC_POINT_point2oct_nid_postfix(group, point, Compressed, buffer, 1, nullptr) == 1 && buffer[0] == 0, "infinity encodes to one zero octet");

    const Bytes generator = ToBytes(P256Generator);
    Require(EC_POINT_oct2point_nid_postfix(group, point, generator.data(), generator.size(), nullptr) == 1, "EC_POINT_oct2point of the generator");
    Require(Encode(group, point, Uncompressed) == generator, "uncompressed generator");
    const Bytes x(generator.begin() + 1, generator.begin() + 33);
    const Bytes y(generator.begin() + 33, generator.end());
    Require(Encode(group, point, Compressed) == Concat({{0x03}, x}), "compressed generator with an odd y");
    Require(Encode(group, point, Hybrid) == Concat({{0x07}, x, y}), "hybrid generator with an odd y");
    Require(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 64, nullptr) == 0, "a buffer one byte short");
    for (const int form : {0, 1, 3, 5, 7, 8}) {
        Require(EC_POINT_point2oct_nid_postfix(group, point, form, buffer, sizeof(buffer), nullptr) == 0, "an invalid conversion form");
    }

    EcPoint* decoded = Decode(group, Concat({{0x03}, x}));
    Require(Encode(group, decoded, Uncompressed) == generator, "decompressing with an odd y");
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, Concat({{0x02}, x}).data(), 33, nullptr) == 1, "decompressing with an even y");
    Require(Encode(group, decoded, Uncompressed) == ToBytes(P256NegatedGenerator), "the even root is the negated generator");
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, Concat({{0x07}, x, y}).data(), 65, nullptr) == 1, "hybrid encoding");
    Require(Encode(group, decoded, Uncompressed) == generator, "hybrid generator");
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, Concat({{0x06}, x, y}).data(), 65, nullptr) == 0, "hybrid encoding with the wrong y bit");

    const Bytes rejected[] = {
        {0x00, 0x00}, {0x01}, {0x05}, Concat({{0x05}, x, y}), Concat({{0x08}, x, y}), Concat({{0x04}, x}),
        Concat({{0x04}, x, y, {0x00}}), Concat({{0x03}, x, {0x00}}), Concat({{0x04}, ToBytes(P256Prime), y}),
        Concat({{0x02}, ToBytes(P256Prime)}), Concat({{0x04}, x, ToBytes(P256Prime)}),
    };
    for (const Bytes& encoded : rejected) {
        Require(EC_POINT_oct2point_nid_postfix(group, decoded, encoded.data(), encoded.size(), nullptr) == 0, "an invalid encoding");
        Require(Encode(group, decoded, Uncompressed) == generator, "an invalid encoding leaves the point");
    }
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, generator.data(), 0, nullptr) == 0, "an empty encoding");

    Bytes offCurve = generator;
    offCurve.back() ^= 1;
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, offCurve.data(), offCurve.size(), nullptr) == 0, "a point off the curve");
    Require(Encode(group, decoded, Uncompressed) == offCurve, "a point off the curve keeps its coordinates");
    Require(EC_POINT_oct2point_nid_postfix(group, decoded, Bytes{0x00}.data(), 1, nullptr) == 1, "decoding infinity");
    Require(Encode(group, decoded, Hybrid) == Bytes{0x00}, "decoded infinity");

    Require(EC_POINT_new_nid_postfix(nullptr) == nullptr, "EC_POINT_new without a group");
    EC_POINT_free_nid_postfix(nullptr);
    RequireThrows([&] { EC_POINT_point2oct_nid_postfix(nullptr, point, Uncompressed, nullptr, 0, nullptr); }, "EC_POINT_point2oct without a group");
    RequireThrows([&] { EC_POINT_point2oct_nid_postfix(group, nullptr, Uncompressed, nullptr, 0, nullptr); }, "EC_POINT_point2oct without a point");
    RequireThrows([&] { EC_POINT_oct2point_nid_postfix(group, nullptr, generator.data(), generator.size(), nullptr); }, "EC_POINT_oct2point without a point");
    RequireThrows([&] { EC_POINT_oct2point_nid_postfix(group, point, nullptr, 1, nullptr); }, "EC_POINT_oct2point without a buffer");
    EC_POINT_free_nid_postfix(decoded);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
}

static void CheckGeneratedKeys() {
    unsigned char digest[64];
    for (std::size_t i = 0; i < sizeof(digest); ++i) digest[i] = static_cast<unsigned char>(i * 37 + 11);
    for (const Curve& curve : Curves) {
        EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(curve.nid);
        EcKey* peer = EC_KEY_new_by_curve_name_nid_postfix(curve.nid);
        const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
        Require(EC_KEY_get0_public_key_nid_postfix(key) == nullptr, "a new key has no public key");
        Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key without a public key");
        Require(EC_KEY_generate_key_nid_postfix(key) == 1 && EC_KEY_generate_key_nid_postfix(peer) == 1, "EC_KEY_generate_key");
        Require(EC_KEY_check_key_nid_postfix(key) == 1, "EC_KEY_check_key of a generated key");
        const EcPoint* publicKey = EC_KEY_get0_public_key_nid_postfix(key);
        const Bytes encoded = Encode(group, publicKey, Uncompressed);
        Require(encoded.size() == 1 + 2 * curve.fieldSize, "public key size");
        Require(Encode(group, publicKey, Compressed).size() == 1 + curve.fieldSize, "compressed public key size");

        unsigned char signature[160];
        unsigned int signatureLength = 0;
        Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &signatureLength, key) == 1, "ECDSA_sign");
        Require(signatureLength > 0 && signatureLength <= static_cast<unsigned int>(ECDSA_size_nid_postfix(key)), "signature within ECDSA_size");
        const Bytes signed64(signature, signature + signatureLength);
        Require(Verify(digest, sizeof(digest), signed64, key) == 1, "ECDSA_verify of a signature");
        Require(Verify(digest, sizeof(digest), signed64, peer) == 0, "ECDSA_verify with another key");
        Bytes wrong = signed64;
        wrong.back() ^= 1;
        Require(Verify(digest, sizeof(digest), wrong, key) == 0, "ECDSA_verify of a changed signature");
        digest[0] ^= 1;
        Require(Verify(digest, sizeof(digest), signed64, key) == 0, "ECDSA_verify of another digest");
        digest[0] ^= 1;
        Require(ECDSA_sign_nid_postfix(0, digest, 0, signature, &signatureLength, key) == 1, "ECDSA_sign of an empty digest");
        Require(Verify(nullptr, 0, Bytes(signature, signature + signatureLength), key) == 1, "ECDSA_verify of an empty digest");

        Bytes shared(curve.fieldSize);
        Bytes peerShared(curve.fieldSize);
        Require(ECDH_compute_key_nid_postfix(shared.data(), shared.size(), EC_KEY_get0_public_key_nid_postfix(peer), key, nullptr) == static_cast<int>(curve.fieldSize), "ECDH_compute_key");
        Require(ECDH_compute_key_nid_postfix(peerShared.data(), peerShared.size(), publicKey, peer, nullptr) == static_cast<int>(curve.fieldSize), "ECDH_compute_key of the peer");
        Require(shared == peerShared, "both sides agree");

        Require(EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key again");
        Require(EC_KEY_get0_public_key_nid_postfix(key) == publicKey, "generating again keeps the public key object");
        Require(Encode(group, publicKey, Uncompressed) != encoded, "generating again changes the public key");
        Require(EC_KEY_check_key_nid_postfix(key) == 1, "EC_KEY_check_key after generating again");
        Require(Verify(digest, sizeof(digest), signed64, key) == 0, "the old signature with the new key");
        EC_KEY_free_nid_postfix(peer);
        EC_KEY_free_nid_postfix(key);
    }
}

static void CheckKnownSignatures() {
    EcKey* key = PublicKeyOnly(415, ToBytes(P256RfcPublicKey));
    Require(EC_KEY_check_key_nid_postfix(key) == 1, "EC_KEY_check_key of a public key");
    const Bytes r = ToBytes(P256RfcR);
    const Bytes s = ToBytes(P256RfcS);
    const Bytes signature = Signature(Concat({{0x00}, r}), Concat({{0x00}, s}));
    Require(signature.size() == 72, "RFC 6979 signature size");
    Require(Verify(SampleDigest, sizeof(SampleDigest), signature, key) == 1, "RFC 6979 P-256 SHA-256 signature");
    unsigned char changed[sizeof(SampleDigest)];
    std::memcpy(changed, SampleDigest, sizeof(changed));
    changed[31] ^= 1;
    Require(Verify(changed, sizeof(changed), signature, key) == 0, "RFC 6979 signature of another digest");

    const Bytes invalid[] = {
        Concat({signature, {0x00}}),
        Bytes(signature.begin(), signature.end() - 1),
        Concat({{0x30, 0x81, 0x46}, Bytes(signature.begin() + 2, signature.end())}),
        Concat({{0x31}, Bytes(signature.begin() + 1, signature.end())}),
        Signature(r, Concat({{0x00}, s})),
        Signature(Concat({{0x00, 0x00}, r}), Concat({{0x00}, s})),
        Signature({}, Concat({{0x00}, s})),
        Signature({0x00, 0x01}, Concat({{0x00}, s})),
        DerElement(0x30, Concat({DerElement(0x02, Concat({{0x00}, r})), DerElement(0x02, Concat({{0x00}, s})), {0x05, 0x00}})),
        DerElement(0x30, DerElement(0x02, Concat({{0x00}, r}))),
    };
    for (const Bytes& encoded : invalid) {
        Require(Verify(SampleDigest, sizeof(SampleDigest), encoded, key) == -1, "a signature that is not DER");
    }
    Require(ECDSA_verify_nid_postfix(0, SampleDigest, sizeof(SampleDigest), signature.data(), 0, key) == -1, "an empty signature");
    Require(ECDSA_verify_nid_postfix(0, SampleDigest, sizeof(SampleDigest), signature.data(), -1, key) == -1, "a negative signature length");

    const Bytes order = Concat({{0x00}, ToBytes(P256Order)});
    const Bytes outOfRange[] = {
        Signature({0x00}, Concat({{0x00}, s})),
        Signature(Concat({{0x00}, r}), {0x00}),
        Signature(order, Concat({{0x00}, s})),
        Signature(Concat({{0x00}, r}), order),
        Signature(Concat({{0x01}, Bytes(299, 0)}), Concat({{0x00}, s})),
    };
    for (const Bytes& encoded : outOfRange) {
        Require(Verify(SampleDigest, sizeof(SampleDigest), encoded, key) == 0, "r or s out of range");
    }

    unsigned char buffer[80];
    unsigned int length = 123;
    Require(ECDSA_sign_nid_postfix(0, SampleDigest, sizeof(SampleDigest), buffer, &length, key) == 0 && length == 0, "ECDSA_sign without a private key");
    RequireThrows([&] { Verify(SampleDigest, sizeof(SampleDigest), signature, nullptr); }, "ECDSA_verify without a key");
    Require(Verify(SampleDigest, sizeof(SampleDigest), Bytes{0x30, 0x00}, nullptr) == -1, "a signature that is not DER is rejected before the key");
    EC_KEY_free_nid_postfix(key);

    EcKey* noPublicKey = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(Verify(SampleDigest, sizeof(SampleDigest), signature, noPublicKey) == -1, "ECDSA_verify without a public key");
    EC_KEY_free_nid_postfix(noPublicKey);

    EcKey* secp224k1 = PublicKeyOnly(712, ToBytes(Secp224k1PublicKey));
    const Bytes signature224 = ToBytes(Secp224k1Signature);
    Require(Verify(SampleDigest, sizeof(SampleDigest), signature224, secp224k1) == 1, "secp224k1 signature of a digest longer than the order");
    std::memcpy(changed, SampleDigest, sizeof(changed));
    changed[28] ^= 0x01;
    changed[31] ^= 0xff;
    Require(Verify(changed, sizeof(changed), signature224, secp224k1) == 1, "bits past the 225-bit order are ignored");
    changed[28] ^= 0x80;
    Require(Verify(changed, sizeof(changed), signature224, secp224k1) == 0, "the 225th bit is used");
    EC_KEY_free_nid_postfix(secp224k1);

    EcKey* signer = EC_KEY_new_by_curve_name_nid_postfix(712);
    Require(EC_KEY_generate_key_nid_postfix(signer) == 1, "secp224k1 key");
    Require(ECDSA_sign_nid_postfix(0, SampleDigest, sizeof(SampleDigest), buffer, &length, signer) == 1, "secp224k1 ECDSA_sign");
    std::memcpy(changed, SampleDigest, sizeof(changed));
    changed[28] ^= 0x7f;
    Require(Verify(changed, sizeof(changed), Bytes(buffer, buffer + length), signer) == 1, "signing ignores bits past the order");
    EC_KEY_free_nid_postfix(signer);
}

static std::size_t KdfInputLength;
static Bytes KdfInput;

static void* APS5_VABI CopyingKdf(const void* in, std::size_t inLength, void* out, std::size_t* outLength) {
    KdfInputLength = inLength;
    KdfInput.assign(static_cast<const unsigned char*>(in), static_cast<const unsigned char*>(in) + inLength);
    std::memcpy(out, in, 7);
    *outLength = 7;
    return out;
}

static void* APS5_VABI FailingKdf(const void*, std::size_t, void*, std::size_t*) {
    return nullptr;
}

static void CheckSharedSecrets() {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcKey* peer = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1 && EC_KEY_generate_key_nid_postfix(peer) == 1, "P-256 keys");
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    const EcPoint* peerKey = EC_KEY_get0_public_key_nid_postfix(peer);

    EcPoint* generator = Decode(group, ToBytes(P256Generator));
    Bytes secret(32);
    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), generator, key, nullptr) == 32, "ECDH_compute_key with the generator");
    const Bytes publicKey = Encode(group, EC_KEY_get0_public_key_nid_postfix(key), Uncompressed);
    Require(secret == Bytes(publicKey.begin() + 1, publicKey.begin() + 33), "the secret with the generator is the x of the public key");

    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), peerKey, key, nullptr) == 32, "ECDH_compute_key");
    Bytes padded(40, 0xaa);
    Require(ECDH_compute_key_nid_postfix(padded.data(), padded.size(), peerKey, key, nullptr) == 32, "ECDH_compute_key into a larger buffer");
    Require(Bytes(padded.begin(), padded.begin() + 32) == secret && Bytes(padded.begin() + 32, padded.end()) == Bytes(8, 0), "a larger buffer is zero-filled");
    Require(ECDH_compute_key_nid_postfix(padded.data(), 31, peerKey, key, nullptr) == -1, "ECDH_compute_key into a short buffer");
    Require(ECDH_compute_key_nid_postfix(nullptr, 0, peerKey, key, nullptr) == -1, "ECDH_compute_key into an empty buffer");
    Require(ECDH_compute_key_nid_postfix(padded.data(), static_cast<std::size_t>(INT_MAX) + 1, peerKey, key, nullptr) == -1, "ECDH_compute_key with a length above INT_MAX");

    unsigned char derived[16] = {};
    Require(ECDH_compute_key_nid_postfix(derived, 2, peerKey, key, CopyingKdf) == 7, "ECDH_compute_key returns the KDF length");
    Require(KdfInputLength == 32 && KdfInput == secret && Bytes(derived, derived + 7) == Bytes(secret.begin(), secret.begin() + 7), "the KDF gets the whole secret");
    Require(ECDH_compute_key_nid_postfix(derived, sizeof(derived), peerKey, key, FailingKdf) == -1, "a failing KDF");

    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), infinity, key, nullptr) == -1, "a peer key at infinity");
    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), nullptr, key, nullptr) == -1, "no peer key");
    EcKey* publicOnly = PublicKeyOnly(415, ToBytes(P256Generator));
    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), peerKey, publicOnly, nullptr) == -1, "ECDH_compute_key without a private key");
    RequireThrows([&] { ECDH_compute_key_nid_postfix(secret.data(), secret.size(), peerKey, nullptr, nullptr); }, "ECDH_compute_key without a key");
    RequireThrows([&] { ECDH_compute_key_nid_postfix(nullptr, 32, peerKey, key, nullptr); }, "ECDH_compute_key without an output buffer");

    EC_KEY_free_nid_postfix(publicOnly);
    EC_POINT_free_nid_postfix(infinity);
    EC_POINT_free_nid_postfix(generator);
    EC_KEY_free_nid_postfix(peer);
    EC_KEY_free_nid_postfix(key);
}

static void CheckKeyChanges() {
    unsigned char digest[32] = {1, 2, 3};
    unsigned char signature[80];
    unsigned int length = 0;

    EcKey* empty = EC_KEY_new_nid_postfix();
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(EC_KEY_generate_key_nid_postfix(empty) == 0, "EC_KEY_generate_key without a group");
    Require(EC_KEY_generate_key_nid_postfix(nullptr) == 0, "EC_KEY_generate_key without a key");
    Require(EC_KEY_check_key_nid_postfix(nullptr) == 0, "EC_KEY_check_key without a key");
    Require(EC_KEY_set_public_key_nid_postfix(empty, point) == 0 && EC_KEY_get0_public_key_nid_postfix(empty) == nullptr, "EC_KEY_set_public_key without a group");
    Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &length, empty) == 0, "ECDSA_sign without a group");
    RequireThrows([&] { EC_KEY_get0_public_key_nid_postfix(nullptr); }, "EC_KEY_get0_public_key without a key");
    RequireThrows([&] { EC_KEY_set_public_key_nid_postfix(nullptr, point); }, "EC_KEY_set_public_key without a key");
    RequireThrows([&] { ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &length, nullptr); }, "ECDSA_sign without a key");
    EC_KEY_free_nid_postfix(empty);

    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcKey* peer = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1 && EC_KEY_generate_key_nid_postfix(peer) == 1, "P-256 keys");
    const EcGroup* keyGroup = EC_KEY_get0_group_nid_postfix(key);
    RequireThrows([&] { ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, nullptr, key); }, "ECDSA_sign without a length");
    Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), nullptr, &length, key) == 1 && length >= 8 && length <= 72, "ECDSA_sign without a buffer returns the length");

    const Bytes own = Encode(keyGroup, EC_KEY_get0_public_key_nid_postfix(key), Uncompressed);
    Require(EC_KEY_set_public_key_nid_postfix(key, EC_KEY_get0_public_key_nid_postfix(key)) == 1, "EC_KEY_set_public_key with the key's own public key");
    Require(Encode(keyGroup, EC_KEY_get0_public_key_nid_postfix(key), Uncompressed) == own && EC_KEY_check_key_nid_postfix(key) == 1, "the key's own public key set again");

    EcPoint* copied = Decode(keyGroup, Encode(keyGroup, EC_KEY_get0_public_key_nid_postfix(peer), Compressed));
    Require(EC_KEY_set_public_key_nid_postfix(key, copied) == 1, "EC_KEY_set_public_key");
    Require(EC_KEY_get0_public_key_nid_postfix(key) != copied, "EC_KEY_set_public_key stores a copy");
    EC_POINT_free_nid_postfix(copied);
    Require(Encode(keyGroup, EC_KEY_get0_public_key_nid_postfix(key), Uncompressed) == Encode(keyGroup, EC_KEY_get0_public_key_nid_postfix(peer), Uncompressed), "the copy outlives the point");
    Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key with another key's public key");
    Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &length, key) == 1, "ECDSA_sign uses the private key");
    Require(Verify(digest, sizeof(digest), Bytes(signature, signature + length), key) == 0, "the signature does not match the replaced public key");

    Require(EC_KEY_set_public_key_nid_postfix(key, point) == 1, "EC_KEY_set_public_key with infinity");
    Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key with a public key at infinity");
    Bytes offCurve = ToBytes(P256Generator);
    offCurve.back() ^= 1;
    Require(EC_POINT_oct2point_nid_postfix(group, point, offCurve.data(), offCurve.size(), nullptr) == 0, "a point off the curve");
    Require(EC_KEY_set_public_key_nid_postfix(key, point) == 1, "EC_KEY_set_public_key with a point off the curve");
    Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key with a public key off the curve");

    Require(EC_KEY_set_public_key_nid_postfix(key, nullptr) == 0 && EC_KEY_get0_public_key_nid_postfix(key) == nullptr, "EC_KEY_set_public_key with a null point");
    Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key after dropping the public key");
    Require(EC_KEY_set_group_nid_postfix(key, group) == 1, "EC_KEY_set_group to the same curve");
    Require(EC_KEY_generate_key_nid_postfix(key) == 1 && EC_KEY_check_key_nid_postfix(key) == 1, "EC_KEY_generate_key after EC_KEY_set_group");

    Require(EC_KEY_set_group_nid_postfix(key, nullptr) == 0, "EC_KEY_set_group with a null group");
    Require(EC_KEY_generate_key_nid_postfix(key) == 0, "EC_KEY_generate_key after dropping the group");
    Require(EC_KEY_check_key_nid_postfix(key) == 0, "EC_KEY_check_key after dropping the group");
    Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &length, key) == 0 && length == 0, "ECDSA_sign after dropping the group");
    Bytes secret(32);
    Require(ECDH_compute_key_nid_postfix(secret.data(), secret.size(), EC_KEY_get0_public_key_nid_postfix(peer), key, nullptr) == -1, "ECDH_compute_key after dropping the group");
    Require(EC_KEY_set_group_nid_postfix(key, group) == 1 && EC_KEY_check_key_nid_postfix(key) == 1, "EC_KEY_set_group back to the curve of the keys");

    EC_KEY_free_nid_postfix(peer);
    EC_KEY_free_nid_postfix(key);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
}

int main() {
    CheckPointEncoding();
    CheckGeneratedKeys();
    CheckKnownSignatures();
    CheckSharedSecrets();
    CheckKeyChanges();
    return 0;
}
