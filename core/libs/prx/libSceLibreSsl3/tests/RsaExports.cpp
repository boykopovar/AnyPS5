#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <type_traits>
#include <unordered_set>

using KeyQuery = int (APS5_VABI*)(const LibreSsl3Rsa*);
static_assert(std::is_same_v<decltype(&RSA_size_nid_disambig1_nid_postfix), KeyQuery>);
static_assert(std::is_same_v<decltype(&RSA_flags_nid_disambig1_nid_postfix), KeyQuery>);

using RsaCrypt = int (APS5_VABI*)(int length, const unsigned char* from, unsigned char* to, LibreSsl3Rsa* rsa, int padding);
static_assert(std::is_same_v<decltype(&RSA_public_encrypt_nid_disambig1_nid_postfix), RsaCrypt>);
static_assert(std::is_same_v<decltype(&RSA_public_decrypt_nid_disambig1_nid_postfix), RsaCrypt>);
static_assert(std::is_same_v<decltype(&RSA_private_encrypt_nid_disambig1_nid_postfix), RsaCrypt>);
static_assert(std::is_same_v<decltype(&RSA_private_decrypt_nid_disambig1_nid_postfix), RsaCrypt>);

using SignRsa = int (APS5_VABI*)(int type, const unsigned char* message, unsigned int messageLength, unsigned char* signature, unsigned int* signatureLength, LibreSsl3Rsa* rsa);
static_assert(std::is_same_v<decltype(&RSA_sign_nid_disambig1_nid_postfix), SignRsa>);

using VerifyRsa = int (APS5_VABI*)(int type, const unsigned char* message, unsigned int messageLength, const unsigned char* signature, unsigned int signatureLength, LibreSsl3Rsa* rsa);
static_assert(std::is_same_v<decltype(&RSA_verify_nid_disambig1_nid_postfix), VerifyRsa>);

using AddPss = int (APS5_VABI*)(LibreSsl3Rsa* rsa, unsigned char* encoded, const unsigned char* messageHash, const LibreSsl3::DigestMethod* digest, int saltLength);
static_assert(std::is_same_v<decltype(&RSA_padding_add_PKCS1_PSS_nid_disambig1_nid_postfix), AddPss>);

using VerifyPss = int (APS5_VABI*)(LibreSsl3Rsa* rsa, const unsigned char* messageHash, const LibreSsl3::DigestMethod* digest, const unsigned char* encoded, int saltLength);
static_assert(std::is_same_v<decltype(&RSA_verify_PKCS1_PSS_nid_disambig1_nid_postfix), VerifyPss>);

static_assert(offsetof(LibreSsl3Rsa, method) == offsetof(LibreSslRsa, meth));

namespace {

constexpr int Sha256Type = 672;
constexpr std::size_t KeyBytes = 256;

constexpr unsigned char KeyModulus[] {
    190, 151, 144, 147, 130, 214, 20, 24, 80, 240, 239, 214, 182, 18, 112, 85,
    63, 39, 33, 58, 54, 38, 0, 33, 157, 225, 239, 240, 245, 124, 69, 106,
    168, 68, 173, 131, 103, 123, 105, 245, 60, 9, 239, 222, 78, 28, 163, 228,
    160, 224, 12, 49, 196, 3, 97, 17, 166, 26, 118, 241, 53, 74, 100, 28,
    44, 230, 215, 92, 213, 59, 101, 7, 120, 157, 217, 36, 203, 52, 24, 250,
    96, 104, 212, 85, 44, 138, 57, 154, 65, 144, 168, 205, 180, 8, 245, 240,
    234, 80, 55, 179, 161, 49, 101, 65, 30, 39, 92, 149, 6, 200, 70, 200,
    226, 171, 41, 172, 205, 143, 207, 5, 220, 129, 137, 107, 52, 252, 113, 138,
    40, 165, 56, 82, 149, 2, 91, 235, 17, 255, 10, 230, 98, 214, 46, 65,
    99, 189, 45, 78, 115, 170, 130, 93, 221, 93, 76, 151, 145, 121, 65, 65,
    236, 200, 166, 149, 152, 195, 209, 48, 89, 149, 106, 112, 117, 26, 129, 231,
    125, 188, 192, 248, 137, 116, 152, 62, 21, 41, 148, 125, 19, 203, 47, 141,
    33, 235, 20, 71, 180, 54, 104, 216, 242, 247, 160, 192, 90, 242, 195, 170,
    149, 191, 185, 241, 33, 239, 56, 149, 196, 86, 58, 39, 86, 181, 73, 155,
    33, 35, 7, 196, 169, 49, 216, 203, 219, 223, 220, 88, 30, 78, 2, 130,
    189, 148, 164, 98, 155, 179, 142, 93, 69, 0, 31, 8, 56, 199, 241, 241,
};

constexpr unsigned char KeyPrivateExponent[] {
    17, 121, 130, 99, 38, 169, 1, 48, 255, 142, 218, 97, 79, 39, 243, 249,
    15, 154, 195, 89, 166, 125, 167, 202, 109, 66, 228, 197, 215, 132, 49, 139,
    39, 106, 197, 235, 238, 144, 213, 95, 96, 240, 175, 162, 86, 208, 26, 110,
    33, 26, 153, 190, 75, 6, 7, 160, 52, 203, 104, 12, 189, 65, 153, 4,
    132, 251, 105, 253, 233, 123, 188, 203, 116, 17, 126, 71, 120, 216, 31, 40,
    41, 175, 24, 52, 199, 170, 41, 107, 141, 114, 100, 42, 110, 196, 217, 142,
    140, 10, 151, 40, 214, 241, 46, 162, 245, 249, 164, 65, 71, 178, 20, 220,
    181, 252, 253, 54, 95, 167, 61, 83, 43, 107, 97, 188, 71, 117, 233, 58,
    68, 119, 250, 137, 243, 99, 234, 163, 197, 65, 135, 160, 170, 88, 230, 161,
    87, 91, 19, 102, 170, 138, 142, 42, 198, 139, 249, 106, 204, 97, 99, 52,
    126, 151, 54, 185, 97, 227, 155, 4, 164, 26, 240, 54, 178, 110, 137, 179,
    208, 127, 176, 127, 134, 138, 240, 156, 96, 128, 89, 211, 61, 24, 187, 194,
    144, 223, 24, 231, 228, 242, 86, 150, 202, 84, 73, 62, 114, 6, 6, 233,
    67, 107, 160, 184, 189, 1, 237, 116, 193, 31, 106, 110, 121, 149, 95, 76,
    224, 99, 32, 209, 100, 210, 159, 246, 38, 184, 34, 162, 177, 155, 161, 173,
    82, 242, 237, 234, 96, 155, 62, 42, 100, 132, 179, 248, 15, 144, 93, 215,
};

constexpr unsigned char PublicExponent[] {1, 0, 1};

std::unordered_set<void*> live;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSA exports check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

template<typename TCall> void Throws(TCall call, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try { call(); } catch (const std::exception&) { threw = true; }
    Require(threw, location);
}

void* APS5_VABI HeapAllocate(std::size_t size) {
    void* pointer = std::malloc(size == 0 ? 1 : size);
    if (pointer) live.insert(pointer);
    return pointer;
}
void APS5_VABI HeapRelease(void* pointer) {
    if (!pointer) return;
    Require(live.erase(pointer) == 1);
    std::free(pointer);
}
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    if (pointer) live.insert(pointer);
    return pointer;
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    if (pointer) Require(live.erase(pointer) == 1);
    void* moved = std::realloc(pointer, size == 0 ? 1 : size);
    if (moved) live.insert(moved);
    return moved;
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

using Block = std::array<unsigned char, KeyBytes>;

std::array<unsigned char, 32> MessageHash() {
    std::array<unsigned char, 32> hash{};
    for (std::size_t i = 0; i < hash.size(); ++i) hash[i] = static_cast<unsigned char>(0x30 + i);
    return hash;
}

Block PlainBlock() {
    Block block{};
    for (std::size_t i = 1; i < block.size(); ++i) block[i] = static_cast<unsigned char>(i * 7);
    return block;
}

const LibreSsl3::DigestMethod* Sha256() {
    return reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix());
}

LibreSsl3Rsa* NewNativeKey() {
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr && rsa->method == nullptr);
    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(KeyModulus, sizeof(KeyModulus));
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(PublicExponent, sizeof(PublicExponent));
    auto* privateExponent = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrivateExponent, sizeof(KeyPrivateExponent));
    Require(modulus != nullptr && exponent != nullptr && privateExponent != nullptr);
    Require(RSA_set0_key_nid_postfix(rsa, modulus, exponent, privateExponent) == 1);
    return rsa;
}

LibreSslRsa* NewParentKey() {
    auto* rsa = RSA_new_nid_postfix();
    Require(rsa != nullptr && rsa->meth != nullptr);
    rsa->n = BN_bin2bn_nid_postfix(KeyModulus, sizeof(KeyModulus), nullptr);
    rsa->e = BN_bin2bn_nid_postfix(PublicExponent, sizeof(PublicExponent), nullptr);
    rsa->d = BN_bin2bn_nid_postfix(KeyPrivateExponent, sizeof(KeyPrivateExponent), nullptr);
    Require(rsa->n != nullptr && rsa->e != nullptr && rsa->d != nullptr);
    return rsa;
}

void TestParentRouting() {
    const std::size_t baseline = live.size();
    auto* parent = RSA_new_nid_postfix();
    Require(parent != nullptr && parent->meth != nullptr && live.contains(parent));
    parent->n = BN_bin2bn_nid_postfix(std::array<unsigned char, 2>{0xc3, 0x01}.data(), 2, nullptr);
    parent->e = BN_bin2bn_nid_postfix(std::array<unsigned char, 1>{0x03}.data(), 1, nullptr);
    auto* key = reinterpret_cast<LibreSsl3Rsa*>(parent);
    Require(RSA_size_nid_disambig1_nid_postfix(key) == RSA_size_nid_postfix(parent));
    Require(RSA_size_nid_disambig1_nid_postfix(key) == 2);
    Require(RSA_flags_nid_disambig1_nid_postfix(key) == RSA_flags_nid_postfix(parent));
    RSA_free_nid_disambig1_nid_postfix(key);
    Require(live.size() == baseline);
}

void TestParentCrypto() {
    const std::size_t baseline = live.size();
    auto* parent = NewParentKey();
    auto* key = reinterpret_cast<LibreSsl3Rsa*>(parent);
    const auto hash = MessageHash();
    const auto plain = PlainBlock();

    Block expected{};
    Block actual{};
    Require(RSA_public_encrypt_nid_postfix(KeyBytes, plain.data(), expected.data(), parent, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(RSA_public_encrypt_nid_disambig1_nid_postfix(KeyBytes, plain.data(), actual.data(), key, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(actual == expected);
    Block decrypted{};
    Require(RSA_private_decrypt_nid_disambig1_nid_postfix(KeyBytes, actual.data(), decrypted.data(), key, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(decrypted == plain);

    Require(RSA_private_encrypt_nid_disambig1_nid_postfix(hash.size(), hash.data(), actual.data(), key, LibreSsl3RsaPaddingPkcs1) == KeyBytes);
    Require(RSA_public_decrypt_nid_disambig1_nid_postfix(KeyBytes, actual.data(), decrypted.data(), key, LibreSsl3RsaPaddingPkcs1) == static_cast<int>(hash.size()));
    Require(std::memcmp(decrypted.data(), hash.data(), hash.size()) == 0);

    unsigned int expectedLength = 0;
    unsigned int actualLength = 0;
    Require(RSA_sign_nid_postfix(Sha256Type, hash.data(), hash.size(), expected.data(), &expectedLength, parent) == 1);
    Require(RSA_sign_nid_disambig1_nid_postfix(Sha256Type, hash.data(), hash.size(), actual.data(), &actualLength, key) == 1);
    Require(actualLength == KeyBytes && actualLength == expectedLength && actual == expected);
    Require(RSA_verify_nid_disambig1_nid_postfix(Sha256Type, hash.data(), hash.size(), actual.data(), actualLength, key) == 1);

    Block encoded{};
    Require(RSA_padding_add_PKCS1_PSS_nid_disambig1_nid_postfix(key, encoded.data(), hash.data(), Sha256(), LibreSsl3RsaPssSaltLengthDigest) == 1);
    Require(RSA_verify_PKCS1_PSS_nid_postfix(parent, hash.data(), EVP_sha256_nid_postfix(), encoded.data(), LibreSsl3RsaPssSaltLengthDigest) == 1);
    Require(RSA_verify_PKCS1_PSS_nid_disambig1_nid_postfix(key, hash.data(), Sha256(), encoded.data(), LibreSsl3RsaPssSaltLengthDigest) == 1);

    RSA_free_nid_disambig1_nid_postfix(key);
    Require(live.size() == baseline);
}

void TestNativeKey() {
    const std::size_t baseline = live.size();
    auto* rsa = NewNativeKey();
    auto* parent = NewParentKey();
    const auto hash = MessageHash();
    const auto plain = PlainBlock();

    Require(RSA_size_nid_disambig1_nid_postfix(rsa) == KeyBytes);
    Require(RSA_flags_nid_disambig1_nid_postfix(rsa) == 0);

    Block expected{};
    Block actual{};
    Require(RSA_public_encrypt_nid_postfix(KeyBytes, plain.data(), expected.data(), parent, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(RSA_public_encrypt_nid_disambig1_nid_postfix(KeyBytes, plain.data(), actual.data(), rsa, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(actual == expected);
    Block decrypted{};
    Require(RSA_private_decrypt_nid_disambig1_nid_postfix(KeyBytes, actual.data(), decrypted.data(), rsa, LibreSsl3RsaPaddingNone) == KeyBytes);
    Require(decrypted == plain);

    Require(RSA_private_encrypt_nid_disambig1_nid_postfix(hash.size(), hash.data(), actual.data(), rsa, LibreSsl3RsaPaddingPkcs1) == KeyBytes);
    Require(RSA_public_decrypt_nid_disambig1_nid_postfix(KeyBytes, actual.data(), decrypted.data(), rsa, LibreSsl3RsaPaddingPkcs1) == static_cast<int>(hash.size()));
    Require(std::memcmp(decrypted.data(), hash.data(), hash.size()) == 0);

    unsigned int expectedLength = 0;
    unsigned int actualLength = 0;
    Require(RSA_sign_nid_postfix(Sha256Type, hash.data(), hash.size(), expected.data(), &expectedLength, parent) == 1);
    Require(RSA_sign_nid_disambig1_nid_postfix(Sha256Type, hash.data(), hash.size(), actual.data(), &actualLength, rsa) == 1);
    Require(actualLength == KeyBytes && actualLength == expectedLength && actual == expected);
    Require(RSA_verify_nid_disambig1_nid_postfix(Sha256Type, hash.data(), hash.size(), actual.data(), actualLength, rsa) == 1);
    actual[KeyBytes - 1] ^= 1;
    Require(RSA_verify_nid_disambig1_nid_postfix(Sha256Type, hash.data(), hash.size(), actual.data(), actualLength, rsa) == 0);

    Block encoded{};
    Require(RSA_padding_add_PKCS1_PSS_nid_disambig1_nid_postfix(rsa, encoded.data(), hash.data(), Sha256(), LibreSsl3RsaPssSaltLengthDigest) == 1);
    Require(RSA_verify_PKCS1_PSS_nid_disambig1_nid_postfix(rsa, hash.data(), Sha256(), encoded.data(), LibreSsl3RsaPssSaltLengthDigest) == 1);
    Require(RSA_verify_PKCS1_PSS_nid_postfix(parent, hash.data(), EVP_sha256_nid_postfix(), encoded.data(), LibreSsl3RsaPssSaltLengthDigest) == 1);
    encoded[0] ^= 1;
    Require(RSA_verify_PKCS1_PSS_nid_disambig1_nid_postfix(rsa, hash.data(), Sha256(), encoded.data(), LibreSsl3RsaPssSaltLengthDigest) == 0);

    ERR_clear_error_nid_postfix();
    RSA_free_nid_disambig1_nid_postfix(rsa);
    RSA_free_nid_postfix(parent);
    Require(live.size() == baseline);
}

void TestInvalidKeys() {
    auto* unreadable = reinterpret_cast<LibreSsl3Rsa*>(std::uintptr_t{1});
    Throws([] { RSA_size_nid_disambig1_nid_postfix(nullptr); });
    Throws([unreadable] { RSA_size_nid_disambig1_nid_postfix(unreadable); });
    Require(RSA_flags_nid_disambig1_nid_postfix(nullptr) == 0);
    Throws([unreadable] { RSA_flags_nid_disambig1_nid_postfix(unreadable); });
    Throws([unreadable] {
        std::array<unsigned char, 1> from{};
        std::array<unsigned char, 1> to{};
        RSA_public_encrypt_nid_disambig1_nid_postfix(1, from.data(), to.data(), unreadable, LibreSsl3RsaPaddingNone);
    });
}

}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapRelease),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestParentRouting();
    TestParentCrypto();
    TestNativeKey();
    TestInvalidKeys();
    return 0;
}
