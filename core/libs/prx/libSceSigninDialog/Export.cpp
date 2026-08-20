#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSigninDialogClose(void) {
 return 0;
}

int sceSigninDialogGetResult(void* result) {
 (void)result;
 return 0;
}

int sceSigninDialogGetStatus(void) {
 return 0;
}

int sceSigninDialogInitialize(void) {
 return 0;
}

int sceSigninDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int sceSigninDialogTerminate(void) {
 return 0;
}

int sceSigninDialogUpdateStatus(void) {
 return 0;
}

}
