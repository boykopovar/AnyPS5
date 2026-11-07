#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI tls_config_set_cert_mem_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI BIO_clear_flags_nid_postfix(LibreSsl3Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_clear_flags requires a BIO");
    bio->flags &= ~flags;
}

void APS5_VABI BIO_set_flags_nid_postfix(LibreSsl3Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_set_flags requires a BIO");
    bio->flags |= flags;
}

int APS5_VABI BN_dup_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ERR_func_error_string_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_CTX_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_assign_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_derive_set_peer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_get0_EC_KEY_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_verify_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_verify_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_sha512_256_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI HKDF_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI HMAC_CTX_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI HMAC_CTX_new_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI OPENSSL_init_ssl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PEM_read_bio_PUBKEY_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI SSL_CTX_add1_chain_cert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set0_chain_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_timeout_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_do_handshake_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_get_version_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_set_accept_state_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_set_alpn_protos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_set_info_callback_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI TLS_method_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI TLS_server_method_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_EXTENSION_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_STORE_CTX_get0_param_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_STORE_load_locations_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_get0_pubkey_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI d2i_SSL_SESSION_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI d2i_X509_CRL_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI i2d_SSL_SESSION_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI i2d_X509_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI tls_read_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI tls_write_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EC_KEY_set_asn1_flag_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI BIO_set_init_nid_postfix(LibreSsl3Bio* bio, int init) {
    if (!bio) throw std::invalid_argument("BIO_set_init requires a BIO");
    bio->initialized = init;
}

int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_NAME_add_entry_by_txt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl3BioMethod* method) {
    if (method) free_nid_postfix(method);
}

int APS5_VABI tls_handshake_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI AUTHORITY_KEYID_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509V3_EXT_d2i_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_3KZ_minus_L4y1gak_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3KZ-L4y1gak", Unknown_3KZ_minus_L4y1gak_nid_no_patch);

int APS5_VABI EVP_chacha20_poly1305_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_chacha20_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_set_pubkey_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI BIO_get_shutdown_nid_postfix(const LibreSsl3Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_shutdown requires a BIO");
    return bio->shutdown;
}

int APS5_VABI tls_accept_socket_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_4pEl_minus_5DmrrM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4pEl-5DmrrM", Unknown_4pEl_minus_5DmrrM_nid_no_patch);

int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl3BioMethod* method, void* write) {
    if (!method) throw std::invalid_argument("BIO_meth_set_write requires a method");
    method->write = write;
    return 1;
}

int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl3BioMethod* method, void* create) {
    if (!method) throw std::invalid_argument("BIO_meth_set_create requires a method");
    method->create = create;
    return 1;
}

int APS5_VABI X509_CRL_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl3BioMethod* method, void* read) {
    if (!method) throw std::invalid_argument("BIO_meth_set_read requires a method");
    method->read = read;
    return 1;
}

int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_max_early_data_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_A_minus_Hs_minus_8mau_plus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("A-Hs-8mau+8", Unknown_A_minus_Hs_minus_8mau_plus_8_nid_no_patch);

int APS5_VABI ERR_load_X509_strings_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_AiNud8LR8B4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AiNud8LR8B4", Unknown_AiNud8LR8B4_nid_no_patch);

int APS5_VABI X509_cmp_current_time_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_get_ext_by_NID_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_check_akid_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_set_quic_use_legacy_codepoint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_default_verify_paths_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ERR_lib_error_string_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl3BioMethod* method, void* ctrl) {
    if (!method) throw std::invalid_argument("BIO_meth_set_ctrl requires a method");
    method->ctrl = ctrl;
    return 1;
}

int APS5_VABI Unknown_GjO5Ap3E_minus_t4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GjO5Ap3E-t4", Unknown_GjO5Ap3E_minus_t4_nid_no_patch);

int APS5_VABI Unknown_HE_minus_vihq_minus_KKw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HE-vihq-KKw", Unknown_HE_minus_vihq_minus_KKw_nid_no_patch);

int APS5_VABI X509_set_version_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI DES_set_key_unchecked_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_CTX_new_id_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI RSAPublicKey_dup_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI AES_unwrap_key_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_print_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_CRL_get0_by_cert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ERR_load_PEM_strings_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI OpenSSL_add_all_digests_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_base_id_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_PBYz0kpXwg8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PBYz0kpXwg8", Unknown_PBYz0kpXwg8_nid_no_patch);

int APS5_VABI X509_new_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_get0_verified_chain_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_derive_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_sign_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_get_signature_nid_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ECDSA_SIG_new_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_derive_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509V3_EXT_conf_nid_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_sign_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_get_peer_quic_transport_params_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI BIO_get_data_nid_postfix(const LibreSsl3Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_data requires a BIO");
    return bio->data;
}

int APS5_VABI EVP_PKEY2PKCS8_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ASN1_TIME_new_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_set_issuer_name_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_add_ext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI tls_connect_socket_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_quic_method_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_gmtime_adj_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_process_quic_post_handshake_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_aLdgrI2jAF0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aLdgrI2jAF0", Unknown_aLdgrI2jAF0_nid_no_patch);

int APS5_VABI EC_POINT_clear_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI d2i_PKCS8_PRIV_KEY_INFO_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ECDSA_do_sign_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_ciphersuites_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PEM_write_bio_PrivateKey_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI BIO_set_data_nid_postfix(LibreSsl3Bio* bio, void* data) {
    if (!bio) throw std::invalid_argument("BIO_set_data requires a BIO");
    bio->data = data;
}

int APS5_VABI SSL_CTX_set_keylog_callback_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_ALGOR_get0_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI BIO_set_shutdown_nid_postfix(LibreSsl3Bio* bio, int shutdown) {
    if (!bio) throw std::invalid_argument("BIO_set_shutdown requires a BIO");
    bio->shutdown = shutdown;
}

int APS5_VABI i2d_PKCS8_PRIV_KEY_INFO_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI BN_div_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_CRL_get_ext_d2i_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_digest_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509V3_set_ctx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl3BioMethod* method, void* destroy) {
    if (!method) throw std::invalid_argument("BIO_meth_set_destroy requires a method");
    method->destroy = destroy;
    return 1;
}

int APS5_VABI tls_config_set_ciphers_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI RSA_pkey_ctx_ctrl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ASN1_get_object_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_provide_quic_data_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

LibreSsl3BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<LibreSsl3BioMethod*>(calloc_nid_postfix(1, sizeof(LibreSsl3BioMethod)));
    if (!method) return nullptr;
    method->type = type;
    method->name = name;
    return method;
}

int APS5_VABI d2i_PUBKEY_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI Unknown_pdxCm9_plus_CCc4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pdxCm9+CCc4", Unknown_pdxCm9_plus_CCc4_nid_no_patch);

int APS5_VABI Unknown_qGsc6uT6p1E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qGsc6uT6p1E", Unknown_qGsc6uT6p1E_nid_no_patch);

int APS5_VABI ERR_reason_error_string_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_add1_ext_i2d_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EC_GROUP_cmp_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_PKEY_sign_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EC_POINT_dup_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_aes_128_ccm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI d2i_ASN1_SEQUENCE_ANY_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCS5_PBKDF2_HMAC_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_post_handshake_auth_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ASN1_TYPE_free_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ECDSA_SIG_set0_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI OBJ_obj2txt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_set_quic_transport_params_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_is_quic_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI X509_get_X509_PUBKEY_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EVP_MD_block_size_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
