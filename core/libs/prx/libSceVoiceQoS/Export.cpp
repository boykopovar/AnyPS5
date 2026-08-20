#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceVoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
 (void)mem_block;
 (void)mem_size;
 (void)app_type;
 return 0;
}

}
