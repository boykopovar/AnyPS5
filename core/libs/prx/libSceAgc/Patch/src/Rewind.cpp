#include "prx/libSceAgc/Patch/include/Rewind.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcRewindPatchSetRewindState(uint32_t* cmd, uint8_t state) {
 (void)cmd;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
