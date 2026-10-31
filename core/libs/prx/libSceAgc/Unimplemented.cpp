#include "prx/libSceAgc/include/Agc.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcAcquireMemSetEngine(std::uint32_t* packet, std::uint8_t engine) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbInitializeDefaultHardwareState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
