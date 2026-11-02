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

}
