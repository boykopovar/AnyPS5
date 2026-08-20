#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSaveDataDialogClose(const void* close_param) {
 (void)close_param;
 return 0;
}

int sceSaveDataDialogGetResult(void* result) {
 (void)result;
 return 0;
}

int sceSaveDataDialogGetStatus(void) {
 return 0;
}

int sceSaveDataDialogInitialize(void) {
 return 0;
}

int sceSaveDataDialogIsReadyToDisplay(void) {
 return 0;
}

int sceSaveDataDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int sceSaveDataDialogProgressBarInc(int target, uint32_t delta) {
 (void)target;
 (void)delta;
 return 0;
}

int sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate) {
 (void)target;
 (void)rate;
 return 0;
}

int sceSaveDataDialogTerminate(void) {
 return 0;
}

int sceSaveDataDialogUpdateStatus(void) {
 return 0;
}

}
