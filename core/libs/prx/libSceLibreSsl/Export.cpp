#include <cinttypes>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libc/include/General.hpp"
#include "mbedtls/des.h"

namespace {

constexpr std::size_t DesScheduleBytes = 128;

}

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI AES_cbc_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_cfb128_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_ctr128_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_decrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_ecb_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_set_decrypt_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI AES_set_encrypt_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_INTEGER_get_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_STRING_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_STRING_length_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_STRING_print_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_STRING_to_UTF8_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_STRING_type_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ASN1_TIME_print_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_f_ssl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_new_fp_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_new_socket_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_printf_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_s_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_write_nid_postfix(LibreSsl::Bio* bio, const void* source, int size) {
    return LibreSslBioWrite_nid_no_patch(bio, source, size);
}

int APS5_VABI BN_print_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CONF_modules_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CONF_modules_load_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_THREADID_set_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_THREADID_set_numeric_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_cleanup_all_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_num_locks_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_set_add_lock_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI CRYPTO_set_locking_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI DES_ecb_encrypt_nid_postfix(const unsigned char* input, unsigned char* output, const unsigned char* schedule, int enc) {
    if (input == nullptr || output == nullptr || schedule == nullptr) APS5_INVALID_ARG_EX;
    mbedtls_des_context context;
    mbedtls_des_init(&context);
    unsigned char block[MBEDTLS_DES_KEY_SIZE];
    std::memcpy(block, input, sizeof(block));
    int status = enc ? mbedtls_des_setkey_enc(&context, schedule) : mbedtls_des_setkey_dec(&context, schedule);
    if (status == 0) status = mbedtls_des_crypt_ecb(&context, block, output);
    mbedtls_des_free(&context);
    if (status != 0) throw std::runtime_error("DES_ecb_encrypt: mbedtls failed");
}

int APS5_VABI DES_set_key_nid_postfix(const unsigned char* key, unsigned char* schedule) {
    if (key == nullptr || schedule == nullptr) APS5_INVALID_ARG_EX;
    std::memset(schedule, 0, DesScheduleBytes);
    std::memcpy(schedule, key, MBEDTLS_DES_KEY_SIZE);
    return 0;
}

void APS5_VABI DES_set_odd_parity_nid_postfix(unsigned char* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    mbedtls_des_key_set_parity(key);
}

int APS5_VABI DTLSv1_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI DTLSv1_server_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ECDH_compute_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ECDSA_sign_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ECDSA_size_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ECDSA_verify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_GROUP_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_GROUP_get_degree_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_GROUP_new_by_curve_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_check_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_generate_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_get0_group_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_get0_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_get0_public_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_new_by_curve_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_set_group_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_set_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_set_public_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_POINT_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_POINT_mul_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_POINT_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_POINT_oct2point_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_POINT_point2oct_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_by_id_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_ctrl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_ctrl_cmd_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_finish_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_get_first_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_get_id_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_get_next_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_load_builtin_engines_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_load_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_set_default_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_ctrl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_CTX_set_padding_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CIPHER_block_size_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CipherFinal_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CipherInit_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_CipherUpdate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DecryptFinal_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DecryptInit_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DecryptUpdate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_Digest_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DigestFinal_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DigestInit_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DigestUpdate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DigestVerifyFinal_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_DigestVerifyInit_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_EncryptFinal_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_EncryptInit_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_EncryptUpdate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_MD_CTX_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_MD_CTX_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_MD_CTX_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_MD_CTX_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_MD_size_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_copy_parameters_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_get1_EC_KEY_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_get1_RSA_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_id_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_set1_RSA_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_PKEY_type_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_128_cfb8_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_128_ctr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_128_gcm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_192_cfb8_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_192_ctr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_192_gcm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_256_cfb8_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_256_ctr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_aes_256_gcm_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_md5_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_sha256_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_sha384_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EVP_sha512_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI GENERAL_NAMES_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI GENERAL_NAME_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_CTX_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_CTX_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_Final_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_Init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_Init_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI HMAC_Update_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OBJ_cleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OBJ_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OBJ_nid2sn_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OBJ_obj2nid_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OBJ_sn2nid_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_BASICRESP_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_RESPONSE_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_basic_add1_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_basic_verify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_cert_status_str_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_check_validity_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_crl_reason_str_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_resp_count_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_resp_get0_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_response_get1_basic_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_response_status_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_response_status_str_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OCSP_single_get0_status_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OPENSSL_add_all_algorithms_noconf_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI OPENSSL_load_builtin_modules_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_X509_INFO_read_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_read_RSAPrivateKey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_read_X509_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_read_bio_PrivateKey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_read_bio_X509_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_read_bio_X509_AUX_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PEM_write_bio_X509_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PKCS12_PBE_add_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PKCS12_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI PKCS12_parse_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_flags_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_generate_key_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_padding_add_PKCS1_PSS_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_private_decrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_private_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_public_decrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_public_encrypt_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_sign_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_size_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_verify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI RSA_verify_PKCS1_PSS_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CIPHER_get_bits_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CIPHER_get_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CIPHER_get_version_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_add_client_CA_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_callback_ctrl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_check_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_ctrl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_get_cert_store_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_get_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_get_ex_new_index_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_get_verify_mode_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_load_verify_locations_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_load_verify_mem_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_sess_set_new_cb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set1_groups_list_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_alpn_protos_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_alpn_select_cb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_cert_store_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_cert_verify_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_cipher_list_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_client_cert_cb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_default_passwd_cb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_info_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_msg_callback_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_next_proto_select_cb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_quiet_shutdown_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_session_id_context_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_verify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_PrivateKey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_PrivateKey_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_PrivateKey_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_certificate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_certificate_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_certificate_chain_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_use_certificate_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_SESSION_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_accept_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_alert_desc_string_long_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_check_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_connect_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_ctrl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get0_alpn_selected_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get0_param_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get1_session_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_SSL_CTX_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_certificate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_current_cipher_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_error_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_ex_new_index_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_fd_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_peer_cert_chain_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_peer_certificate_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_privatekey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_rbio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_servername_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_session_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_shutdown_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_verify_result_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_wbio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_library_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_load_error_strings_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_peek_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_pending_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_read_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_select_next_proto_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_SSL_CTX_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_connect_state_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_fd_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_session_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_verify_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_shutdown_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_use_PrivateKey_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_use_certificate_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_version_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_want_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_write_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSLeay_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSLv23_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSLv23_server_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI TLS_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_OpenSSL_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_create_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_destroy_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_get0_user_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_get_input_flags_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_get_string_type_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_get_closer_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_get_opener_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_get_reader_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_get_writer_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_set_closer_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_set_opener_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_set_reader_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_method_set_writer_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_set_result_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509V3_EXT_print_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_EXTENSION_get_critical_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_EXTENSION_get_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_EXTENSION_get_object_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_INFO_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_LOOKUP_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_ENTRY_get_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_get_entry_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_get_index_by_NID_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_get_text_by_NID_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_oneline_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_print_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get1_chain_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get_chain_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get_current_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get_error_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get_error_depth_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_get_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_CTX_set_error_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_add_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_add_crl_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_add_lookup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_set_flags_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_VERIFY_PARAM_set1_host_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_VERIFY_PARAM_set_hostflags_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_check_issued_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_ext_d2i_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_issuer_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_pubkey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_serialNumber_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_subject_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_load_crl_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_print_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_verify_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI X509_verify_cert_error_string_nid_postfix(std::int64_t error) {
    static constexpr const char* messages[] {
        "ok",
        nullptr,
        "unable to get issuer certificate",
        "unable to get certificate CRL",
        "unable to decrypt certificate's signature",
        "unable to decrypt CRL's signature",
        "unable to decode issuer public key",
        "certificate signature failure",
        "CRL signature failure",
        "certificate is not yet valid",
        "certificate has expired",
        "CRL is not yet valid",
        "CRL has expired",
        "format error in certificate's notBefore field",
        "format error in certificate's notAfter field",
        "format error in CRL's lastUpdate field",
        "format error in CRL's nextUpdate field",
        "out of memory",
        "self signed certificate",
        "self signed certificate in certificate chain",
        "unable to get local issuer certificate",
        "unable to verify the first certificate",
        "certificate chain too long",
        "certificate revoked",
        "invalid CA certificate",
        "path length constraint exceeded",
        "unsupported certificate purpose",
        "certificate not trusted",
        "certificate rejected",
        "subject issuer mismatch",
        "authority and subject key identifier mismatch",
        "authority and issuer serial number mismatch",
        "key usage does not include certificate signing",
        "unable to get CRL issuer certificate",
        "unhandled critical extension",
        "key usage does not include CRL signing",
        "unhandled critical CRL extension",
        "invalid non-CA certificate (has CA markings)",
        "proxy path length constraint exceeded",
        "key usage does not include digital signature",
        "proxy certificates not allowed, please set the appropriate flag",
        "invalid or inconsistent certificate extension",
        "invalid or inconsistent certificate policy extension",
        "no explicit policy",
        "Different CRL scope",
        "Unsupported extension feature",
        "RFC 3779 resource not subset of parent's resources",
        "permitted subtree violation",
        "excluded subtree violation",
        "name constraints minimum and maximum not supported",
        "application verification failure",
        "unsupported name constraint type",
        "unsupported or invalid name constraint syntax",
        "unsupported or invalid name syntax",
        "CRL path validation error",
    };
    const auto code = static_cast<std::uint32_t>(error);
    if (code < sizeof(messages) / sizeof(messages[0]) && messages[code]) return messages[code];
    static char unknown[100];
    std::snprintf(unknown, sizeof(unknown), "error number %" PRId64, error);
    return unknown;
}

int APS5_VABI d2i_OCSP_RESPONSE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI d2i_PKCS12_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI d2i_PKCS12_fp_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI d2i_RSA_PUBKEY_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI d2i_X509_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI i2a_ASN1_OBJECT_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI i2d_X509_PUBKEY_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI i2t_ASN1_OBJECT_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslGetMemoryPoolStats(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInitEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInitEx2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sk_num_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sk_pop_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sk_pop_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sk_value_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
