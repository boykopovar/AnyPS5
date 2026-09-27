#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI accept_nid_postfix(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int APS5_VABI connect_nid_postfix(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI listen_nid_postfix(int s, int backlog) {
 (void)s;
 (void)backlog;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}





int64_t APS5_VABI send_nid_postfix(int s, const void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


int64_t APS5_VABI recv_nid_postfix(int s, void* buf, uint64_t len, int flags) {
 (void)s;
 (void)buf;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


const char* APS5_VABI inet_ntop_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
 (void)af;
 (void)src;
 (void)dst;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI inet_pton_nid_postfix(int af, const char* src, void* dst) {
 (void)af;
 (void)src;
 (void)dst;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
 (void)nfds;
 (void)readfds;
 (void)writefds;
 (void)exceptfds;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
