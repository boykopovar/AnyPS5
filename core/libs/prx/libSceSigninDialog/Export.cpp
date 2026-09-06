#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSigninDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSigninDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
