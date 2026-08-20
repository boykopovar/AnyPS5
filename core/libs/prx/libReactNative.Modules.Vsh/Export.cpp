#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int RemotePlayGetConnectionStatus(int user_id, int* status) {
 (void)user_id;
 (void)status;
 return 0;
}

int ShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
 (void)feature_flag;
 (void)status;
 return 0;
}

int ShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
 (void)heap_size;
 (void)thread_priority;
 (void)affinity_mask;
 return 0;
}

int ShareTerminate(void) {
 return 0;
}

}
