#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
 (void)ssl_ctx_id;
 (void)ca_certs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
 (void)ssl_ctx_id;
 (void)ca_certs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
 (void)pool_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
 (void)ssl_ctx_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
