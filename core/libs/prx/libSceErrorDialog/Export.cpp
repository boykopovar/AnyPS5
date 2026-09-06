#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceErrorDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceErrorDialogGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceErrorDialogInitialize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceErrorDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceErrorDialogTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceErrorDialogUpdateStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
