#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceErrorDialogClose(void) {
 return 0;
}

int sceErrorDialogGetStatus(void) {
 return 0;
}

int sceErrorDialogInitialize(void) {
 return 0;
}

int sceErrorDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int sceErrorDialogTerminate(void) {
 return 0;
}

int sceErrorDialogUpdateStatus(void) {
 return 0;
}

}
