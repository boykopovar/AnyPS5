#include "prx/libSceAgc/DcbState/include/Predication.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t wait_op, const volatile void* address, uint32_t count_in_dwords) {
 (void)buf;
 (void)condition;
 (void)op;
 (void)wait_op;
 (void)address;
 (void)count_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
