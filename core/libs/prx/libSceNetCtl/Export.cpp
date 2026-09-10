#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNetCtlCheckCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
 (void)code;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
 (void)nat_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code) {
 (void)event_type;
 (void)error_code;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlGetState(int* state) {
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlInit(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
 (void)func;
 (void)arg;
 (void)cid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceNetCtlTerm(void) {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
 (void)cid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
