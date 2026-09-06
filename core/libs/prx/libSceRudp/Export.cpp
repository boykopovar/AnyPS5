#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceRudpEnableInternalIOThread(uint32_t stack_size, uint32_t priority) {
 (void)stack_size;
 (void)priority;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceRudpInit_nid_postfix(void* mem_pool, int mem_pool_size) {
 (void)mem_pool;
 (void)mem_pool_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceRudpSetEventHandler(RudpEventHandler handler, void* arg) {
 (void)handler;
 (void)arg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
