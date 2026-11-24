#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr int RsaType = 6;
constexpr int EcType = 408;
constexpr int HmacType = 855;
constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr int PointConversionCompressed = 2;
constexpr int PointConversionUncompressed = 4;

constexpr int ErrLibEc = 16;
constexpr int ErrLibX509 = 11;

constexpr int ErrREcLib = 16;
constexpr int ErrRPassedNullParameter = 67;
constexpr int EcRMissingParameters = 124;
constexpr int X509RUnsupportedAlgorithm = 111;
constexpr int X509RPublicKeyEncodeError = 126;
constexpr int X509RMethodNotSupported = 124;

constexpr int DsaType = 116;
constexpr int RsaPssType = 912;
constexpr int Secp521r1 = 716;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "requirement failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::vector<unsigned char> Hex(const char* text) {
    std::vector<unsigned char> bytes;
    for (std::size_t index = 0; text[index] != '\0'; index += 2) {
        const char pair[3] = {text[index], text[index + 1], '\0'};
        bytes.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return bytes;
}

const std::vector<unsigned char> RsaSpkiDer = Hex(
    "30820122300d06092a864886f70d01010105000382010f003082010a0282010100b059e3d97fed73cbe97f78b51b0601b6c30fb63fe9"
    "d9c20853bd9668b9e5d0dd697ed72631eb0e13c8eb6de9881efed8286c2d312aeb28f1a1b10616828185bd352dd78da0173adfb85477"
    "df5b3f3529c56c19fd66f99231b5f019dc9bc5311dc0ed2ff3d51d2bdc3c0e3518bf60b38903d0ecd148332d81dc5e6a88823dab5b0c"
    "882ba20cb6b4e71eb66651ae759f757c31069cfdb8481680bc20d2dc5c9d2c01a6c41c01a8d4d236479cfbe19f83c17009bd9542c031"
    "ae066c6529ca7d71ca31f873701f43725b8d5bd074f436cfcf34e290e8bd4f63f25116360a5dd70a72edf7e0dcfdcf511eccfd8aac46"
    "7e32f3713ce96546857841c2652850d94962650203010001");

const std::vector<unsigned char> P256SpkiDer = Hex(
    "3059301306072a8648ce3d020106082a8648ce3d030107034200042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340d"
    "f7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807b7c20a497bec0d55a6075410");

const std::vector<unsigned char> P384Pkcs8Der = Hex(
    "3081b6020100301006072a8648ce3d020106052b8104002204819e30819b02010104306e518f934d0aff63bd695e6dd06c73bc5a8f78"
    "0f8903c22ed479b916944e9add1b9d91d5317e077e38a82cca7d26e775a16403620004fea682de112d46d5b0fb886f995b2d2595d623"
    "2f1a6a9291585bf9feacc96dd831b6879dae519df72c30c9a24b63c4b0dc00672e322b837ee6d40268bca6c8e91a10b79cce0d02429d"
    "36e885907807c1cc9384517b5caed5c9a08f54232823d1");

void ExpectError(int expectedLib, int expectedReason,
    std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    const int lib = static_cast<int>((code >> 24) & 0xff);
    const int reason = static_cast<int>(code & 0xfff);
    Require(lib == expectedLib && reason == expectedReason, location);
}

LibreSsl3EvpPkey* DecodeSpki(const std::vector<unsigned char>& der) {
    const unsigned char* cursor = der.data();
    LibreSsl3EvpPkey* key = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    Require(key != nullptr && cursor == der.data() + der.size());
    return key;
}

LibreSsl3EvpPkey* DecodePkcs8(const std::vector<unsigned char>& der) {
    const unsigned char* cursor = der.data();
    LibreSsl3Pkcs8PrivKeyInfo* info = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(der.size()));
    Require(info != nullptr && cursor == der.data() + der.size());
    LibreSsl3EvpPkey* key = EVP_PKCS82PKEY_nid_postfix(info);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(info);
    Require(key != nullptr);
    return key;
}

std::vector<unsigned char> EcPublicBytes(const EcKey* ec) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(ec);
    const EcPoint* point = EC_KEY_get0_public_key_nid_postfix(ec);
    Require(group != nullptr && point != nullptr);
    const std::size_t size = EC_POINT_point2oct_nid_postfix(
        group, point, PointConversionUncompressed, nullptr, 0, nullptr);
    Require(size != 0);
    std::vector<unsigned char> bytes(size);
    Require(EC_POINT_point2oct_nid_postfix(
        group, point, PointConversionUncompressed, bytes.data(), bytes.size(), nullptr) == size);
    return bytes;
}

void RequireEncodedKey(LibreSsl3EvpPkey* key, const std::vector<unsigned char>& expected) {
    const int sizeOnly = i2d_PUBKEY_nid_postfix(key, nullptr);
    Require(sizeOnly == static_cast<int>(expected.size()));

    unsigned char* allocated = nullptr;
    const int allocatedLength = i2d_PUBKEY_nid_postfix(key, &allocated);
    Require(allocatedLength == sizeOnly && allocated != nullptr);
    Require(std::memcmp(allocated, expected.data(), expected.size()) == 0);

    std::vector<unsigned char> manual(expected.size() + 4, 0xcc);
    unsigned char* writePointer = manual.data();
    const int writtenLength = i2d_PUBKEY_nid_postfix(key, &writePointer);
    Require(writtenLength == sizeOnly);
    Require(writePointer == manual.data() + expected.size());
    Require(std::memcmp(manual.data(), expected.data(), expected.size()) == 0);
    Require(manual[expected.size()] == 0xcc);

    free_nid_postfix(allocated);
}

void TestRsaRoundTrip() {
    LibreSsl3EvpPkey* key = DecodeSpki(RsaSpkiDer);
    Require(key->type == RsaType);
    Require(key->method != nullptr);
    RequireEncodedKey(key, RsaSpkiDer);

    const int sizeOnly = i2d_PUBKEY_nid_postfix(key, nullptr);
    unsigned char* allocated = nullptr;
    Require(i2d_PUBKEY_nid_postfix(key, &allocated) == sizeOnly && allocated != nullptr);
    const unsigned char* cursor = allocated;
    LibreSsl3EvpPkey* redecoded = d2i_PUBKEY_nid_postfix(nullptr, &cursor, sizeOnly);
    Require(redecoded != nullptr && cursor == allocated + sizeOnly);
    Require(redecoded->type == RsaType);
    RequireEncodedKey(redecoded, RsaSpkiDer);

    free_nid_postfix(allocated);
    EVP_PKEY_free_nid_disambig1_nid_postfix(redecoded);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void TestEcP256RoundTrip() {
    LibreSsl3EvpPkey* key = DecodeSpki(P256SpkiDer);
    Require(key->type == EcType);
    Require(key->method == nullptr);
    RequireEncodedKey(key, P256SpkiDer);

    auto* ec = static_cast<EcKey*>(key->key);
    Require(ec != nullptr && ec->conversionForm == PointConversionUncompressed);
    const std::vector<unsigned char> uncompressed = EcPublicBytes(ec);
    unsigned char* allocated = nullptr;
    const int allocatedLength = i2d_PUBKEY_nid_postfix(key, &allocated);
    Require(allocatedLength == static_cast<int>(P256SpkiDer.size()) && allocated != nullptr);
    Require(std::memcmp(allocated + allocatedLength - uncompressed.size(), uncompressed.data(),
                uncompressed.size()) == 0);
    free_nid_postfix(allocated);

    ec->conversionForm = PointConversionCompressed;
    const int compressedSize = i2d_PUBKEY_nid_postfix(key, nullptr);
    Require(compressedSize == 59);
    std::vector<unsigned char> compressed(static_cast<std::size_t>(compressedSize));
    unsigned char* writePointer = compressed.data();
    Require(i2d_PUBKEY_nid_postfix(key, &writePointer) == compressedSize);
    Require(writePointer == compressed.data() + compressedSize);
    Require(compressed[26] == 0x02 || compressed[26] == 0x03);

    LibreSsl3EvpPkey* redecoded = DecodeSpki(compressed);
    Require(redecoded->type == EcType);
    const auto* redecodeEc = static_cast<const EcKey*>(redecoded->key);
    Require(redecodeEc != nullptr && redecodeEc->group != nullptr);
    Require(redecodeEc->group->curve.nid == Secp256r1);
    Require(redecodeEc->conversionForm == PointConversionCompressed);
    Require(EcPublicBytes(redecodeEc) == uncompressed);
    Require(i2d_PUBKEY_nid_postfix(redecoded, nullptr) == compressedSize);

    EVP_PKEY_free_nid_disambig1_nid_postfix(redecoded);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void TestEcP384RoundTrip() {
    LibreSsl3EvpPkey* key = DecodePkcs8(P384Pkcs8Der);
    Require(key->type == EcType);
    const auto* ec = static_cast<const EcKey*>(key->key);
    Require(ec != nullptr && ec->group != nullptr && ec->group->curve.nid == Secp384r1);
    const std::vector<unsigned char> point = EcPublicBytes(ec);

    const int sizeOnly = i2d_PUBKEY_nid_postfix(key, nullptr);
    Require(sizeOnly > 0);
    std::vector<unsigned char> spki(static_cast<std::size_t>(sizeOnly));
    unsigned char* writePointer = spki.data();
    Require(i2d_PUBKEY_nid_postfix(key, &writePointer) == sizeOnly);
    Require(writePointer == spki.data() + sizeOnly);
    Require(spki.size() == 120);

    LibreSsl3EvpPkey* redecoded = DecodeSpki(spki);
    const auto* redecodedEc = static_cast<const EcKey*>(redecoded->key);
    Require(redecodedEc != nullptr && redecodedEc->group != nullptr);
    Require(redecodedEc->group->curve.nid == Secp384r1);
    Require(EcPublicBytes(redecodedEc) == point);
    Require(i2d_PUBKEY_nid_postfix(redecoded, nullptr) == sizeOnly);

    EVP_PKEY_free_nid_disambig1_nid_postfix(redecoded);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void TestNullKey() {
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(nullptr, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == 0);

    unsigned char* untouched = nullptr;
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(nullptr, &untouched) == 0);
    Require(untouched == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
}

void TestMissingKeyData() {
    LibreSsl3EvpPkey* rsa = DecodeSpki(RsaSpkiDer);
    void* savedKey = rsa->key;
    rsa->key = nullptr;
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(rsa, nullptr) == -1);
    ExpectError(ErrLibX509, X509RPublicKeyEncodeError);
    rsa->key = savedKey;
    EVP_PKEY_free_nid_disambig1_nid_postfix(rsa);

    LibreSsl3EvpPkey* ec = DecodeSpki(P256SpkiDer);
    savedKey = ec->key;
    ec->key = nullptr;
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(ec, nullptr) == -1);
    ExpectError(ErrLibEc, EcRMissingParameters);
    ExpectError(ErrLibEc, ErrREcLib);
    ExpectError(ErrLibX509, X509RPublicKeyEncodeError);
    ec->key = savedKey;
    EVP_PKEY_free_nid_disambig1_nid_postfix(ec);
}

void TestMissingMethod() {
    LibreSsl3EvpPkey* key = DecodeSpki(RsaSpkiDer);
    const void* savedMethod = key->method;
    key->method = nullptr;
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(key, nullptr) == -1);
    ExpectError(ErrLibX509, X509RUnsupportedAlgorithm);
    key->method = savedMethod;
    Require(i2d_PUBKEY_nid_postfix(key, nullptr) == static_cast<int>(RsaSpkiDer.size()));
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void RequireNotImplemented(LibreSsl3EvpPkey* key) {
    bool threw = false;
    try {
        i2d_PUBKEY_nid_postfix(key, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestGuestOutputPointers() {
    LibreSsl3EvpPkey* key = DecodeSpki(RsaSpkiDer);
    Require(key != nullptr);

    bool threw = false;
    try {
        i2d_PUBKEY_nid_postfix(key, reinterpret_cast<unsigned char**>(0x10));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    unsigned char* unwritable = reinterpret_cast<unsigned char*>(0x10);
    threw = false;
    try {
        i2d_PUBKEY_nid_postfix(key, &unwritable);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(unwritable == reinterpret_cast<unsigned char*>(0x10));
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void TestHmacKeyWithoutPublicKeyEncoder() {
    LibreSsl3EvpPkey* key = DecodeSpki(RsaSpkiDer);
    const std::int32_t savedType = key->type;
    key->type = HmacType;
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(key, nullptr) == -1);
    ExpectError(ErrLibX509, X509RMethodNotSupported);
    ExpectError(ErrLibX509, X509RPublicKeyEncodeError);
    key->type = savedType;
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void TestUnsupportedKeyTypes() {
    LibreSsl3EvpPkey* key = DecodeSpki(RsaSpkiDer);
    const std::int32_t savedType = key->type;
    key->type = DsaType;
    RequireNotImplemented(key);
    key->type = RsaPssType;
    RequireNotImplemented(key);
    key->type = savedType;
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);

    LibreSsl3EvpPkey* ecKey = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(ecKey != nullptr);
    EcKey* secp521 = EC_KEY_new_by_curve_name_nid_postfix(Secp521r1);
    Require(secp521 != nullptr);
    Require(EVP_PKEY_assign_nid_postfix(ecKey, EcType, secp521) == 1);
    RequireNotImplemented(ecKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ecKey);
}

void TestEcWithoutPublicKey() {
    LibreSsl3EvpPkey* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    EcKey* ec = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(ec != nullptr && ec->publicKey == nullptr);
    Require(EVP_PKEY_assign_nid_postfix(key, EcType, ec) == 1);
    ERR_clear_error_nid_postfix();
    Require(i2d_PUBKEY_nid_postfix(key, nullptr) == -1);
    ExpectError(ErrLibEc, ErrRPassedNullParameter);
    ExpectError(ErrLibX509, X509RPublicKeyEncodeError);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size == 0 ? 1 : size);
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestRsaRoundTrip();
    TestEcP256RoundTrip();
    TestEcP384RoundTrip();
    TestNullKey();
    TestMissingKeyData();
    TestMissingMethod();
    TestGuestOutputPointers();
    TestHmacKeyWithoutPublicKeyEncoder();
    TestUnsupportedKeyTypes();
    TestEcWithoutPublicKey();
    return 0;
}
