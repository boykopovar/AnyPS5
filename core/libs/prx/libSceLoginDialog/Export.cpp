#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceLoginDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceLoginDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
