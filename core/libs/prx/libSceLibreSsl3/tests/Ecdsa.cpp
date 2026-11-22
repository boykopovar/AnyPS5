#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_ecdsa check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes ? bytes : 1); }
void APS5_VABI Release(void* block) { std::free(block); }
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return std::calloc(count ? count : 1, bytes ? bytes : 1); }
void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::string Hex(const unsigned char* bytes, std::size_t size) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += Digits[bytes[i] >> 4];
        text += Digits[bytes[i] & 15];
    }
    return text;
}

std::string Digest(std::string_view message) {
    unsigned char digest[64];
    for (auto& byte : digest) byte = 0xA5;
    std::uint32_t size = 0;
    Require(EVP_Digest_nid_postfix(message.data(), message.size(), digest, &size, EVP_ecdsa_nid_postfix(), nullptr) == 1);
    Require(size == 20);
    for (std::size_t i = 20; i < sizeof(digest); ++i) Require(digest[i] == 0xA5);
    return Hex(digest, size);
}

std::string Streamed(std::string_view message, std::size_t chunk) {
    EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
    Require(context != nullptr);
    Require(EVP_DigestInit_ex_nid_postfix(context, EVP_ecdsa_nid_postfix(), nullptr) == 1);
    for (std::size_t offset = 0; offset < message.size(); offset += chunk)
        Require(EVP_DigestUpdate_nid_postfix(context, message.data() + offset, std::min(chunk, message.size() - offset)) == 1);
    unsigned char digest[20];
    std::uint32_t size = 0;
    Require(EVP_DigestFinal_ex_nid_postfix(context, digest, &size) == 1);
    Require(size == 20);
    EVP_MD_CTX_destroy_nid_postfix(context);
    return Hex(digest, size);
}

using SignMethod = int (APS5_VABI*)(int, const unsigned char*, unsigned int, unsigned char*, unsigned int*, void*);
using VerifyMethod = int (APS5_VABI*)(int, const unsigned char*, unsigned int, const unsigned char*, unsigned int, void*);

void SignsAndVerifiesThroughSlots(const EvpMd* md) {
    Require(md->sign != nullptr && md->verify != nullptr);
    const auto sign = reinterpret_cast<SignMethod>(md->sign);
    const auto verify = reinterpret_cast<VerifyMethod>(md->verify);
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(key != nullptr && EC_KEY_generate_key_nid_postfix(key) == 1);
    unsigned char digest[20];
    for (std::size_t index = 0; index < sizeof(digest); ++index) digest[index] = static_cast<unsigned char>(index * 11 + 3);
    std::vector<unsigned char> signature(static_cast<std::size_t>(ECDSA_size_nid_postfix(key)));
    unsigned int length = 0;
    Require(sign(md->type, digest, sizeof(digest), signature.data(), &length, key) == 1);
    Require(length > 0 && length <= signature.size());
    Require(verify(md->type, digest, sizeof(digest), signature.data(), length, key) == 1);
    Require(ECDSA_verify_nid_postfix(md->type, digest, static_cast<int>(sizeof(digest)), signature.data(), static_cast<int>(length), key) == 1);
    digest[0] ^= 1;
    Require(verify(md->type, digest, sizeof(digest), signature.data(), length, key) == 0);
    EC_KEY_free_nid_postfix(key);
}

}

int main() {
    InitializeHeap();
    const auto* md = EVP_ecdsa_nid_postfix();
    Require(md == EVP_ecdsa_nid_postfix());
    Require(md->type == 416 && md->pkeyType == 416 && md->mdSize == 20 && md->flags == 0x2);
    Require(md->copy == nullptr && md->cleanup == nullptr && md->control == nullptr);
    SignsAndVerifiesThroughSlots(md);
    Require(md->requiredPkeyType[0] == 408);
    Require(std::all_of(md->requiredPkeyType + 1, md->requiredPkeyType + 5, [](std::int32_t type) { return type == 0; }));
    Require(md->blockSize == 64 && md->contextSize == 104);
    Require(EVP_MD_size_nid_postfix(md) == 20);
    Require(Digest("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    Require(Digest("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    const std::string twoBlocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    Require(Digest(twoBlocks) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    const std::string thousand(1000, 'a');
    Require(Digest(thousand) == "291e9a6c66994949b57ba5e650361e98fc36b1ba");
    Require(Streamed(thousand, 7) == "291e9a6c66994949b57ba5e650361e98fc36b1ba");
}
