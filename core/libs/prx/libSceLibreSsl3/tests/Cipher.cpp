#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_CIPHER check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

LibreSsl3EvpCipher Filled() {
    LibreSsl3EvpCipher cipher;
    std::memset(&cipher, 0xA5, sizeof(cipher));
    return cipher;
}

LibreSsl3EvpCipher Aes256Cbc() {
    LibreSsl3EvpCipher cipher {};
    cipher.nid = 427;
    cipher.blockSize = 16;
    cipher.keyLength = 32;
    cipher.ivLength = 16;
    cipher.flags = 2;
    cipher.contextSize = 264;
    return cipher;
}

template<typename TRead> void CheckField(TRead read, std::int32_t LibreSsl3EvpCipher::* field) {
    for (std::int32_t value : {0, 1, 8, 12, 16, 24, 32, 64, -1, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()}) {
        LibreSsl3EvpCipher cipher = Filled();
        cipher.*field = value;
        LibreSsl3EvpCipher before;
        std::memcpy(&before, &cipher, sizeof(cipher));
        Require(read(&cipher) == value);
        Require(std::memcmp(&before, &cipher, sizeof(cipher)) == 0);
    }
    RequireInvalidArgument([&] { read(nullptr); });
}

void TestKeyLength() {
    CheckField(EVP_CIPHER_key_length_nid_postfix, &LibreSsl3EvpCipher::keyLength);
    const LibreSsl3EvpCipher cipher = Aes256Cbc();
    Require(EVP_CIPHER_key_length_nid_postfix(&cipher) == 32);
}

void TestIvLength() {
    CheckField(EVP_CIPHER_iv_length_nid_postfix, &LibreSsl3EvpCipher::ivLength);
    const LibreSsl3EvpCipher cipher = Aes256Cbc();
    Require(EVP_CIPHER_iv_length_nid_postfix(&cipher) == 16);
}

void TestSslCipherId() {
    Require(SSL_CIPHER_get_id_nid_postfix(nullptr) == 0);
    LibreSsl3Cipher cipher {};
    for (const std::uint64_t id : {0ULL, 1ULL, 0x03000035ULL, 0x0300c02fULL, 0x103000035ULL, 0xffffffffffffffffULL}) {
        cipher.id = id;
        LibreSsl3Cipher before;
        std::memcpy(&before, &cipher, sizeof(cipher));
        Require(SSL_CIPHER_get_id_nid_postfix(&cipher) == id);
        Require(std::memcmp(&before, &cipher, sizeof(cipher)) == 0);
    }
    std::memset(&cipher, 0x5A, sizeof(cipher));
    cipher.id = 0x0300c02fULL;
    cipher.valid = 0x1;
    cipher.keyExchange = 0x80ULL;
    cipher.authentication = 0x40ULL;
    cipher.encryption = 0x20ULL;
    cipher.mac = 0x10ULL;
    cipher.protocol = 0x08ULL;
    cipher.strengthFlags = 0x04ULL;
    cipher.extraFlags = 0x02ULL;
    cipher.strengthBits = 256;
    cipher.algorithmBits = 128;
    Require(SSL_CIPHER_get_id_nid_postfix(&cipher) == 0x0300c02fULL);
}

}

int main() {
    TestKeyLength();
    TestIvLength();
    TestSslCipherId();
    std::puts("LibreSSL3 EVP_CIPHER tests passed");
}
