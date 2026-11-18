#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
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
#include <initializer_list>
#include <source_location>
#include <type_traits>
#include <unordered_set>
#include <vector>

using FreeRsa = void (APS5_VABI*)(LibreSsl3Rsa*);
using ReadRsaPublicKey = LibreSsl3Rsa* (APS5_VABI*)(LibreSsl::Bio*, LibreSsl3Rsa**);
static_assert(std::is_same_v<decltype(&RSA_free_nid_disambig1_nid_postfix), FreeRsa>);
static_assert(std::is_same_v<decltype(&d2i_RSAPublicKey_bio_nid_postfix), ReadRsaPublicKey>);
static_assert(offsetof(LibreSsl3Rsa, method) == offsetof(LibreSslRsa, meth));
static_assert(offsetof(LibreSsl3Rsa, references) == offsetof(LibreSslRsa, references));

namespace {

constexpr int ErrLibAsn1 = 13;

constexpr int ErrRNestedAsn1Error = 58;
constexpr int ErrRAsn1LengthMismatch = 62;
constexpr int Asn1RBadObjectHeader = 102;
constexpr int Asn1RFieldMissing = 121;
constexpr int Asn1RHeaderTooLong = 123;
constexpr int Asn1RSequenceLengthMismatch = 148;
constexpr int Asn1RSequenceNotConstructed = 149;
constexpr int Asn1RTooLong = 155;
constexpr int Asn1RUnexpectedEoc = 159;
constexpr int Asn1RWrongTag = 168;
constexpr int Asn1RTypeNotPrimitive = 231;

std::unordered_set<void*> live;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSAPublicKey check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

template<typename TCall> void Throws(TCall call, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try { call(); } catch (const std::exception&) { threw = true; }
    Require(threw, location);
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

const std::vector<unsigned char> RsaPkcs1PubDer = Hex(
    "3082010a0282010100b059e3d97fed73cbe97f78b51b0601b6c30fb63fe9d9c20853bd9668b9e5d0dd697ed72631eb0e13c8eb6de988"
    "1efed8286c2d312aeb28f1a1b10616828185bd352dd78da0173adfb85477df5b3f3529c56c19fd66f99231b5f019dc9bc5311dc0ed2ff"
    "3d51d2bdc3c0e3518bf60b38903d0ecd148332d81dc5e6a88823dab5b0c882ba20cb6b4e71eb66651ae759f757c31069cfdb8481680b"
    "c20d2dc5c9d2c01a6c41c01a8d4d236479cfbe19f83c17009bd9542c031ae066c6529ca7d71ca31f873701f43725b8d5bd074f436cfcf"
    "34e290e8bd4f63f25116360a5dd70a72edf7e0dcfdcf511eccfd8aac467e32f3713ce96546857841c2652850d94962650203010001");

const std::vector<unsigned char> SecondPkcs1PubDer = Hex(
    "3048024100c04fcdb9783192c1c998b11b16f7bebbd63e04f93010ac126a1d09e2cc7d34281f0fb2c0fe4ba1da3f68865bb7cab90a4c"
    "a5fea79b3e62c86eca1188f3346c690203010001");

std::vector<unsigned char> Bytes(const LibreSsl3Bignum* number) {
    const auto* value = reinterpret_cast<const Bignum*>(number);
    const int bits = BN_num_bits_nid_postfix(value);
    Require(bits >= 0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>((bits + 7) / 8));
    if (!bytes.empty()) Require(BN_bn2bin_nid_postfix(value, bytes.data()) == static_cast<int>(bytes.size()));
    return bytes;
}

bool Negative(const LibreSsl3Bignum* number) {
    return BN_is_negative_nid_postfix(reinterpret_cast<const Bignum*>(number)) != 0;
}

LibreSsl3Bignum* NewBignum(std::initializer_list<unsigned char> bytes) {
    const std::vector<unsigned char> value(bytes);
    auto* number = BN_bin2bn_nid_postfix(value.data(), static_cast<int>(value.size()), nullptr);
    Require(number != nullptr);
    return reinterpret_cast<LibreSsl3Bignum*>(number);
}

void ExpectErrors(std::initializer_list<int> reasons, std::source_location location = std::source_location::current()) {
    for (const int reason : reasons) {
        const std::uint64_t code = ERR_get_error_nid_postfix();
        Require(static_cast<int>((code >> 24) & 0xff) == ErrLibAsn1, location);
        Require(static_cast<int>(code & 0xfff) == reason, location);
    }
    Require(ERR_get_error_nid_postfix() == 0, location);
}

LibreSsl::Bio* MemoryBio(const std::vector<unsigned char>& bytes) {
    static const unsigned char nothing = 0;
    auto* bio = BIO_new_mem_buf_nid_postfix(bytes.empty() ? &nothing : bytes.data(), static_cast<int>(bytes.size()));
    Require(bio != nullptr);
    return bio;
}

LibreSsl3Rsa* ReadOne(const std::vector<unsigned char>& bytes, LibreSsl3Rsa** output) {
    auto* bio = MemoryBio(bytes);
    auto* rsa = d2i_RSAPublicKey_bio_nid_postfix(bio, output);
    BIO_free_nid_postfix(bio);
    return rsa;
}

LibreSsl3Rsa* ReadKey(const std::vector<unsigned char>& bytes) {
    auto* rsa = ReadOne(bytes, nullptr);
    Require(rsa != nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    return rsa;
}

void TestNativeFree() {
    const std::size_t baseline = live.size();
    RSA_free_nid_disambig1_nid_postfix(nullptr);
    Require(live.size() == baseline);

    auto* empty = RSA_new_nid_disambig1_nid_postfix();
    Require(empty != nullptr && empty->method == nullptr && live.contains(empty));
    RSA_free_nid_disambig1_nid_postfix(empty);
    Require(live.size() == baseline);

    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    auto* n = NewBignum({0xc3, 0x01});
    auto* e = NewBignum({0x01, 0x00, 0x01});
    auto* d = NewBignum({0x7f});
    Require(RSA_set0_key_nid_postfix(rsa, n, e, d) == 1);
    auto* p = NewBignum({0x0b});
    auto* q = NewBignum({0x0d});
    Require(RSA_set0_factors_nid_postfix(rsa, p, q) == 1);
    auto* dmp1 = NewBignum({0x03});
    auto* dmq1 = NewBignum({0x05});
    auto* iqmp = NewBignum({0x07});
    Require(RSA_set0_crt_params_nid_postfix(rsa, dmp1, dmq1, iqmp) == 1);
    Require(RSA_up_ref_nid_postfix(rsa) == 1);
    Require(rsa->references == 2);
    RSA_free_nid_disambig1_nid_postfix(rsa);
    Require(live.contains(rsa) && rsa->references == 1 && rsa->n == n && live.contains(n));
    RSA_free_nid_disambig1_nid_postfix(rsa);
    for (const void* pointer : {static_cast<const void*>(rsa), static_cast<const void*>(n), static_cast<const void*>(e),
             static_cast<const void*>(d), static_cast<const void*>(p), static_cast<const void*>(q),
             static_cast<const void*>(dmp1), static_cast<const void*>(dmq1), static_cast<const void*>(iqmp)})
        Require(!live.contains(const_cast<void*>(pointer)));
    Require(live.size() == baseline);

    auto* pss = RSA_new_nid_disambig1_nid_postfix();
    Require(pss != nullptr);
    int marker = 0;
    pss->pss = &marker;
    Throws([&] { RSA_free_nid_disambig1_nid_postfix(pss); });
    Require(live.contains(pss));
    pss->pss = nullptr;
    pss->references = 1;
    RSA_free_nid_disambig1_nid_postfix(pss);
    Require(live.size() == baseline);
}

void TestParentFree() {
    const std::size_t baseline = live.size();
    auto* parent = RSA_new_nid_postfix();
    Require(parent != nullptr && parent->meth != nullptr && live.contains(parent));
    parent->n = BN_bin2bn_nid_postfix(std::array<unsigned char, 2>{0xc3, 0x01}.data(), 2, nullptr);
    parent->e = BN_bin2bn_nid_postfix(std::array<unsigned char, 1>{0x03}.data(), 1, nullptr);
    Require(parent->n != nullptr && parent->e != nullptr);
    void* const n = parent->n;
    parent->references = 2;
    RSA_free_nid_disambig1_nid_postfix(reinterpret_cast<LibreSsl3Rsa*>(parent));
    Require(live.contains(parent) && parent->references == 1 && live.contains(n));
    RSA_free_nid_disambig1_nid_postfix(reinterpret_cast<LibreSsl3Rsa*>(parent));
    Require(!live.contains(parent) && !live.contains(n));
    Require(live.size() == baseline);
}

void TestPkeyReferenceRelease() {
    const std::size_t baseline = live.size();
    const unsigned char* cursor = RsaSpkiDer.data();
    auto* pkey = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(RsaSpkiDer.size()));
    Require(pkey != nullptr);
    auto* rsa = EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(pkey);
    Require(rsa != nullptr && rsa->references == 2);
    RSA_free_nid_disambig1_nid_postfix(rsa);
    Require(live.contains(rsa) && rsa->references == 1);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
    Require(!live.contains(rsa));
    Require(live.size() == baseline);
}

void TestDecodeMatchesSubjectPublicKeyInfo() {
    const std::size_t baseline = live.size();
    ERR_clear_error_nid_postfix();
    auto* rsa = ReadKey(RsaPkcs1PubDer);
    Require(rsa->method == nullptr && rsa->references == 1);
    Require(rsa->d == nullptr && rsa->p == nullptr && rsa->q == nullptr);
    Require(rsa->dmp1 == nullptr && rsa->dmq1 == nullptr && rsa->iqmp == nullptr);

    const unsigned char* cursor = RsaSpkiDer.data();
    auto* pkey = d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(RsaSpkiDer.size()));
    Require(pkey != nullptr);
    const auto* reference = static_cast<const LibreSsl3Rsa*>(pkey->key);
    Require(Bytes(static_cast<const LibreSsl3Bignum*>(rsa->n)) == Bytes(static_cast<const LibreSsl3Bignum*>(reference->n)));
    Require(Bytes(static_cast<const LibreSsl3Bignum*>(rsa->e)) == Bytes(static_cast<const LibreSsl3Bignum*>(reference->e)));
    Require(Bytes(static_cast<const LibreSsl3Bignum*>(rsa->e)) == std::vector<unsigned char>{0x01, 0x00, 0x01});
    Require(RSA_bits_nid_postfix(rsa) == 2048);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
    ERR_clear_error_nid_postfix();

    LibreSsl3Rsa* output = nullptr;
    auto* bio = MemoryBio(RsaPkcs1PubDer);
    auto* returned = d2i_RSAPublicKey_bio_nid_postfix(bio, &output);
    BIO_free_nid_postfix(bio);
    Require(returned != nullptr && output == returned);
    RSA_free_nid_disambig1_nid_postfix(output);
    RSA_free_nid_disambig1_nid_postfix(rsa);
    Require(live.size() == baseline);
}

void TestConsecutiveKeys() {
    const std::size_t baseline = live.size();
    std::vector<unsigned char> both = RsaPkcs1PubDer;
    both.insert(both.end(), SecondPkcs1PubDer.begin(), SecondPkcs1PubDer.end());
    auto* bio = MemoryBio(both);
    auto* first = d2i_RSAPublicKey_bio_nid_postfix(bio, nullptr);
    auto* second = d2i_RSAPublicKey_bio_nid_postfix(bio, nullptr);
    Require(first != nullptr && second != nullptr && first != second);
    Require(RSA_bits_nid_postfix(first) == 2048);
    Require(RSA_bits_nid_postfix(second) == 512);
    Require(d2i_RSAPublicKey_bio_nid_postfix(bio, nullptr) == nullptr);
    ExpectErrors({Asn1RHeaderTooLong});
    BIO_free_nid_postfix(bio);
    RSA_free_nid_disambig1_nid_postfix(first);
    RSA_free_nid_disambig1_nid_postfix(second);
    Require(live.size() == baseline);
}

void TestOutputReplacement() {
    const std::size_t baseline = live.size();

    auto* old = ReadKey(SecondPkcs1PubDer);
    Require(RSA_up_ref_nid_postfix(old) == 1);
    LibreSsl3Rsa* output = old;
    auto* replaced = ReadOne(RsaPkcs1PubDer, &output);
    Require(replaced != nullptr && replaced != old && output == replaced);
    Require(live.contains(old) && old->references == 1);
    RSA_free_nid_disambig1_nid_postfix(replaced);
    RSA_free_nid_disambig1_nid_postfix(old);
    Require(live.size() == baseline);

    auto* parent = RSA_new_nid_postfix();
    Require(parent != nullptr);
    parent->references = 2;
    output = reinterpret_cast<LibreSsl3Rsa*>(parent);
    replaced = ReadOne(RsaPkcs1PubDer, &output);
    Require(replaced != nullptr && output == replaced && parent->references == 1);
    RSA_free_nid_disambig1_nid_postfix(replaced);
    RSA_free_nid_disambig1_nid_postfix(reinterpret_cast<LibreSsl3Rsa*>(parent));
    Require(live.size() == baseline);

    old = ReadKey(SecondPkcs1PubDer);
    Require(RSA_up_ref_nid_postfix(old) == 1);
    output = old;
    Require(ReadOne(Hex("3003020101"), &output) == nullptr);
    ExpectErrors({Asn1RFieldMissing});
    Require(output == nullptr && live.contains(old) && old->references == 1);
    RSA_free_nid_disambig1_nid_postfix(old);
    Require(live.size() == baseline);

    auto* only = RSA_new_nid_disambig1_nid_postfix();
    output = only;
    Require(ReadOne(Hex("3103020101"), &output) == nullptr);
    ExpectErrors({Asn1RWrongTag, ErrRNestedAsn1Error});
    Require(output == nullptr && !live.contains(only));
    Require(live.size() == baseline);

    old = ReadKey(SecondPkcs1PubDer);
    output = old;
    Require(ReadOne({}, &output) == nullptr);
    ExpectErrors({Asn1RHeaderTooLong});
    Require(output == old && live.contains(old) && old->references == 1);
    Require(ReadOne(Hex("3081050201050201"), &output) == nullptr);
    ExpectErrors({Asn1RBadObjectHeader});
    Require(output == old && live.contains(old));
    RSA_free_nid_disambig1_nid_postfix(old);
    Require(live.size() == baseline);
}

void ExpectModulus(const char* der, std::initializer_list<unsigned char> magnitude, bool negative,
    std::source_location location = std::source_location::current()) {
    const std::size_t baseline = live.size();
    ERR_clear_error_nid_postfix();
    auto* rsa = ReadOne(Hex(der), nullptr);
    Require(rsa != nullptr, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
    const auto* n = static_cast<const LibreSsl3Bignum*>(rsa->n);
    Require(Bytes(n) == std::vector<unsigned char>(magnitude), location);
    Require(Negative(n) == negative, location);
    Require(Bytes(static_cast<const LibreSsl3Bignum*>(rsa->e)) == std::vector<unsigned char>{0x03}, location);
    RSA_free_nid_disambig1_nid_postfix(rsa);
    Require(live.size() == baseline, location);
}

void ExpectRejected(const char* der, std::initializer_list<int> reasons,
    std::source_location location = std::source_location::current()) {
    const std::size_t baseline = live.size();
    ERR_clear_error_nid_postfix();
    Require(ReadOne(Hex(der), nullptr) == nullptr, location);
    ExpectErrors(reasons, location);
    Require(live.size() == baseline, location);
}

void TestIntegerContents() {
    ExpectModulus("3006020105020103", {0x05}, false);
    ExpectModulus("3006020100020103", {}, false);
    ExpectModulus("300702020080020103", {0x80}, false);
    ExpectModulus("30060201ff020103", {0x01}, true);
    ExpectModulus("3006020180020103", {0x80}, true);
    ExpectModulus("30070202ff00020103", {0x01, 0x00}, true);
    ExpectModulus("30070202ff7f020103", {0x81}, true);
    ExpectModulus("3008020300ff01020103", {0xff, 0x01}, false);
    ExpectRejected("30070202007f020103", {ErrRAsn1LengthMismatch, ErrRNestedAsn1Error});
    ExpectRejected("30070202ff80020103", {ErrRAsn1LengthMismatch, ErrRNestedAsn1Error});
    ExpectRejected("300702020000020103", {ErrRAsn1LengthMismatch, ErrRNestedAsn1Error});
    ExpectRejected("30050200020103", {ErrRAsn1LengthMismatch, ErrRNestedAsn1Error});
    ExpectRejected("3006020105020000", {ErrRAsn1LengthMismatch, ErrRNestedAsn1Error});
}

void TestBerHeaders() {
    ExpectModulus("300702810105020103", {0x05}, false);
    ExpectModulus("30080282000105020103", {0x05}, false);
    ExpectModulus("30071f020105020103", {0x05}, false);
    ExpectModulus("30081f80020105020103", {0x05}, false);
}

void TestStructureErrors() {
    ExpectRejected("3103020101", {Asn1RWrongTag, ErrRNestedAsn1Error});
    ExpectRejected("7003020101", {Asn1RWrongTag, ErrRNestedAsn1Error});
    ExpectRejected("1006020105020103", {Asn1RSequenceNotConstructed});
    ExpectRejected("3000", {Asn1RFieldMissing});
    ExpectRejected("3003020105", {Asn1RFieldMissing});
    ExpectRejected("3009020105020103020107", {Asn1RSequenceLengthMismatch});
    ExpectRejected("30080000020105020103", {Asn1RUnexpectedEoc});
    ExpectRejected("30050201050000", {Asn1RUnexpectedEoc});
    ExpectRejected("300402010500", {Asn1RWrongTag, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("30082203020105020103", {Asn1RTypeNotPrimitive, ErrRNestedAsn1Error});
    ExpectRejected("3006040105020103", {Asn1RWrongTag, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("3006420105020103", {Asn1RWrongTag, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("3006020505020103", {Asn1RTooLong, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("30050280050000", {Asn1RBadObjectHeader, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("300502ff010201", {Asn1RBadObjectHeader, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("30020282", {Asn1RBadObjectHeader, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("30021f82", {Asn1RBadObjectHeader, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
    ExpectRejected("30071f8f8f8f8f8f7f", {Asn1RBadObjectHeader, ErrRNestedAsn1Error, ErrRNestedAsn1Error});
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
    TestNativeFree();
    TestParentFree();
    TestPkeyReferenceRelease();
    TestDecodeMatchesSubjectPublicKeyInfo();
    TestConsecutiveKeys();
    TestOutputReplacement();
    TestIntegerContents();
    TestBerHeaders();
    TestStructureErrors();
    return 0;
}
