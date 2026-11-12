#include <openssl/aes.h>
#include <openssl/conf.h>
#include <openssl/des.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/ocsp.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cstddef>
#include <cstring>
#include <string>

namespace LibreSslReference {

std::size_t AesKeySize() { return sizeof(AES_KEY); }
int AesSetEncryptKey(const unsigned char* userKey, int bits, void* key) { return AES_set_encrypt_key(userKey, bits, static_cast<AES_KEY*>(key)); }
int AesSetDecryptKey(const unsigned char* userKey, int bits, void* key) { return AES_set_decrypt_key(userKey, bits, static_cast<AES_KEY*>(key)); }
void AesEncrypt(const unsigned char* in, unsigned char* out, const void* key) { AES_encrypt(in, out, static_cast<const AES_KEY*>(key)); }
void AesDecrypt(const unsigned char* in, unsigned char* out, const void* key) { AES_decrypt(in, out, static_cast<const AES_KEY*>(key)); }
void AesEcb(const unsigned char* in, unsigned char* out, const void* key, int encrypt) { AES_ecb_encrypt(in, out, static_cast<const AES_KEY*>(key), encrypt); }
void AesCfb128(const unsigned char* in, unsigned char* out, std::size_t length, const void* key, unsigned char* ivec, int* num, int encrypt) { AES_cfb128_encrypt(in, out, length, static_cast<const AES_KEY*>(key), ivec, num, encrypt); }
void AesCtr128(const unsigned char* in, unsigned char* out, std::size_t length, const void* key, unsigned char* ivec, unsigned char* counter, unsigned* num) { AES_ctr128_encrypt(in, out, length, static_cast<const AES_KEY*>(key), ivec, counter, num); }

std::size_t DigestContextSize(int kind) {
    switch (kind) {
    case 0: return sizeof(MD4_CTX);
    case 1: return sizeof(MD5_CTX);
    case 2: return sizeof(SHA_CTX);
    case 3:
    case 4: return sizeof(SHA256_CTX);
    default: return sizeof(SHA512_CTX);
    }
}

int DigestInit(int kind, void* context) {
    switch (kind) {
    case 0: return MD4_Init(static_cast<MD4_CTX*>(context));
    case 1: return MD5_Init(static_cast<MD5_CTX*>(context));
    case 2: return SHA1_Init(static_cast<SHA_CTX*>(context));
    case 3: return SHA224_Init(static_cast<SHA256_CTX*>(context));
    case 4: return SHA256_Init(static_cast<SHA256_CTX*>(context));
    case 5: return SHA384_Init(static_cast<SHA512_CTX*>(context));
    default: return SHA512_Init(static_cast<SHA512_CTX*>(context));
    }
}

int DigestUpdate(int kind, void* context, const void* data, std::size_t length) {
    switch (kind) {
    case 0: return MD4_Update(static_cast<MD4_CTX*>(context), data, length);
    case 1: return MD5_Update(static_cast<MD5_CTX*>(context), data, length);
    case 2: return SHA1_Update(static_cast<SHA_CTX*>(context), data, length);
    case 3: return SHA224_Update(static_cast<SHA256_CTX*>(context), data, length);
    case 4: return SHA256_Update(static_cast<SHA256_CTX*>(context), data, length);
    case 5: return SHA384_Update(static_cast<SHA512_CTX*>(context), data, length);
    default: return SHA512_Update(static_cast<SHA512_CTX*>(context), data, length);
    }
}

int DigestFinal(int kind, unsigned char* digest, void* context) {
    switch (kind) {
    case 0: return MD4_Final(digest, static_cast<MD4_CTX*>(context));
    case 1: return MD5_Final(digest, static_cast<MD5_CTX*>(context));
    case 2: return SHA1_Final(digest, static_cast<SHA_CTX*>(context));
    case 3: return SHA224_Final(digest, static_cast<SHA256_CTX*>(context));
    case 4: return SHA256_Final(digest, static_cast<SHA256_CTX*>(context));
    case 5: return SHA384_Final(digest, static_cast<SHA512_CTX*>(context));
    default: return SHA512_Final(digest, static_cast<SHA512_CTX*>(context));
    }
}

unsigned char* Sha256(const unsigned char* data, std::size_t length, unsigned char* digest) { return SHA256(data, length, digest); }
unsigned char* Md5(const unsigned char* data, std::size_t length, unsigned char* digest) { return MD5(data, length, digest); }
unsigned char* Sha1(const unsigned char* data, std::size_t length, unsigned char* digest) { return SHA1(data, length, digest); }

void DesSetOddParity(unsigned char* key) { DES_set_odd_parity(reinterpret_cast<DES_cblock*>(key)); }

const char* X509VerifyCertErrorString(long error) { return X509_verify_cert_error_string(error); }
const char* SslAlertDescStringLong(int value) { return SSL_alert_desc_string_long(value); }
const char* OcspCertStatusString(long status) { return OCSP_cert_status_str(status); }
const char* OcspCrlReasonString(long reason) { return OCSP_crl_reason_str(reason); }
const char* OcspResponseStatusString(long status) { return OCSP_response_status_str(status); }
int ObjCreate(const char* oid, const char* shortName, const char* longName) { return OBJ_create(oid, shortName, longName); }
void ObjCleanup() { OBJ_cleanup(); }
void AddAllAlgorithms() { OPENSSL_add_all_algorithms_noconf(); }
void LoadBuiltinModules() { OPENSSL_load_builtin_modules(); }
void ConfModulesFree() { CONF_modules_free(); }
int I2tAsn1Object(char* buffer, int length, const void* object) { return i2t_ASN1_OBJECT(buffer, length, const_cast<ASN1_OBJECT*>(static_cast<const ASN1_OBJECT*>(object))); }
unsigned long ErrGetError() { return ERR_get_error(); }
void ErrClearError() { ERR_clear_error(); }

std::size_t MdContextSize() { return sizeof(EVP_MD_CTX); }
void MdContextInit(void* context) { EVP_MD_CTX_init(static_cast<EVP_MD_CTX*>(context)); }
int MdContextCleanup(void* context) { return EVP_MD_CTX_cleanup(static_cast<EVP_MD_CTX*>(context)); }

void* DigestMethod(int kind) {
    switch (kind) {
    case 1: return const_cast<EVP_MD*>(EVP_md5());
    case 2: return const_cast<EVP_MD*>(EVP_sha1());
    case 3: return const_cast<EVP_MD*>(EVP_sha224());
    case 4: return const_cast<EVP_MD*>(EVP_sha256());
    case 5: return const_cast<EVP_MD*>(EVP_sha384());
    default: return const_cast<EVP_MD*>(EVP_sha512());
    }
}

void* GenerateRsa(int bits) { return RSA_generate_key(bits, 65537, nullptr, nullptr); }
void RsaFree(void* rsa) { RSA_free(static_cast<RSA*>(rsa)); }
int RsaSign(int type, const unsigned char* message, unsigned int messageLength, unsigned char* signature, unsigned int* signatureLength, void* rsa) {
    return RSA_sign(type, message, messageLength, signature, signatureLength, static_cast<RSA*>(rsa));
}
int I2dRsaPubKey(void* rsa, unsigned char** output) { return i2d_RSA_PUBKEY(static_cast<RSA*>(rsa), output); }
void ReferenceFree(void* pointer) { OPENSSL_free(pointer); }

void* PkeyNew() { return EVP_PKEY_new(); }
int PkeySet1Rsa(void* pkey, void* rsa) { return EVP_PKEY_set1_RSA(static_cast<EVP_PKEY*>(pkey), static_cast<RSA*>(rsa)); }
void PkeyFree(void* pkey) { EVP_PKEY_free(static_cast<EVP_PKEY*>(pkey)); }

int DigestUpdateContext(void* context, const void* data, std::size_t length) { return EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(context), data, length); }
int DigestVerifyInit(void* context, void** pctx, const void* digest, void* pkey) {
    return EVP_DigestVerifyInit(static_cast<EVP_MD_CTX*>(context), reinterpret_cast<EVP_PKEY_CTX**>(pctx),
        static_cast<const EVP_MD*>(digest), nullptr, static_cast<EVP_PKEY*>(pkey));
}
int DigestVerifyFinal(void* context, unsigned char* signature, std::size_t signatureLength) {
    return EVP_DigestVerifyFinal(static_cast<EVP_MD_CTX*>(context), signature, signatureLength);
}

int CipherListNames(const char* rules, char* buffer, int length) {
    static const int initialised = SSL_library_init();
    static_cast<void>(initialised);
    if (length > 0) buffer[0] = '\0';
    SSL_CTX* ctx = SSL_CTX_new(TLSv1_2_method());
    if (!ctx) return -1;
    const int result = SSL_CTX_set_cipher_list(ctx, rules);
    std::string names;
    STACK_OF(SSL_CIPHER)* ciphers = ctx->cipher_list;
    for (int index = 0; ciphers && index < sk_SSL_CIPHER_num(ciphers); ++index) {
        if (index) names += ':';
        names += SSL_CIPHER_get_name(sk_SSL_CIPHER_value(ciphers, index));
    }
    if (length > 0) {
        const std::size_t copied = names.size() < static_cast<std::size_t>(length - 1) ? names.size() : static_cast<std::size_t>(length - 1);
        std::memcpy(buffer, names.data(), copied);
        buffer[copied] = '\0';
    }
    SSL_CTX_free(ctx);
    return result;
}

int CipherFields(int index, void* fields, int length) {
    static const int initialised = SSL_library_init();
    static_cast<void>(initialised);
    SSL_CTX* ctx = SSL_CTX_new(TLSv1_2_method());
    if (!ctx) return -1;
    int copied = 0;
    if (SSL_CTX_set_cipher_list(ctx, "ALL:COMPLEMENTOFALL") == 1 && index >= 0 && index < sk_SSL_CIPHER_num(ctx->cipher_list)) {
        const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ctx->cipher_list, index);
        copied = length < static_cast<int>(sizeof(SSL_CIPHER)) ? length : static_cast<int>(sizeof(SSL_CIPHER));
        std::memcpy(fields, cipher, static_cast<std::size_t>(copied));
    }
    SSL_CTX_free(ctx);
    return copied;
}

}
