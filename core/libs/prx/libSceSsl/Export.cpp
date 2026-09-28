#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80435001);
static std::atomic<int> g_nextHandle{1};

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
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    (void)ssl_ctx_id;
    return 0;
}

int APS5_VABI sceSslClose() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSslGetSerialNumber() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
