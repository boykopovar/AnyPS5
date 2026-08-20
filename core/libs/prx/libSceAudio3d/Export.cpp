#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

void sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* p) {
 (void)p;
}

int sceAudio3dInitialize(int64_t reserved) {
 (void)reserved;
 return 0;
}

int sceAudio3dPortAdvance(uint32_t port_id) {
 (void)port_id;
 return 0;
}

int sceAudio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level, uint32_t* queue_available) {
 (void)port_id;
 (void)queue_level;
 (void)queue_available;
 return 0;
}

int sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, uint32_t* id) {
 (void)user_id;
 (void)parameters;
 (void)id;
 return 0;
}

int sceAudio3dPortPush(uint32_t port_id, uint32_t blocking) {
 (void)port_id;
 (void)blocking;
 return 0;
}

int sceAudio3dPortSetAttribute(uint32_t port_id, uint32_t attribute_id, const void* attribute, size_t attribute_size) {
 (void)port_id;
 (void)attribute_id;
 (void)attribute;
 (void)attribute_size;
 return 0;
}

}
