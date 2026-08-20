#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceLoginDialogClose(void) {
 return 0;
}

int sceLoginDialogGetResult(void* result) {
 (void)result;
 return 0;
}

int sceLoginDialogGetStatus(void) {
 return 0;
}

int sceLoginDialogInitialize(void) {
 return 0;
}

int sceLoginDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int sceLoginDialogTerminate(void) {
 return 0;
}

int sceLoginDialogUpdateStatus(void) {
 return 0;
}

}
