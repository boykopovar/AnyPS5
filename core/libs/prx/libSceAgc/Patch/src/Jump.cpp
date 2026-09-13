#include "prx/libSceAgc/Patch/include/Jump.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
 (void)cmd;
 (void)target;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
