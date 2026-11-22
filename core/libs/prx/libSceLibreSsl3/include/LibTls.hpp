#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct TlsConfig;
struct TlsContext;

extern "C" {

TlsConfig* APS5_VABI tls_config_new_nid_postfix();
void APS5_VABI tls_config_free_nid_postfix(TlsConfig* config);
const char* APS5_VABI tls_config_error_nid_postfix(TlsConfig* config);
int APS5_VABI tls_config_set_key_mem_nid_postfix(TlsConfig* config, const std::uint8_t* key, std::size_t length);
TlsContext* APS5_VABI tls_client_nid_postfix();
TlsContext* APS5_VABI tls_server_nid_postfix();
void APS5_VABI tls_free_nid_postfix(TlsContext* context);
const char* APS5_VABI tls_error_nid_postfix(TlsContext* context);
int APS5_VABI tls_configure_nid_postfix(TlsContext* context, TlsConfig* config);
int APS5_VABI tls_close_nid_postfix(TlsContext* context);
int APS5_VABI tls_config_set_ca_mem_nid_postfix(TlsConfig* config, const std::uint8_t* ca, std::size_t length);
int APS5_VABI tls_config_set_cert_mem_nid_postfix(TlsConfig* config, const std::uint8_t* cert, std::size_t length);
int APS5_VABI tls_config_set_protocols_nid_postfix(TlsConfig* config, std::uint32_t protocols);
int APS5_VABI tls_config_set_ciphers_nid_postfix(TlsConfig* config, const char* ciphers);
void APS5_VABI tls_config_verify_client_nid_postfix(TlsConfig* config);
void APS5_VABI tls_config_insecure_noverifyname_nid_postfix(TlsConfig* config);

}
