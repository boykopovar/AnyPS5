#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "EvpDigest.hpp"
#include "OcspCertId.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"
#include "prx/libSceLibreSsl3/include/Ssl.hpp"
#include "prx/libSceLibreSsl3/include/X509.hpp"
#include "prx/libSceLibreSsl3/include/X509V3Context.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Cipher.hpp"
#include "prx/libSceLibreSsl3/include/EcdsaSig.hpp"

using LibreSsl3KeylogCallback = void (APS5_VABI*)(const LibreSsl3Ssl* ssl, const char* line);

struct LibreSsl3Stack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

struct LibreSsl3ExtensionsCertInfo {
    unsigned char beforeExtensions[72];
    const LibreSsl3Stack* extensions;
    unsigned char rest[24];
};

struct LibreSsl3ExtensionsX509 {
    const LibreSsl3ExtensionsCertInfo* certInfo;
    unsigned char rest[208];
};

struct LibreSsl3Asn1String {
    int length;
    int type;
    unsigned char* data;
    std::int64_t flags;
};

inline constexpr int LibreSsl3Asn1Undefined = -1;

struct LibreSsl3Asn1Type {
    int type;
    void* value;
};

struct LibreSsl3X509Algor {
    void* algorithm;
    LibreSsl3Asn1Type* parameter;
};

struct LibreSsl3X509Pubkey {
    LibreSsl3X509Algor* algor;
    LibreSsl3Asn1String* publicKey;
    void* pkey;
};

static_assert(sizeof(LibreSsl3Asn1String) == 24);
static_assert(offsetof(LibreSsl3Asn1String, data) == 8);
static_assert(sizeof(LibreSsl3Asn1Type) == 16);
static_assert(offsetof(LibreSsl3Asn1Type, value) == 8);
static_assert(sizeof(LibreSsl3X509Algor) == 16);
static_assert(offsetof(LibreSsl3X509Algor, parameter) == 8);
static_assert(sizeof(LibreSsl3X509Pubkey) == 24);
static_assert(offsetof(LibreSsl3X509Pubkey, publicKey) == 8);

struct LibreSsl3CertificateInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    void* validity;
    void* subject;
    LibreSsl3X509Pubkey* key;
    void* issuerUid;
    void* subjectUid;
    const LibreSsl3Stack* extensions;
    unsigned char encoding[24];
};

struct LibreSsl3Certificate {
    const LibreSsl3CertificateInfo* certInfo;
    const LibreSsl3X509Algor* sigAlg;
    unsigned char rest[200];
};

struct LibreSsl3X509Extension {
    const LibreSslAsn1Object* object;
    int critical;
    void* value;
};

static_assert(sizeof(LibreSsl3CertificateInfo) == 104);
static_assert(offsetof(LibreSsl3CertificateInfo, key) == 48);
static_assert(offsetof(LibreSsl3CertificateInfo, extensions) == 72);
static_assert(sizeof(LibreSsl3Certificate) == 216);
static_assert(offsetof(LibreSsl3Certificate, sigAlg) == 8);
static_assert(sizeof(LibreSsl3X509Extension) == 24);

struct LibreSsl3Dh {
    int pad;
    int version;
    const void* p;
    const void* g;
    std::int64_t length;
    const void* pubKey;
    const void* privKey;
    int flags;
    void* methodMontP;
    const void* q;
    unsigned char rest[56];
};

struct LibreSsl3Dsa {
    int pad;
    std::int64_t version;
    const void* p;
    const void* q;
    const void* g;
    const void* pubKey;
    const void* privKey;
    unsigned char rest[56];
};

struct Asn1Object_nid_no_patch {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

static_assert(sizeof(Asn1Object_nid_no_patch) == 40);
static_assert(offsetof(Asn1Object_nid_no_patch, nid) == 16);
static_assert(offsetof(Asn1Object_nid_no_patch, length) == 20);
static_assert(offsetof(Asn1Object_nid_no_patch, data) == 24);
static_assert(offsetof(Asn1Object_nid_no_patch, flags) == 32);

struct LibreSsl3X509Validity {
    void* notBefore;
    void* notAfter;
};

struct LibreSsl3ValidityCertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    LibreSsl3X509Validity* validity;
    unsigned char rest[64];
};

struct LibreSsl3ValidityX509 {
    LibreSsl3ValidityCertInfo* certInfo;
    const void* sigAlg;
    const void* signature;
    unsigned char rest[192];
};

struct Asn1Type {
    std::int32_t type;
    std::int32_t padding;
    union {
        void* pointer;
        std::int32_t boolean;
    } value;
};

static_assert(sizeof(Asn1Type) == 16);

extern "C" {

void APS5_VABI ASN1_INTEGER_free_nid_postfix(Asn1String* integer);
void APS5_VABI ASN1_OBJECT_free_nid_postfix(Asn1Object_nid_no_patch* object);
const unsigned char* APS5_VABI ASN1_STRING_get0_data_nid_postfix(const Asn1String* string);
int APS5_VABI BIO_up_ref_nid_postfix(LibreSsl::Bio* bio);
int APS5_VABI AES_wrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, std::uint32_t inlen);
void APS5_VABI ASN1_TYPE_free_nid_postfix(Asn1Type* value);
LibreSsl3EcdsaSig* APS5_VABI ECDSA_SIG_new_nid_postfix(void);
int APS5_VABI ECDSA_SIG_set0_nid_postfix(LibreSsl3EcdsaSig* signature, Bignum* r, Bignum* s);
void APS5_VABI ECDSA_SIG_free_nid_postfix(LibreSsl3EcdsaSig* signature);
int APS5_VABI RSA_bits_nid_postfix(const LibreSsl3Rsa* rsa);
int APS5_VABI BN_is_negative_nid_postfix(const Bignum* value);
int APS5_VABI EVP_CIPHER_nid_nid_postfix(const LibreSsl3EvpCipher* cipher);
void APS5_VABI ERR_load_ERR_strings_nid_postfix();
int APS5_VABI BIO_vfree_nid_postfix(void);
int APS5_VABI CRYPTO_memcmp_nid_postfix(const void* first, const void* second, std::size_t length);
void APS5_VABI DH_get0_key_nid_postfix(const LibreSsl3Dh* dh, const void** pubKey, const void** privKey);
void APS5_VABI DH_get0_pqg_nid_postfix(const LibreSsl3Dh* dh, const void** p, const void** q, const void** g);
void APS5_VABI DSA_get0_key_nid_postfix(const LibreSsl3Dsa* dsa, const void** pubKey, const void** privKey);
void APS5_VABI DSA_get0_pqg_nid_postfix(const LibreSsl3Dsa* dsa, const void** p, const void** q, const void** g);
int APS5_VABI EC_KEY_up_ref_nid_postfix(LibreSsl3EcKey* key);
void APS5_VABI ERR_load_crypto_strings_nid_postfix();
std::uint64_t APS5_VABI ERR_peek_last_error_nid_postfix();
int APS5_VABI ERR_print_errors_cb_nid_postfix(void);
int APS5_VABI EVP_CIPHER_CTX_set_flags_nid_postfix(void);
int APS5_VABI EVP_DigestInit_nid_postfix(LibreSsl3::DigestContext* context, const LibreSsl3::DigestMethod* digest);
int APS5_VABI EVP_DigestSignFinal_nid_postfix(void);
int APS5_VABI EVP_DigestSignInit_nid_postfix(void);
int APS5_VABI EVP_EncryptInit_nid_postfix(LibreSslEvpCipherCtx* context, const LibreSslEvpCipher* cipher, const unsigned char* key, const unsigned char* iv);
void APS5_VABI EVP_MD_CTX_free_nid_postfix(LibreSsl3::DigestContext* context);
LibreSsl3::DigestContext* APS5_VABI EVP_MD_CTX_new_nid_postfix();
int APS5_VABI EVP_PKCS82PKEY_nid_postfix(void);
int APS5_VABI EVP_PKEY_CTX_ctrl_nid_postfix(void);
int APS5_VABI EVP_PKEY_CTX_new_nid_postfix(void);
int APS5_VABI EVP_PKEY_decrypt_nid_postfix(void);
int APS5_VABI EVP_PKEY_decrypt_init_nid_postfix(void);
int APS5_VABI EVP_PKEY_encrypt_nid_postfix(void);
int APS5_VABI EVP_PKEY_encrypt_init_nid_postfix(void);
void* APS5_VABI EVP_PKEY_get0_DH_nid_postfix(LibreSsl3EvpPkey* key);
void* APS5_VABI EVP_PKEY_get0_DSA_nid_postfix(LibreSsl3EvpPkey* key);
LibreSsl3Rsa* APS5_VABI EVP_PKEY_get0_RSA_nid_postfix(LibreSsl3EvpPkey* key);
LibreSsl3Rsa* APS5_VABI EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key);
LibreSsl3EvpPkey* APS5_VABI EVP_PKEY_new_nid_disambig1_nid_postfix(void);
void APS5_VABI EVP_PKEY_free_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key);
int APS5_VABI EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key, LibreSsl3Rsa* rsa);
int APS5_VABI EVP_PKEY_copy_parameters_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* to, const LibreSsl3EvpPkey* from);
int APS5_VABI EVP_PKEY_new_mac_key_nid_postfix(void);
int APS5_VABI EVP_PKEY_set1_EC_KEY_nid_postfix(LibreSsl3EvpPkey* key, LibreSsl3EcKey* ec);
int APS5_VABI EVP_PKEY_size_nid_postfix(const LibreSsl3EvpPkey* key);
int APS5_VABI EVP_PKEY_up_ref_nid_postfix(LibreSsl3EvpPkey* key);
int APS5_VABI EVP_SignFinal_nid_postfix(void);
int APS5_VABI EVP_VerifyFinal_nid_postfix(void);
const LibreSsl3::DigestMethod* APS5_VABI EVP_sha1_nid_postfix();
const LibreSsl3::DigestMethod* APS5_VABI EVP_sha224_nid_postfix();
int APS5_VABI OBJ_cmp_nid_postfix(const void* first, const void* second);
int APS5_VABI OBJ_txt2obj_nid_postfix(void);
void APS5_VABI OCSP_CERTID_free_nid_postfix(OCSP_CERTID* id);
int APS5_VABI OCSP_cert_to_id_nid_postfix(void);
int APS5_VABI OCSP_resp_find_status_nid_postfix(LibreSslOcspBasicResponse* bs, OCSP_CERTID* id, int* status, int* reason, Asn1String** revtime, Asn1String** thisupd, Asn1String** nextupd);
const char* APS5_VABI OpenSSL_version_nid_postfix(int type);
int APS5_VABI PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(void);
int APS5_VABI PKCS7_free_nid_postfix(void);
int APS5_VABI PKCS8_PRIV_KEY_INFO_free_nid_postfix(void);
int APS5_VABI RSA_check_key_nid_postfix(void);
void APS5_VABI RSA_get0_key_nid_postfix(const LibreSsl3Rsa* rsa, const void** n, const void** e, const void** d);
int APS5_VABI RSA_new_nid_disambig1_nid_postfix(void);
int APS5_VABI RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix(void);
int APS5_VABI RSA_up_ref_nid_postfix(LibreSsl3Rsa* rsa);
int APS5_VABI SSL_CTX_add0_chain_cert_nid_postfix(void);
int APS5_VABI SSL_CTX_clear_chain_certs_nid_postfix(void);
int APS5_VABI SSL_CTX_set_max_proto_version_nid_postfix(LibreSsl3SslCtx* context, std::uint16_t version);
int APS5_VABI SSL_CTX_set_min_proto_version_nid_postfix(LibreSsl3SslCtx* context, std::uint16_t version);
int APS5_VABI SSL_CTX_up_ref_nid_postfix(LibreSsl3SslCtx* ctx);
int APS5_VABI SSL_SESSION_up_ref_nid_postfix(LibreSsl3SslSession* session);
std::size_t APS5_VABI SSL_get_client_random_nid_postfix(const LibreSsl3Ssl* ssl, unsigned char* out, std::size_t maxOut);
int APS5_VABI SSL_up_ref_nid_postfix(LibreSsl3Ssl* ssl);
int APS5_VABI X509_PUBKEY_get0_param_nid_postfix(void** publicKeyAlgorithm, const unsigned char** publicKey, int* publicKeyLength, void** algorithm, const void* pub);
void APS5_VABI X509_STORE_CTX_free_nid_postfix(LibreSslX509StoreContext* context);
int APS5_VABI X509_STORE_CTX_init_nid_postfix(LibreSslX509StoreContext* context, LibreSslX509Store* store, LibreSsl3X509* leaf, LibreSslStack* untrusted);
LibreSslX509StoreContext* APS5_VABI X509_STORE_CTX_new_nid_postfix(void);
int APS5_VABI X509_STORE_up_ref_nid_postfix(LibreSslX509Store* store);
const LibreSsl3Stack* APS5_VABI X509_get0_extensions_nid_postfix(const LibreSsl3ExtensionsX509* certificate);
const void* APS5_VABI X509_get0_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate);
const void* APS5_VABI X509_get0_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate);
void APS5_VABI X509_get0_signature_nid_postfix(const void** signature, const void** algorithm, const LibreSsl3ValidityX509* certificate);
void* APS5_VABI X509_get_ext_nid_postfix(const LibreSsl3ExtensionsX509* certificate, int loc);
int APS5_VABI X509_get_ext_count_nid_postfix(const LibreSsl3ExtensionsX509* certificate);
std::int64_t APS5_VABI X509_get_version_nid_postfix(const LibreSsl3X509* certificate);
int APS5_VABI X509_set_version_nid_postfix(LibreSsl3X509* certificate, std::int64_t version);
void* APS5_VABI X509_getm_notAfter_nid_postfix(const LibreSsl3ValidityX509* certificate);
void* APS5_VABI X509_getm_notBefore_nid_postfix(const LibreSsl3ValidityX509* certificate);
int APS5_VABI X509_up_ref_nid_postfix(LibreSsl3X509* certificate);
void* APS5_VABI d2i_ASN1_INTEGER_nid_postfix(void** output, const unsigned char** input, std::int64_t length);
EcKey* APS5_VABI d2i_ECParameters_nid_postfix(EcKey** key, const unsigned char** in, std::int64_t length);
int APS5_VABI d2i_PKCS7_nid_postfix(void);
int APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(void);
int APS5_VABI d2i_PrivateKey_bio_nid_postfix(void);
int APS5_VABI d2i_RSAPublicKey_bio_nid_postfix(void);
int APS5_VABI d2i_RSA_PUBKEY_nid_disambig1_nid_postfix(void);
int APS5_VABI d2i_X509_bio_nid_postfix(void);
EcKey* APS5_VABI o2i_ECPublicKey_nid_postfix(EcKey** key, const unsigned char** in, std::int64_t length);
unsigned char* APS5_VABI MD5_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest);
unsigned char* APS5_VABI SHA1_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest);

void APS5_VABI BN_clear_free_nid_postfix(LibreSsl3Bignum* number);
void APS5_VABI BIO_clear_flags_nid_postfix(LibreSsl::Bio* bio, int flags);
void APS5_VABI BIO_set_flags_nid_postfix(LibreSsl::Bio* bio, int flags);
Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* source);
const char* APS5_VABI ERR_func_error_string_nid_postfix(std::uint64_t error);
int APS5_VABI EVP_CIPHER_iv_length_nid_postfix(const LibreSsl3EvpCipher* cipher);
int APS5_VABI EVP_CIPHER_key_length_nid_postfix(const LibreSsl3EvpCipher* cipher);
const void* APS5_VABI EVP_MD_CTX_md_nid_postfix(const void* context);
int APS5_VABI EVP_PKEY_CTX_free_nid_postfix();
int APS5_VABI EVP_PKEY_assign_nid_postfix(LibreSsl3EvpPkey* key, int type, void* data);
int APS5_VABI EVP_PKEY_bits_nid_postfix(const LibreSsl3EvpPkey* key);
int APS5_VABI EVP_PKEY_derive_set_peer_nid_postfix();
LibreSsl3EcKey* APS5_VABI EVP_PKEY_get0_EC_KEY_nid_postfix(LibreSsl3EvpPkey* key);
int APS5_VABI EVP_PKEY_verify_nid_postfix();
int APS5_VABI EVP_PKEY_verify_init_nid_postfix();
const void* APS5_VABI EVP_get_digestbyname_nid_postfix(const char* name);
const EvpMd* APS5_VABI EVP_sha512_256_nid_postfix();
int APS5_VABI HKDF_nid_postfix(std::uint8_t* outKey, std::size_t outLength, const EvpMd* digest, const std::uint8_t* secret, std::size_t secretLength, const std::uint8_t* salt, std::size_t saltLength, const std::uint8_t* info, std::size_t infoLength);
void APS5_VABI HMAC_CTX_free_nid_postfix(void* context);
void* APS5_VABI HMAC_CTX_new_nid_postfix();
int APS5_VABI OPENSSL_init_ssl_nid_postfix(std::uint64_t options, const void* settings);
int APS5_VABI PEM_read_bio_PUBKEY_nid_postfix();
std::uint64_t APS5_VABI SSL_CIPHER_get_id_nid_postfix(const LibreSsl3Cipher* cipher);
int APS5_VABI SSL_CTX_add1_chain_cert_nid_postfix();
int APS5_VABI SSL_CTX_set0_chain_nid_postfix();
std::int64_t APS5_VABI SSL_CTX_set_timeout_nid_postfix(LibreSsl3SslCtx* context, std::int64_t timeout);
std::int64_t APS5_VABI SSL_SESSION_get_timeout_nid_postfix(const void* session);
std::int64_t APS5_VABI SSL_SESSION_set_time_nid_postfix(void* session, std::int64_t time);
std::uint64_t APS5_VABI SSL_SESSION_get_master_key_nid_postfix(const void* session, unsigned char* out, std::uint64_t outLength);
int APS5_VABI SSL_do_handshake_nid_postfix();
const char* APS5_VABI SSL_get_version_nid_postfix(const LibreSsl3Ssl* ssl);
int APS5_VABI SSL_set_accept_state_nid_postfix();
int APS5_VABI SSL_set_alpn_protos_nid_postfix(LibreSsl3Ssl* ssl, const std::uint8_t* protos, std::uint32_t protosLength);
using LibreSsl3InfoCallback = void (APS5_VABI*)(const LibreSsl3Ssl* ssl, int type, int value);
void APS5_VABI SSL_set_info_callback_nid_postfix(LibreSsl3Ssl* ssl, LibreSsl3InfoCallback callback);
const LibreSsl3SslMethod* APS5_VABI TLS_method_nid_postfix();
const LibreSsl3SslMethod* APS5_VABI TLS_server_method_nid_postfix();
int APS5_VABI X509_EXTENSION_free_nid_postfix();
void* APS5_VABI X509_STORE_CTX_get0_param_nid_postfix(LibreSslX509StoreContext* context);
int APS5_VABI X509_STORE_load_locations_nid_postfix();
int APS5_VABI X509_VERIFY_PARAM_set_flags_nid_postfix(LibreSslX509VerifyParam* param, std::uint64_t flags);
void* APS5_VABI X509_get0_pubkey_nid_postfix(const LibreSsl3Certificate* certificate);
int APS5_VABI d2i_SSL_SESSION_nid_postfix();
int APS5_VABI d2i_X509_CRL_nid_postfix();
int APS5_VABI i2d_SSL_SESSION_nid_postfix();
int APS5_VABI i2d_X509_nid_postfix();
void APS5_VABI sk_free_nid_postfix(LibreSslStack* stack);
LibreSslStack* APS5_VABI sk_new_nid_postfix(StackCompare compare);
int APS5_VABI sk_push_nid_postfix(LibreSslStack* stack, void* element);
int APS5_VABI tls_read_nid_postfix();
int APS5_VABI tls_write_nid_postfix();
void APS5_VABI EC_KEY_set_asn1_flag_nid_postfix(EcKey* key, int flag);
void APS5_VABI BIO_set_init_nid_postfix(LibreSsl::Bio* bio, int init);
int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix();
int APS5_VABI SSL_SESSION_get_protocol_version_nid_postfix(const void* session);
int APS5_VABI X509_NAME_add_entry_by_txt_nid_postfix();
void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl::BioMethod* method);
int APS5_VABI tls_handshake_nid_postfix();
int APS5_VABI AUTHORITY_KEYID_free_nid_postfix();
int APS5_VABI X509V3_EXT_d2i_nid_postfix();
int APS5_VABI ASN1_INTEGER_set_nid_postfix(Asn1String* integer, std::int64_t value);
int APS5_VABI RSA_set0_factors_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* p, LibreSsl3Bignum* q);
int APS5_VABI X509_set_pubkey_nid_postfix();
int APS5_VABI BIO_get_shutdown_nid_postfix(LibreSsl::Bio* bio);
int APS5_VABI tls_accept_socket_nid_postfix();
int APS5_VABI ERR_print_errors_fp_nid_postfix();
int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::write) write);
int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::create) create);
int APS5_VABI X509_CRL_free_nid_postfix();
int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::read) read);
int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix();
int APS5_VABI SSL_CTX_set_max_early_data_nid_postfix(void* ctx, std::uint32_t maxEarlyData);
int APS5_VABI Unknown_A_minus_Hs_minus_8mau_plus_8_nid_no_patch();
int APS5_VABI ERR_load_X509_strings_nid_postfix();
int APS5_VABI X509_cmp_current_time_nid_postfix(const Asn1String* time);
int APS5_VABI X509_get_ext_by_NID_nid_postfix(const LibreSsl3Certificate* certificate, int nid, int lastpos);
int APS5_VABI EVP_PKEY_security_bits_nid_postfix(const LibreSsl3EvpPkey* key);
int APS5_VABI X509_check_akid_nid_postfix();
void APS5_VABI SSL_set_quic_use_legacy_codepoint_nid_postfix(void* ssl, int useLegacy);
int APS5_VABI SSL_CTX_set_default_verify_paths_nid_postfix();
const char* APS5_VABI ERR_lib_error_string_nid_postfix(std::uint64_t error);
int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::control) ctrl);
void APS5_VABI DES_set_key_unchecked_nid_postfix(const unsigned char* key, unsigned char* schedule);
int APS5_VABI EVP_PKEY_CTX_new_id_nid_postfix();
int APS5_VABI RSAPublicKey_dup_nid_postfix();
int APS5_VABI AES_unwrap_key_nid_postfix(AesKey* key, const unsigned char* iv, unsigned char* out, const unsigned char* in, unsigned int length);
int APS5_VABI ASN1_OCTET_STRING_cmp_nid_postfix(const Asn1String* first, const Asn1String* second);
int APS5_VABI X509_print_nid_postfix();
int APS5_VABI X509_CRL_get0_by_cert_nid_postfix();
int APS5_VABI ERR_load_PEM_strings_nid_postfix();
int APS5_VABI OpenSSL_add_all_digests_nid_postfix();
int APS5_VABI EVP_PKEY_base_id_nid_postfix(const LibreSsl3EvpPkey* key);
int APS5_VABI Unknown_PBYz0kpXwg8_nid_no_patch();
int APS5_VABI X509_new_nid_postfix();
void APS5_VABI RSA_get0_factors_nid_postfix(const LibreSsl3Rsa* rsa, const void** p, const void** q);
int APS5_VABI SSL_get0_verified_chain_nid_postfix();
int APS5_VABI RSA_set0_crt_params_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* dmp1, LibreSsl3Bignum* dmq1, LibreSsl3Bignum* iqmp);
int APS5_VABI EVP_PKEY_derive_nid_postfix();
int APS5_VABI EVP_PKEY_sign_nid_postfix();
int APS5_VABI X509_get_signature_nid_nid_postfix(const LibreSsl3Certificate* certificate);
int APS5_VABI EVP_PKEY_derive_init_nid_postfix();
int APS5_VABI X509V3_EXT_conf_nid_nid_postfix();
int APS5_VABI X509_sign_nid_postfix();
int APS5_VABI SSL_get_peer_quic_transport_params_nid_postfix();
void* APS5_VABI BIO_get_data_nid_postfix(LibreSsl::Bio* bio);
int APS5_VABI EVP_PKEY2PKCS8_nid_postfix();
Asn1String* APS5_VABI ASN1_TIME_new_nid_postfix();
int APS5_VABI X509_set_issuer_name_nid_postfix();
int APS5_VABI X509_add_ext_nid_postfix();
int APS5_VABI tls_connect_socket_nid_postfix();
int APS5_VABI SSL_CTX_set_quic_method_nid_postfix();
Asn1String* APS5_VABI X509_gmtime_adj_nid_postfix(Asn1String* time, std::int64_t adjustment);
int APS5_VABI SSL_process_quic_post_handshake_nid_postfix(void* ssl);
void APS5_VABI EC_POINT_clear_free_nid_postfix(EcPoint* point);
int APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_nid_postfix();
int APS5_VABI ECDSA_do_sign_nid_postfix();
int APS5_VABI SSL_CTX_set_ciphersuites_nid_postfix();
int APS5_VABI PEM_write_bio_PrivateKey_nid_postfix();
void APS5_VABI RSA_get0_crt_params_nid_postfix(const LibreSsl3Rsa* rsa, const void** dmp1, const void** dmq1, const void** iqmp);
void APS5_VABI BIO_set_data_nid_postfix(LibreSsl::Bio* bio, void* data);
void APS5_VABI SSL_CTX_set_keylog_callback_nid_postfix(LibreSsl3SslCtx* context, LibreSsl3KeylogCallback callback);
void APS5_VABI X509_ALGOR_get0_nid_postfix(const void** outObject, int* outType, const void** outValue, const LibreSsl3X509Algor* algor);
int APS5_VABI ASN1_STRING_set_nid_postfix(Asn1String* string, const void* data, int length);
void APS5_VABI BIO_set_shutdown_nid_postfix(LibreSsl::Bio* bio, int shutdown);
int APS5_VABI i2d_PKCS8_PRIV_KEY_INFO_nid_postfix();
int APS5_VABI BN_div_nid_postfix(Bignum* quotient, Bignum* remainder, const Bignum* numerator,
                                 const Bignum* divisor, BignumContext* context);
int APS5_VABI X509_CRL_get_ext_d2i_nid_postfix();
int APS5_VABI X509_digest_nid_postfix();
void APS5_VABI X509V3_set_ctx_nid_postfix(LibreSsl3X509V3Context* context, LibreSslX509* issuer, LibreSslX509* subject, void* request, void* crl, int flags);
int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::destroy) destroy);
int APS5_VABI tls_config_set_ciphers_nid_postfix();
int APS5_VABI RSA_pkey_ctx_ctrl_nid_postfix();
int APS5_VABI ASN1_get_object_nid_postfix();
int APS5_VABI SSL_provide_quic_data_nid_postfix();
LibreSsl::BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name);
int APS5_VABI d2i_PUBKEY_nid_postfix();
int APS5_VABI OBJ_find_sigid_algs_nid_postfix(int signatureNid, int* digestNid, int* keyNid);
int APS5_VABI ECDSA_do_verify_nid_postfix();
const char* APS5_VABI ERR_reason_error_string_nid_postfix(std::uint64_t error);
int APS5_VABI X509_add1_ext_i2d_nid_postfix();
int APS5_VABI EC_GROUP_cmp_nid_postfix();
int APS5_VABI EVP_PKEY_sign_init_nid_postfix();
int APS5_VABI EC_POINT_dup_nid_postfix();
int APS5_VABI RSA_set0_key_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* n, LibreSsl3Bignum* e, LibreSsl3Bignum* d);
int APS5_VABI EVP_PKEY_cmp_nid_postfix(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second);
int APS5_VABI d2i_ASN1_SEQUENCE_ANY_nid_postfix();
int APS5_VABI PKCS5_PBKDF2_HMAC_nid_postfix(const char* password, int passwordLength, const unsigned char* salt, int saltLength, int iterations, const EvpMd* digest, int keyLength, unsigned char* output);
void APS5_VABI ASN1_OCTET_STRING_free_nid_postfix(Asn1String* string);
int APS5_VABI tls_config_parse_protocols_nid_postfix(std::uint32_t* protocols, const char* text);
void APS5_VABI SSL_CTX_set_post_handshake_auth_nid_postfix(void* ctx, int value);
int APS5_VABI ASN1_TIME_diff_nid_postfix(int* days, int* seconds, const Asn1String* from, const Asn1String* to);
int APS5_VABI OBJ_obj2txt_nid_postfix();
int APS5_VABI SSL_set_quic_transport_params_nid_postfix();
int APS5_VABI SSL_is_quic_nid_postfix();
LibreSsl3X509Pubkey* APS5_VABI X509_get_X509_PUBKEY_nid_postfix(const LibreSsl3Certificate* certificate);
void APS5_VABI ECDSA_SIG_get0_nid_postfix(const LibreSsl3EcdsaSig* sig, const void** r, const void** s);
int APS5_VABI EVP_MD_block_size_nid_postfix(const void* md);

}
