#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
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

int main() {
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

    std::puts("libSceLibreSsl3 digest tests passed");
}
