#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceVoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
 (void)mem_block;
 (void)mem_size;
 (void)app_type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
