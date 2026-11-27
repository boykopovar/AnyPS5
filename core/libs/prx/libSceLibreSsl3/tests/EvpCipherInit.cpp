#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void TestRsaSet0ConstTimeFlags() {
    constexpr int StaticData = 2;
    constexpr int ConstTime = 4;

    LibreSsl3Bignum n{};
    n.flags = StaticData;
    LibreSsl3Bignum e{};
    e.flags = StaticData;
    LibreSsl3Bignum d{};
    d.flags = StaticData;
    LibreSsl3Bignum p{};
    p.flags = StaticData;
    LibreSsl3Bignum q{};
    q.flags = StaticData;
    LibreSsl3Bignum dmp1{};
    dmp1.flags = StaticData;
    LibreSsl3Bignum dmq1{};
    dmq1.flags = StaticData;
    LibreSsl3Bignum iqmp{};
    iqmp.flags = StaticData;

    LibreSsl3Rsa rsa{};
    Require(RSA_set0_key_nid_postfix(&rsa, &n, &e, &d) == 1);
    Require(rsa.n == &n && rsa.e == &e && rsa.d == &d);
    Require(n.flags == StaticData && e.flags == StaticData);
    Require(d.flags == (StaticData | ConstTime));

    Require(RSA_set0_factors_nid_postfix(&rsa, &p, &q) == 1);
    Require(rsa.p == &p && rsa.q == &q);
    Require(p.flags == (StaticData | ConstTime));
    Require(q.flags == (StaticData | ConstTime));

    Require(RSA_set0_crt_params_nid_postfix(&rsa, &dmp1, &dmq1, &iqmp) == 1);
    Require(rsa.dmp1 == &dmp1 && rsa.dmq1 == &dmq1 && rsa.iqmp == &iqmp);
    Require(dmp1.flags == (StaticData | ConstTime));
    Require(dmq1.flags == (StaticData | ConstTime));
    Require(iqmp.flags == (StaticData | ConstTime));

    LibreSsl3Bignum dReplacement{};
    dReplacement.flags = StaticData;
    Require(RSA_set0_key_nid_postfix(&rsa, nullptr, nullptr, &dReplacement) == 1);
    Require(rsa.n == &n && rsa.e == &e && rsa.d == &dReplacement);
    Require(n.flags == StaticData && e.flags == StaticData);
    Require(d.flags == 0);
    Require(dReplacement.flags == (StaticData | ConstTime));
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

    LibreSslEvpCipherCtx stale;
    std::memset(&stale, 0xa5, sizeof(stale));
    Require(EVP_EncryptInit_nid_postfix(&stale, &cipher, key, iv) == 1);
    Require(calls == 2 && stale.cipher == &cipher && stale.encrypt == 1 && stale.engine == nullptr);

    LibreSslEvpCipherCtx empty{};
    Require(EVP_EncryptInit_nid_postfix(&empty, nullptr, nullptr, nullptr) == 0);

    LibreSslEvpCipher cbcCipher{};
    cbcCipher.nid = 419;
    cbcCipher.blockSize = 16;
    cbcCipher.keyLength = 16;
    cbcCipher.ivLength = 16;
    cbcCipher.flags = CipherModeCbc;
    cbcCipher.init = Init;
    unsigned char initialIv[16]{};
    unsigned char newKey[16]{};
    unsigned char newIv[16]{};
    unsigned char thirdKey[16]{};
    std::memset(initialIv, 0x11, sizeof(initialIv));
    std::memset(newKey, 0x22, sizeof(newKey));
    std::memset(newIv, 0x44, sizeof(newIv));
    std::memset(thirdKey, 0x66, sizeof(thirdKey));

    LibreSslEvpCipherCtx cbcContext{};
    Require(EVP_EncryptInit_nid_postfix(&cbcContext, &cbcCipher, key, initialIv) == 1);
    Require(calls == 3 && lastEncrypt == 1 && lastKey == key && lastIv == initialIv);
    Require(cbcContext.cipher == &cbcCipher && cbcContext.encrypt == 1 && cbcContext.keyLen == 16 && cbcContext.blockMask == 15);

    unsigned char cipherState[8]{};
    cbcContext.cipherData = cipherState;
    cbcContext.appData = &cbcCipher;
    cbcContext.flags = 0x42;
    cbcContext.encrypt = 0;
    cbcContext.bufLen = 7;
    cbcContext.finalUsed = 1;
    cbcContext.blockMask = 0;
    std::memset(cbcContext.iv, 0x77, sizeof(cbcContext.iv));

    Require(EVP_EncryptInit_nid_postfix(&cbcContext, nullptr, newKey, newIv) == 1);
    Require(calls == 4 && lastEncrypt == 1 && lastKey == newKey && lastIv == newIv);
    Require(cbcContext.cipher == &cbcCipher && cbcContext.engine == nullptr && cbcContext.encrypt == 1);
    Require(cbcContext.keyLen == 16 && cbcContext.flags == 0x42);
    Require(cbcContext.appData == &cbcCipher && cbcContext.cipherData == cipherState);
    Require(std::memcmp(cbcContext.oiv, newIv, sizeof(newIv)) == 0);
    Require(std::memcmp(cbcContext.iv, newIv, sizeof(newIv)) == 0);
    Require(cbcContext.bufLen == 0 && cbcContext.finalUsed == 0 && cbcContext.blockMask == 15);

    cbcContext.bufLen = 5;
    cbcContext.finalUsed = 1;
    cbcContext.blockMask = 0;
    std::memset(cbcContext.iv, 0x33, sizeof(cbcContext.iv));

    Require(EVP_EncryptInit_nid_postfix(&cbcContext, nullptr, thirdKey, nullptr) == 1);
    Require(calls == 5 && lastEncrypt == 1 && lastKey == thirdKey && lastIv == nullptr);
    Require(cbcContext.cipher == &cbcCipher && cbcContext.engine == nullptr && cbcContext.encrypt == 1);
    Require(cbcContext.keyLen == 16 && cbcContext.flags == 0x42);
    Require(cbcContext.appData == &cbcCipher && cbcContext.cipherData == cipherState);
    Require(std::memcmp(cbcContext.oiv, newIv, sizeof(newIv)) == 0);
    Require(std::memcmp(cbcContext.iv, newIv, sizeof(newIv)) == 0);
    Require(cbcContext.bufLen == 0 && cbcContext.finalUsed == 0 && cbcContext.blockMask == 15);

    bool threw = false;
    try {
        EVP_EncryptInit_nid_postfix(nullptr, &cipher, key, iv);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    TestRsaSet0ConstTimeFlags();
    std::puts("libSceLibreSsl3 EVP_EncryptInit tests passed");
}