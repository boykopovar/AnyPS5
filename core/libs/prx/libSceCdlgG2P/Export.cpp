#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceG2PDialogInitialize(void) {
    throw std::runtime_error("sceG2PDialogInitialize: unknown signature");
}

int APS5_VABI sceG2PDialogTerminate(void) {
    throw std::runtime_error("sceG2PDialogTerminate: unknown signature");
}

int APS5_VABI sceG2PDialogOpen(void) {
    throw std::runtime_error("sceG2PDialogOpen: unknown signature");
}

int APS5_VABI sceG2PDialogGetStatus(void) {
    throw std::runtime_error("sceG2PDialogGetStatus: unknown signature");
}

int APS5_VABI sceG2PDialogUpdateStatus(void) {
    throw std::runtime_error("sceG2PDialogUpdateStatus: unknown signature");
}

int APS5_VABI sceG2PDialogGetResult(void) {
    throw std::runtime_error("sceG2PDialogGetResult: unknown signature");
}
}
