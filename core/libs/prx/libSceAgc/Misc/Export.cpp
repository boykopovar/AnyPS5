#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceAgcGetRegisterDefaults2(uint32_t ver) {
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

void* APS5_VABI sceAgcGetRegisterDefaults2Internal(uint32_t ver) {
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type) {
 (void)addr;
 (void)cmd;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSuspendPoint(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcInit_nid_postfix(uint32_t* state, uint32_t ver) {
 (void)state;
 (void)ver;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
}
