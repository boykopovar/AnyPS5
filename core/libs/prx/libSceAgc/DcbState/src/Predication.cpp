#include "prx/libSceAgc/DcbState/include/Predication.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t wait_op, const volatile void* address, uint32_t count_in_dwords) {
    // SET_PREDICATION turns predication on for the packets that follow it; the range length is a
    // guest-side bookkeeping hint the single PM4 packet does not carry.
    (void)count_in_dwords;
    return Agc::Command::WritePredication(buf, condition, op, wait_op, address, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetZPassPredicationEnableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbSetPredicationDisableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbSetBoolPredicationEnableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
