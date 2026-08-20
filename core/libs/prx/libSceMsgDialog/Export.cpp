#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceMsgDialogClose(void) {
 return 0;
}

int sceMsgDialogGetResult(void* result) {
 (void)result;
 return 0;
}

int sceMsgDialogGetStatus(void) {
 return 0;
}

int sceMsgDialogInitialize(void) {
 return 0;
}

int sceMsgDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int sceMsgDialogProgressBarInc(int target, uint32_t delta) {
 (void)target;
 (void)delta;
 return 0;
}

int sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
 (void)target;
 (void)msg;
 return 0;
}

int sceMsgDialogProgressBarSetValue(int target, uint32_t rate) {
 (void)target;
 (void)rate;
 return 0;
}

int sceMsgDialogTerminate(void) {
 return 0;
}

int sceMsgDialogUpdateStatus(void) {
 return 0;
}

}
