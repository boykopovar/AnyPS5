#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <vector>

using Bytes = std::vector<unsigned char>;

struct Queued {
    int library;
    int reason;
};

constexpr int P256 = 415;
constexpr int P384 = 715;
constexpr int Compressed = 2;
constexpr int Uncompressed = 4;
constexpr int Hybrid = 6;

constexpr int LibraryEc = 16;
constexpr int LibraryEvp = 6;
constexpr int LibraryEcdsa = 42;
constexpr int LibraryEcdh = 43;
constexpr int ReasonEcLibrary = 16;
constexpr int MallocFailure = 65;
constexpr int PassedNullParameter = 67;
constexpr int EcBufferTooSmall = 100;
constexpr int EcInvalidForm = 104;
constexpr int EcPointAtInfinity = 106;
constexpr int EcPointIsNotOnCurve = 107;
constexpr int EcInvalidPrivateKey = 123;
constexpr int EcUnknownGroup = 129;
constexpr int EcdsaBadSignature = 100;
constexpr int EcdsaMissingParameters = 103;
constexpr int EcdhNoPrivateValue = 100;
constexpr int EcdhPointArithmeticFailure = 101;
constexpr int EcdhKdfFailed = 102;
constexpr int EcdhKeyTruncation = 104;
constexpr Queued Earlier{LibraryEvp, 111};

constexpr const char* Generator =
    "046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5";
constexpr const char* RfcPublicKey =
    "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb67903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299";
constexpr const char* RfcR = "efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716";
constexpr const char* RfcS = "f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8";
constexpr const char* SampleDigest = "af2bdbe1aa9b6ec1e2ade1d694f41fc71a831d0268e9891562113d8a62add1bf";
constexpr const char* Order = "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551";
constexpr const char* InfinityR = "28815b9a52dcc43d91fe0e07a91bb000f5f475306723ba5e6392aee8be90ee9c";

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EC error queue check failed at line %u\n", location.line());
    std::abort();
}

static void ExpectQueue(std::initializer_list<Queued> errors, std::source_location location = std::source_location::current()) {
    for (const Queued& error : errors) {
        const std::uint64_t code = (static_cast<std::uint64_t>(error.library) << 24) | 0xfff000u | static_cast<std::uint64_t>(error.reason);
        Require(ERR_get_error_nid_postfix() == code, location);
    }
    Require(ERR_get_error_nid_postfix() == 0, location);
}

template<typename TResult, typename TExpected> static void Expect(TResult result, TExpected expected, std::initializer_list<Queued> errors,
    std::source_location location = std::source_location::current()) {
    Require(result == static_cast<TResult>(expected), location);
    ExpectQueue(errors, location);
}

static void QueueEarlier() {
    LibreSslPutError_nid_no_patch(Earlier.library, 0xfff, Earlier.reason);
}

static Bytes Hex(const char* text) {
    const auto nibble = [](char digit) { return static_cast<unsigned char>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) bytes.push_back(static_cast<unsigned char>(nibble(text[0]) << 4 | nibble(text[1])));
    return bytes;
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

static EcPoint* Decode(const EcGroup* group, const Bytes& encoded) {
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Require(EC_POINT_oct2point_nid_postfix(group, point, encoded.data(), encoded.size(), nullptr) == 1);
    return point;
}

static EcKey* PublicKeyOnly(int nid, const Bytes& encoded) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr);
    EcPoint* point = Decode(EC_KEY_get0_group_nid_postfix(key), encoded);
    Require(EC_KEY_set_public_key_nid_postfix(key, point) == 1);
    EC_POINT_free_nid_postfix(point);
    return key;
}

static EcKey* GeneratedKey(int nid) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr);
    Expect(EC_KEY_generate_key_nid_postfix(key), 1, {});
    return key;
}

static int Verify(const Bytes& digest, const Bytes& signature, EcKey* key) {
    return ECDSA_verify_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), signature.data(), static_cast<int>(signature.size()), key);
}

static int Sign(const Bytes& digest, unsigned char* signature, unsigned int* length, EcKey* key) {
    return ECDSA_sign_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), signature, length, key);
}

static int Agree(void* out, std::size_t length, const EcPoint* peer, EcKey* key, void* (APS5_VABI* kdf)(const void*, std::size_t, void*, std::size_t*) = nullptr) {
    return ECDH_compute_key_nid_postfix(out, length, peer, key, kdf);
}

static void* APS5_VABI CopyingKdf(const void* in, std::size_t, void* out, std::size_t* outLength) {
    std::memcpy(out, in, 7);
    *outLength = 7;
    return out;
}

static void* APS5_VABI FailingKdf(const void*, std::size_t, void*, std::size_t*) {
    return nullptr;
}

static void TestCurveNames() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(P256);
    Require(group != nullptr);
    ExpectQueue({});
    EC_GROUP_free_nid_postfix(group);
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(key != nullptr);
    ExpectQueue({});
    EC_KEY_free_nid_postfix(key);
    for (const int nid : {1, 408, 9999}) {
        Expect(EC_GROUP_new_by_curve_name_nid_postfix(nid), nullptr, {{LibraryEc, EcUnknownGroup}});
        Expect(EC_KEY_new_by_curve_name_nid_postfix(nid), nullptr, {{LibraryEc, EcUnknownGroup}});
    }
    for (const int nid : {0, -1, INT_MIN}) {
        Expect(EC_GROUP_new_by_curve_name_nid_postfix(nid), nullptr, {});
        Expect(EC_KEY_new_by_curve_name_nid_postfix(nid), nullptr, {});
    }
    QueueEarlier();
    Expect(EC_GROUP_new_by_curve_name_nid_postfix(1), nullptr, {Earlier, {LibraryEc, EcUnknownGroup}});
}

static void TestPointEncoding() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(P256);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(group != nullptr && point != nullptr);
    unsigned char buffer[80] = {};
    for (const int form : {0, 1, 3, 5, 7, 8}) {
        Expect(EC_POINT_point2oct_nid_postfix(group, point, form, buffer, sizeof(buffer), nullptr), 0, {{LibraryEc, EcInvalidForm}});
        Expect(EC_POINT_point2oct_nid_postfix(group, point, form, nullptr, 0, nullptr), 0, {{LibraryEc, EcInvalidForm}});
    }
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, nullptr, 0, nullptr), 1, {});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 0, nullptr), 0, {{LibraryEc, EcBufferTooSmall}});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Compressed, buffer, 1, nullptr), 1, {});

    const Bytes generator = Hex(Generator);
    Require(EC_POINT_oct2point_nid_postfix(group, point, generator.data(), generator.size(), nullptr) == 1);
    for (const int form : {0, 1, 3, 5, 7, 8}) {
        Expect(EC_POINT_point2oct_nid_postfix(group, point, form, buffer, sizeof(buffer), nullptr), 0, {{LibraryEc, EcInvalidForm}});
        Expect(EC_POINT_point2oct_nid_postfix(group, point, form, nullptr, 0, nullptr), 0, {{LibraryEc, EcInvalidForm}});
    }
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, nullptr, 0, nullptr), 65, {});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 64, nullptr), 0, {{LibraryEc, EcBufferTooSmall}});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 65, nullptr), 65, {});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Compressed, nullptr, 0, nullptr), 33, {});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Compressed, buffer, 32, nullptr), 0, {{LibraryEc, EcBufferTooSmall}});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Compressed, buffer, 33, nullptr), 33, {});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Hybrid, buffer, 64, nullptr), 0, {{LibraryEc, EcBufferTooSmall}});
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Hybrid, buffer, 65, nullptr), 65, {});
    QueueEarlier();
    Expect(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, buffer, 0, nullptr), 0, {Earlier, {LibraryEc, EcBufferTooSmall}});

    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
}

static void TestKeys() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(P256);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(group != nullptr && point != nullptr);

    EcKey* empty = EC_KEY_new_nid_postfix();
    Require(empty != nullptr);
    ExpectQueue({});
    Expect(EC_KEY_generate_key_nid_postfix(nullptr), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_generate_key_nid_postfix(empty), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_check_key_nid_postfix(nullptr), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_check_key_nid_postfix(empty), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_set_public_key_nid_postfix(empty, point), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_set_public_key_nid_postfix(empty, nullptr), 0, {});
    EC_KEY_free_nid_postfix(empty);

    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(key != nullptr);
    Expect(EC_KEY_check_key_nid_postfix(key), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_set_public_key_nid_postfix(key, nullptr), 0, {});
    Expect(EC_KEY_generate_key_nid_postfix(key), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(key), 1, {});
    Expect(EC_KEY_generate_key_nid_postfix(key), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(key), 1, {});

    EcKey* peer = GeneratedKey(P256);
    Expect(EC_KEY_set_public_key_nid_postfix(key, EC_KEY_get0_public_key_nid_postfix(peer)), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(key), 0, {{LibraryEc, EcInvalidPrivateKey}});
    Expect(EC_KEY_set_public_key_nid_postfix(key, point), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(key), 0, {{LibraryEc, EcPointAtInfinity}});
    Bytes offCurve = Hex(Generator);
    offCurve.back() ^= 1;
    Require(EC_POINT_oct2point_nid_postfix(group, point, offCurve.data(), offCurve.size(), nullptr) == 0);
    ERR_clear_error_nid_postfix();
    Expect(EC_KEY_set_public_key_nid_postfix(key, point), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(key), 0, {{LibraryEc, EcPointIsNotOnCurve}});
    QueueEarlier();
    Expect(EC_KEY_check_key_nid_postfix(key), 0, {Earlier, {LibraryEc, EcPointIsNotOnCurve}});

    EcKey* publicOnly = PublicKeyOnly(P256, Hex(RfcPublicKey));
    Expect(EC_KEY_check_key_nid_postfix(publicOnly), 1, {});
    Expect(EC_KEY_set_public_key_nid_postfix(publicOnly, point), 1, {});
    Expect(EC_KEY_check_key_nid_postfix(publicOnly), 0, {{LibraryEc, EcPointIsNotOnCurve}});

    Expect(EC_KEY_set_group_nid_postfix(peer, nullptr), 0, {});
    Expect(EC_KEY_check_key_nid_postfix(peer), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_generate_key_nid_postfix(peer), 0, {{LibraryEc, PassedNullParameter}});
    Expect(EC_KEY_set_public_key_nid_postfix(peer, point), 0, {{LibraryEc, PassedNullParameter}});

    EC_KEY_free_nid_postfix(publicOnly);
    EC_KEY_free_nid_postfix(peer);
    EC_KEY_free_nid_postfix(key);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
}

static void TestSignatures() {
    const Bytes digest = Hex(SampleDigest);
    unsigned char buffer[80];
    unsigned int length = 123;

    EcKey* publicOnly = PublicKeyOnly(P256, Hex(RfcPublicKey));
    Expect(Sign(digest, buffer, &length, publicOnly), 0, {{LibraryEcdsa, PassedNullParameter}});
    Require(length == 0);
    EcKey* empty = EC_KEY_new_nid_postfix();
    length = 123;
    Expect(Sign(digest, buffer, &length, empty), 0, {{LibraryEcdsa, PassedNullParameter}});
    Require(length == 0);
    QueueEarlier();
    Expect(Sign(digest, buffer, &length, publicOnly), 0, {Earlier, {LibraryEcdsa, PassedNullParameter}});

    EcKey* signer = GeneratedKey(P256);
    Expect(Sign(digest, buffer, &length, signer), 1, {});
    Expect(Verify(digest, Bytes(buffer, buffer + length), signer), 1, {});
    Expect(Verify(digest, Bytes(buffer, buffer + length), publicOnly), 0, {});

    const Bytes r = Concat({{0x00}, Hex(RfcR)});
    const Bytes s = Concat({{0x00}, Hex(RfcS)});
    const Bytes signature = Signature(r, s);
    Expect(Verify(digest, signature, publicOnly), 1, {});
    Bytes changed = digest;
    changed[31] ^= 1;
    Expect(Verify(changed, signature, publicOnly), 0, {});

    const Bytes order = Concat({{0x00}, Hex(Order)});
    const Bytes outOfRange[] = {
        Signature({0x00}, s), Signature(r, {0x00}), Signature(order, s), Signature(r, order),
        Signature(Concat({{0x01}, Bytes(299, 0)}), s), Signature({0x00}, {0x00}),
    };
    for (const Bytes& encoded : outOfRange) Expect(Verify(digest, encoded, publicOnly), 0, {{LibraryEcdsa, EcdsaBadSignature}});
    QueueEarlier();
    Expect(Verify(digest, outOfRange[0], publicOnly), 0, {Earlier, {LibraryEcdsa, EcdsaBadSignature}});

    EcKey* noPublicKey = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(noPublicKey != nullptr);
    Expect(Verify(digest, signature, noPublicKey), -1, {{LibraryEcdsa, EcdsaMissingParameters}});
    Expect(Verify(digest, outOfRange[0], noPublicKey), -1, {{LibraryEcdsa, EcdsaMissingParameters}});
    Expect(Verify(digest, signature, empty), -1, {{LibraryEcdsa, EcdsaMissingParameters}});

    const Bytes infinityR = Hex(InfinityR);
    for (const Bytes& infinityS : {Bytes{0x01}, Bytes{0x02}, Bytes{0x01, 0x23, 0x45, 0x67}}) {
        Expect(Verify(digest, Signature(infinityR, infinityS), publicOnly), -1, {{LibraryEc, EcPointAtInfinity}, {LibraryEcdsa, ReasonEcLibrary}});
    }
    Expect(Verify(changed, Signature(infinityR, {0x01}), publicOnly), 0, {});

    EC_KEY_free_nid_postfix(noPublicKey);
    EC_KEY_free_nid_postfix(signer);
    EC_KEY_free_nid_postfix(empty);
    EC_KEY_free_nid_postfix(publicOnly);
}

static void TestSharedSecrets() {
    EcKey* key = GeneratedKey(P256);
    EcKey* peer = GeneratedKey(P256);
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    const EcPoint* peerKey = EC_KEY_get0_public_key_nid_postfix(peer);
    Bytes secret(40, 0xaa);
    unsigned char derived[16] = {};

    Expect(Agree(secret.data(), 32, peerKey, key), 32, {});
    Expect(Agree(secret.data(), 40, peerKey, key), 32, {});
    Expect(Agree(secret.data(), 31, peerKey, key), -1, {{LibraryEcdh, EcdhKeyTruncation}});
    Expect(Agree(nullptr, 0, peerKey, key), -1, {{LibraryEcdh, EcdhKeyTruncation}});
    Expect(Agree(secret.data(), static_cast<std::size_t>(INT_MAX) + 1, peerKey, key), -1, {{LibraryEcdh, MallocFailure}});
    Expect(Agree(secret.data(), static_cast<std::size_t>(INT_MAX), peerKey, key, CopyingKdf), 7, {});
    Expect(Agree(derived, 2, peerKey, key, CopyingKdf), 7, {});
    Expect(Agree(derived, sizeof(derived), peerKey, key, FailingKdf), -1, {{LibraryEcdh, EcdhKdfFailed}});
    QueueEarlier();
    Expect(Agree(secret.data(), 31, peerKey, key), -1, {Earlier, {LibraryEcdh, EcdhKeyTruncation}});

    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    EcGroup* otherGroup = EC_GROUP_new_by_curve_name_nid_postfix(P384);
    EcPoint* otherInfinity = EC_POINT_new_nid_postfix(otherGroup);
    Require(infinity != nullptr && otherGroup != nullptr && otherInfinity != nullptr);
    Expect(Agree(secret.data(), 32, infinity, key), -1, {{LibraryEc, EcPointAtInfinity}, {LibraryEcdh, EcdhPointArithmeticFailure}});
    Expect(Agree(secret.data(), 32, nullptr, key), -1, {{LibraryEc, EcPointAtInfinity}, {LibraryEcdh, EcdhPointArithmeticFailure}});
    Expect(Agree(secret.data(), 32, otherInfinity, key), -1, {});

    EcKey* publicOnly = PublicKeyOnly(P256, Hex(Generator));
    EcKey* empty = EC_KEY_new_nid_postfix();
    Expect(Agree(secret.data(), 32, peerKey, publicOnly), -1, {{LibraryEcdh, EcdhNoPrivateValue}});
    Expect(Agree(secret.data(), 32, nullptr, publicOnly), -1, {{LibraryEcdh, EcdhNoPrivateValue}});
    Expect(Agree(secret.data(), 32, peerKey, empty), -1, {{LibraryEcdh, EcdhNoPrivateValue}});
    Expect(Agree(secret.data(), static_cast<std::size_t>(INT_MAX) + 1, peerKey, publicOnly), -1, {{LibraryEcdh, MallocFailure}});

    Expect(EC_KEY_set_group_nid_postfix(key, nullptr), 0, {});
    Expect(Agree(secret.data(), 32, peerKey, key), -1, {{LibraryEc, PassedNullParameter}, {LibraryEcdh, MallocFailure}});
    Expect(Agree(secret.data(), 32, nullptr, key), -1, {{LibraryEc, PassedNullParameter}, {LibraryEcdh, MallocFailure}});

    EC_KEY_free_nid_postfix(empty);
    EC_KEY_free_nid_postfix(publicOnly);
    EC_POINT_free_nid_postfix(otherInfinity);
    EC_GROUP_free_nid_postfix(otherGroup);
    EC_POINT_free_nid_postfix(infinity);
    EC_KEY_free_nid_postfix(peer);
    EC_KEY_free_nid_postfix(key);
}

int main() {
    void* const heapApi[10]{};
    ApplicationHeapRegister_nid_no_patch(heapApi);
    ERR_clear_error_nid_postfix();
    TestCurveNames();
    TestPointEncoding();
    TestKeys();
    TestSignatures();
    TestSharedSecrets();
    std::puts("libSceLibreSsl EC error queue tests passed");
    return 0;
}
