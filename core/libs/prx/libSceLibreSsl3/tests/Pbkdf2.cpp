#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 PKCS5_PBKDF2_HMAC check failed at line %u\n", location.line());
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

std::string Hex(const std::vector<unsigned char>& bytes) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (const unsigned char byte : bytes) {
        text += Digits[byte >> 4];
        text += Digits[byte & 15];
    }
    return text;
}

std::string Derive(const EvpMd* digest, std::string_view password, int passwordLength, std::string_view salt, int iterations,
                   int keyLength) {
    std::vector<unsigned char> output(static_cast<std::size_t>(keyLength) + 1, 0xA5);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password.data(), passwordLength, reinterpret_cast<const unsigned char*>(salt.data()),
                                          static_cast<int>(salt.size()), iterations, digest, keyLength, output.data()) == 1);
    Require(output.back() == 0xA5);
    output.pop_back();
    return Hex(output);
}

std::string Derive(const EvpMd* digest, std::string_view password, std::string_view salt, int iterations, int keyLength) {
    return Derive(digest, password, static_cast<int>(password.size()), salt, iterations, keyLength);
}

}

int main() {
    InitializeHeap();
    const auto* sha256 = EVP_sha256_nid_postfix();
    Require(Derive(sha256, "passwd", "salt", 1, 64) ==
            "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
            "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    Require(Derive(sha256, "Password", "NaCl", 80000, 64) ==
            "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
            "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
    Require(Derive(sha256, "password", "salt", 2, 20) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8e");
    Require(Derive(sha256, "password", -1, "salt", 2, 20) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8e");
    Require(Derive(sha256, "", "salt", 1, 32) == "f135c27993baf98773c5cdb40a5706ce6a345cde61b000a67858650cd6a324d7");
    Require(Derive(sha256, "password", "salt", 0, 20) == Derive(sha256, "password", "salt", 1, 20));
    Require(Derive(EVP_sha512_nid_postfix(), "password", "salt", 3, 100) ==
            "b6b07cb2cebf4ad84468391a543824fccffe0e0769dbe6bddf10a65673c4b648"
            "e612d44918f9ce9a19a1294cf5140628084ba994c3b21a4ef4741220b811c633"
            "cfc0641fccbcc4164f1bbfcb1f33f595ae9aa4a33ddcce570157775980362c0e"
            "e28aa340");
    Require(Derive(EVP_md5_nid_postfix(), "pass", "", 5, 37) ==
            "56915dc521d153d2d8c8fafa462a145e22a4433cc1d56ff7f5c645f59a2d372c6c5ee27ea7");
    const std::string longPassword(200, 'k');
    Require(Derive(EVP_sha384_nid_postfix(), longPassword, "saltSALT", 4, 48) ==
            "0710ab30b60309eef4f7e690e0e07d1374f370ef4e0b665ffc929bd9833e3aa25a8b3ab09e41cf0a28ca8f8fe178a414");
    Require(Derive(sha256, "password", "salt", 1, 0).empty());
    unsigned char untouched[4]{1, 2, 3, 4};
    const auto* salt = reinterpret_cast<const unsigned char*>("salt");
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(nullptr, 8, salt, 4, 1, sha256, 4, untouched) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("password", -2, salt, 4, 1, sha256, 4, untouched) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("password", -5, salt, 4, 1, sha256, -1, nullptr) == 0);
    Require(untouched[0] == 1 && untouched[1] == 2 && untouched[2] == 3 && untouched[3] == 4);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("password", 8, salt, 4, 1, nullptr, 4, untouched) == 0);
    Require(ERR_get_error_nid_postfix() == std::uint64_t{0x06fff09f});
    Require(ERR_get_error_nid_postfix() == 0);
    Require(untouched[0] == 1 && untouched[1] == 2 && untouched[2] == 3 && untouched[3] == 4);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("password", 8, nullptr, -3, 1, sha256, 0, nullptr) == 1);
}
