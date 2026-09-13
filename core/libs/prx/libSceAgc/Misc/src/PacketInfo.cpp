#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"

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

int APS5_VABI sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type) {
 (void)addr;
 (void)cmd;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("V++UgBtQhn0", sceAgcGetDataPacketPayloadAnotherAddress);
int APS5_VABI sceAgcGetDataPacketPayloadAnotherAddress(uint32_t** addr, uint32_t* cmd, int type) {
    (void)addr;
    (void)cmd;
    (void)type;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
