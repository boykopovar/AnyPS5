#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceMsgDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogProgressBarInc(int target, uint32_t delta) {
 (void)target;
 (void)delta;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
 (void)target;
 (void)msg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogProgressBarSetValue(int target, uint32_t rate) {
 (void)target;
 (void)rate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceMsgDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
