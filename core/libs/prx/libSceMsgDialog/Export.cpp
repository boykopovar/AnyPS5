#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMsgDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarInc(int target, uint32_t delta) {
 (void)target;
 (void)delta;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
 (void)target;
 (void)msg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarSetValue(int target, uint32_t rate) {
 (void)target;
 (void)rate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
