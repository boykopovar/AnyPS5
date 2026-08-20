#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNetAccept(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

int sceNetBind(int s, const void* addr, uint32_t addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

int sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
 (void)eid;
 (void)op;
 (void)id;
 (void)event;
 return 0;
}

int sceNetEpollCreate(const char* name, int flags) {
 (void)name;
 (void)flags;
 return 0;
}

int sceNetEpollDestroy(int eid) {
 (void)eid;
 return 0;
}

int sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
 (void)eid;
 (void)events;
 (void)maxevents;
 (void)timeout;
 return 0;
}

int sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
 (void)n;
 (void)str;
 (void)len;
 return 0;
}

int sceNetGetMacAddress(NetEtherAddr* addr, int flags) {
 (void)addr;
 (void)flags;
 return 0;
}

int sceNetGetSockInfo(int s, void* info, int n, int flags) {
 (void)s;
 (void)info;
 (void)n;
 (void)flags;
 return 0;
}

int sceNetGetsockname(int s, void* addr, uint32_t* addrlen) {
 (void)s;
 (void)addr;
 (void)addrlen;
 return 0;
}

uint32_t sceNetHtonl(uint32_t host32) {
 (void)host32;
 return 0;
}

uint16_t sceNetHtons(uint16_t host16) {
 (void)host16;
 return 0;
}

const char* sceNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
 (void)af;
 (void)src;
 (void)dst;
 (void)size;
 return nullptr;
}

int sceNetInetPton(int af, const char* src, void* dst) {
 (void)af;
 (void)src;
 (void)dst;
 return 0;
}

int sceNetInit(void) {
 return 0;
}

int sceNetListen(int s, int backlog) {
 (void)s;
 (void)backlog;
 return 0;
}

uint32_t sceNetNtohl(uint32_t net32) {
 (void)net32;
 return 0;
}

uint16_t sceNetNtohs(uint16_t net16) {
 (void)net16;
 return 0;
}

int sceNetPoolCreate(const char* name, int size, int flags) {
 (void)name;
 (void)size;
 (void)flags;
 return 0;
}

int sceNetPoolDestroy(int memid) {
 (void)memid;
 return 0;
}

int sceNetResolverCreate(const char* name, int memid, int flags) {
 (void)name;
 (void)memid;
 (void)flags;
 return 0;
}

int sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
 (void)rid;
 (void)hostname;
 (void)addr;
 (void)timeout;
 (void)retry;
 (void)flags;
 return 0;
}

int sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
 (void)s;
 (void)level;
 (void)optname;
 (void)optval;
 (void)optlen;
 return 0;
}

int sceNetShutdown(int s, int how) {
 (void)s;
 (void)how;
 return 0;
}

int sceNetSocket(const char* name, int family, int type, int protocol) {
 (void)name;
 (void)family;
 (void)type;
 (void)protocol;
 return 0;
}

int sceNetSocketClose(int s) {
 (void)s;
 return 0;
}

}
