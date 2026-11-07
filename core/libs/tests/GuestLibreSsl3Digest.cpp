#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 digest check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; } catch (const std::exception&) {}
    Require(false, location);
}

static std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free(void* memory) {
    std::free(memory);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate(void* memory, std::size_t size) {
    return std::realloc(memory, size);
}

void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) {
    return std::realloc(memory, size);
}

int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    const auto* abc = reinterpret_cast<const unsigned char*>("abc");
    const std::string million(1000000, 'a');
    const auto* many = reinterpret_cast<const unsigned char*>(million.data());
    unsigned char digest[24];
    std::memset(digest, 0xee, sizeof(digest));
    Require(MD5_nid_postfix(abc, 3, digest) == digest && Hex(digest, 16) == "900150983cd24fb0d6963f7d28e17f72" && digest[16] == 0xee);
    Require(MD5_nid_postfix(nullptr, 0, digest) == digest && Hex(digest, 16) == "d41d8cd98f00b204e9800998ecf8427e");
    Require(MD5_nid_postfix(many, million.size(), digest) == digest && Hex(digest, 16) == "7707d6ae4e027c70eea2a935c2296f21");
    std::memset(digest, 0xee, sizeof(digest));
    Require(SHA1_nid_postfix(abc, 3, digest) == digest && Hex(digest, 20) == "a9993e364706816aba3e25717850c26c9cd0d89d" && digest[20] == 0xee);
    Require(SHA1_nid_postfix(nullptr, 0, digest) == digest && Hex(digest, 20) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    Require(SHA1_nid_postfix(many, million.size(), digest) == digest && Hex(digest, 20) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    RequireInvalidArgument([&] { MD5_nid_postfix(abc, 3, nullptr); });
    RequireInvalidArgument([&] { SHA1_nid_postfix(abc, 3, nullptr); });
    RequireInvalidArgument([&] { SHA1_nid_postfix(nullptr, 1, digest); });

    Require(EVP_MD_block_size_nid_postfix(EVP_md5_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha1_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha224_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha256_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha384_nid_postfix()) == 128);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha512_nid_postfix()) == 128);
    Require(EVP_MD_block_size_nid_postfix(nullptr) == -1);

    unsigned char mac[32];
    HmacContext* hmac = HMAC_CTX_new_nid_postfix();
    Require(hmac != nullptr);
    Require(HMAC_Init_ex_nid_postfix(hmac, "Jefe", 4, EVP_sha256_nid_postfix(), nullptr) == 1);
    Require(HMAC_Update_nid_postfix(hmac, reinterpret_cast<const unsigned char*>("what do ya want for nothing?"), 28) == 1);
    std::uint32_t macLength = 0;
    Require(HMAC_Final_nid_postfix(hmac, mac, &macLength) == 1 && macLength == 32);
    Require(Hex(mac, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    HMAC_CTX_free_nid_postfix(hmac);
    HMAC_CTX_free_nid_postfix(nullptr);

    unsigned char derived[64];
    const auto* password = "password";
    const auto* salt = reinterpret_cast<const unsigned char*>("salt");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix()), 20, derived) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, reinterpret_cast<const EvpMd*>(EVP_sha224_nid_postfix()), 28, derived) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, -1, salt, 4, 2, EVP_sha256_nid_postfix(), 32, derived) == 1);
    Require(Hex(derived, 32) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 4096, EVP_sha256_nid_postfix(), 32, derived) == 1);
    Require(Hex(derived, 32) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, EVP_sha256_nid_postfix(), 32, derived) == 1);
    Require(Hex(derived, 32) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 2, EVP_sha256_nid_postfix(), 40, derived) == 1);
    Require(Hex(derived, 40) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43830651afcb5c862f");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 0, EVP_sha256_nid_postfix(), 32, derived) == 1);
    Require(Hex(derived, 32) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, EVP_sha512_nid_postfix(), 64, derived) == 1);
    Require(Hex(derived, 64) == "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("passwordPASSWORDpassword", 24, reinterpret_cast<const unsigned char*>("saltSALTsaltSALTsaltSALTsaltSALTsalt"), 36, 4096, EVP_sha256_nid_postfix(), 40, derived) == 1);
    Require(Hex(derived, 40) == "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(nullptr, 0, nullptr, 0, 1, EVP_sha256_nid_postfix(), 20, derived) == 1);
    Require(Hex(derived, 20) == "f7ce0b653d2d72a4108cf5abe912ffdd777616db");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, EVP_sha256_nid_postfix(), 0, derived) == 1);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, nullptr, 20, derived) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, 8, salt, 4, 1, EVP_sha256_nid_postfix(), -1, derived) == 0);

    std::puts("libSceLibreSsl3 digest tests passed");
}
