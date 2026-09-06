#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceNetCtlCheckCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlGetInfo(int code, NetCtlInfo* info) {
 (void)code;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
 (void)nat_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlGetResult(int event_type, int* error_code) {
 (void)event_type;
 (void)error_code;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlGetState(int* state) {
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlGetStateV6(int* state) {
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlInit(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
 (void)func;
 (void)arg;
 (void)cid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceNetCtlTerm(void) {
 NotImplemented_nid_no_patch(__func__);
}

int sceNetCtlUnregisterCallback(int cid) {
 (void)cid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
