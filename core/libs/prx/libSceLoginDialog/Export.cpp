#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceLoginDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceLoginDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
