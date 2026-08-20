#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNetCtlCheckCallback(void) {
 return 0;
}

int sceNetCtlGetInfo(int code, NetCtlInfo* info) {
 (void)code;
 (void)info;
 return 0;
}

int sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
 (void)nat_info;
 return 0;
}

int sceNetCtlGetResult(int event_type, int* error_code) {
 (void)event_type;
 (void)error_code;
 return 0;
}

int sceNetCtlGetState(int* state) {
 (void)state;
 return 0;
}

int sceNetCtlGetStateV6(int* state) {
 (void)state;
 return 0;
}

int sceNetCtlInit(void) {
 return 0;
}

int sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
 (void)func;
 (void)arg;
 (void)cid;
 return 0;
}

void sceNetCtlTerm(void) {
}

int sceNetCtlUnregisterCallback(int cid) {
 (void)cid;
 return 0;
}

}
