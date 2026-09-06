#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSaveDataDialogClose(const void* close_param) {
 (void)close_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogIsReadyToDisplay(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogProgressBarInc(int target, uint32_t delta) {
 (void)target;
 (void)delta;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate) {
 (void)target;
 (void)rate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
