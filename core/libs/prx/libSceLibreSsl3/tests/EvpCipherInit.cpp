#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

int calls;
int lastEncrypt;
const unsigned char* lastKey;
const unsigned char* lastIv;

int APS5_VABI Init(LibreSslEvpCipherCtx*, const unsigned char* key, const unsigned char* iv, int encrypt) {
    ++calls;
    lastEncrypt = encrypt;
    lastKey = key;
    lastIv = iv;
    return 1;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_EncryptInit check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    LibreSslEvpCipher cipher{};
    cipher.blockSize = 16;
    cipher.keyLength = 16;
    cipher.ivLength = 0;
    cipher.init = Init;
    unsigned char key[16]{};
    unsigned char iv[16]{};
    LibreSslEvpCipherCtx context{};
    Require(EVP_EncryptInit_nid_postfix(&context, &cipher, key, iv) == 1);
    Require(calls == 1 && lastEncrypt == 1 && lastKey == key && lastIv == iv);
    Require(context.cipher == &cipher && context.encrypt == 1);

    LibreSslEvpCipherCtx empty{};
    Require(EVP_EncryptInit_nid_postfix(&empty, nullptr, nullptr, nullptr) == 0);

    bool threw = false;
    try {
        EVP_EncryptInit_nid_postfix(nullptr, &cipher, key, iv);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    std::puts("libSceLibreSsl3 EVP_EncryptInit tests passed");
}
