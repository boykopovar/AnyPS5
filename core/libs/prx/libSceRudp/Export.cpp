#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceRudpEnableInternalIOThread(uint32_t stack_size, uint32_t priority) {
 (void)stack_size;
 (void)priority;
 return 0;
}

int sceRudpInit(void* mem_pool, int mem_pool_size) {
 (void)mem_pool;
 (void)mem_pool_size;
 return 0;
}

int sceRudpSetEventHandler(RudpEventHandler handler, void* arg) {
 (void)handler;
 (void)arg;
 return 0;
}

}
