#include <openssl/aes.h>
#include <openssl/des.h>
#include <openssl/err.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/ocsp.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cstddef>

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
int I2tAsn1Object(char* buffer, int length, const void* object) { return i2t_ASN1_OBJECT(buffer, length, const_cast<ASN1_OBJECT*>(static_cast<const ASN1_OBJECT*>(object))); }
unsigned long ErrGetError() { return ERR_get_error(); }
void ErrClearError() { ERR_clear_error(); }

}
