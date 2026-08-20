#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
 (void)ssl_ctx_id;
 (void)ca_certs;
 return 0;
}

int sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
 (void)ssl_ctx_id;
 (void)ca_certs;
 return 0;
}

int sceSslInit(uint64_t pool_size) {
 (void)pool_size;
 return 0;
}

int sceSslTerm(int ssl_ctx_id) {
 (void)ssl_ctx_id;
 return 0;
}

}
