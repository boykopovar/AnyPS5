#pragma once

#include "SceTypes.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

extern "C" {

int APS5_VABI ASN1_INTEGER_free_nid_postfix(void);
int APS5_VABI ASN1_OBJECT_free_nid_postfix(void);
int APS5_VABI ASN1_STRING_get0_data_nid_postfix(void);
int APS5_VABI BIO_up_ref_nid_postfix(void);
int APS5_VABI BIO_vfree_nid_postfix(void);
int APS5_VABI CRYPTO_memcmp_nid_postfix(const void* first, const void* second, std::size_t length);
int APS5_VABI DH_get0_key_nid_postfix(void);
int APS5_VABI DH_get0_pqg_nid_postfix(void);
int APS5_VABI DSA_get0_key_nid_postfix(void);
int APS5_VABI DSA_get0_pqg_nid_postfix(void);
int APS5_VABI EC_KEY_up_ref_nid_postfix(LibreSsl3EcKey* key);
int APS5_VABI ERR_load_crypto_strings_nid_postfix(void);
int APS5_VABI ERR_peek_last_error_nid_postfix(void);
int APS5_VABI ERR_print_errors_cb_nid_postfix(void);
int APS5_VABI EVP_CIPHER_CTX_set_flags_nid_postfix(void);
int APS5_VABI EVP_DigestInit_nid_postfix(void);
int APS5_VABI EVP_DigestSignFinal_nid_postfix(void);
int APS5_VABI EVP_DigestSignInit_nid_postfix(void);
int APS5_VABI EVP_EncryptInit_nid_postfix(void);
int APS5_VABI EVP_MD_CTX_free_nid_postfix(void);
int APS5_VABI EVP_MD_CTX_new_nid_postfix(void);
int APS5_VABI EVP_PKCS82PKEY_nid_postfix(void);
int APS5_VABI EVP_PKEY_CTX_ctrl_nid_postfix(void);
int APS5_VABI EVP_PKEY_CTX_new_nid_postfix(void);
int APS5_VABI EVP_PKEY_decrypt_nid_postfix(void);
int APS5_VABI EVP_PKEY_decrypt_init_nid_postfix(void);
int APS5_VABI EVP_PKEY_encrypt_nid_postfix(void);
int APS5_VABI EVP_PKEY_encrypt_init_nid_postfix(void);
int APS5_VABI EVP_PKEY_get0_DH_nid_postfix(void);
int APS5_VABI EVP_PKEY_get0_DSA_nid_postfix(void);
int APS5_VABI EVP_PKEY_get0_RSA_nid_postfix(void);
int APS5_VABI EVP_PKEY_new_mac_key_nid_postfix(void);
int APS5_VABI EVP_PKEY_set1_EC_KEY_nid_postfix(void);
int APS5_VABI EVP_PKEY_size_nid_postfix(void);
int APS5_VABI EVP_PKEY_up_ref_nid_postfix(LibreSsl3EvpPkey* key);
int APS5_VABI EVP_SignFinal_nid_postfix(void);
int APS5_VABI EVP_VerifyFinal_nid_postfix(void);
int APS5_VABI EVP_aes_128_wrap_nid_postfix(void);
int APS5_VABI EVP_aes_192_wrap_nid_postfix(void);
int APS5_VABI EVP_aes_256_wrap_nid_postfix(void);
int APS5_VABI EVP_sha1_nid_postfix(void);
int APS5_VABI EVP_sha224_nid_postfix(void);
int APS5_VABI OBJ_cmp_nid_postfix(void);
int APS5_VABI OBJ_txt2obj_nid_postfix(void);
int APS5_VABI OCSP_CERTID_free_nid_postfix(void);
int APS5_VABI OCSP_cert_to_id_nid_postfix(void);
int APS5_VABI OCSP_resp_find_status_nid_postfix(void);
int APS5_VABI OpenSSL_version_nid_postfix(void);
int APS5_VABI PKCS7_free_nid_postfix(void);
int APS5_VABI PKCS8_PRIV_KEY_INFO_free_nid_postfix(void);
int APS5_VABI RSA_check_key_nid_postfix(void);
int APS5_VABI RSA_get0_key_nid_postfix(void);
int APS5_VABI RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix(void);
int APS5_VABI RSA_up_ref_nid_postfix(void);
int APS5_VABI SSL_CTX_add0_chain_cert_nid_postfix(void);
int APS5_VABI SSL_CTX_clear_chain_certs_nid_postfix(void);
int APS5_VABI SSL_CTX_set_max_proto_version_nid_postfix(void);
int APS5_VABI SSL_CTX_set_min_proto_version_nid_postfix(void);
int APS5_VABI SSL_CTX_up_ref_nid_postfix(void);
int APS5_VABI SSL_SESSION_up_ref_nid_postfix(void);
int APS5_VABI SSL_get_client_random_nid_postfix(void);
int APS5_VABI SSL_up_ref_nid_postfix(void);
int APS5_VABI X509_PUBKEY_get0_param_nid_postfix(void);
int APS5_VABI X509_STORE_CTX_free_nid_postfix(void);
int APS5_VABI X509_STORE_CTX_init_nid_postfix(void);
int APS5_VABI X509_STORE_CTX_new_nid_postfix(void);
int APS5_VABI X509_STORE_up_ref_nid_postfix(void);
int APS5_VABI X509_get0_extensions_nid_postfix(void);
int APS5_VABI X509_get0_notAfter_nid_postfix(void);
int APS5_VABI X509_get0_notBefore_nid_postfix(void);
int APS5_VABI X509_get0_signature_nid_postfix(void);
int APS5_VABI X509_get_ext_nid_postfix(void);
int APS5_VABI X509_get_ext_count_nid_postfix(void);
int APS5_VABI X509_get_version_nid_postfix(void);
int APS5_VABI X509_getm_notAfter_nid_postfix(void);
int APS5_VABI X509_getm_notBefore_nid_postfix(void);
int APS5_VABI X509_up_ref_nid_postfix(void);
void* APS5_VABI d2i_ASN1_INTEGER_nid_postfix(void** output, const unsigned char** input, std::int64_t length);
int APS5_VABI d2i_ECParameters_nid_postfix(void);
int APS5_VABI d2i_PKCS7_nid_postfix(void);
int APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(void);
int APS5_VABI d2i_PrivateKey_bio_nid_postfix(void);
int APS5_VABI d2i_RSAPublicKey_bio_nid_postfix(void);
int APS5_VABI d2i_X509_bio_nid_postfix(void);
int APS5_VABI o2i_ECPublicKey_nid_postfix(void);
unsigned char* APS5_VABI MD5_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest);
unsigned char* APS5_VABI SHA1_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest);

}
