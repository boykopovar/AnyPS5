#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI ErrorDialogClose(void) {
 return 0;
}

int APS5_VABI ErrorDialogOpen(const void* param) {
 (void)param;
 return 0;
}

int APS5_VABI ShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
 (void)feature_flag;
 if (status) *status = {};
 return 0;
}

int APS5_VABI ShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
 (void)heap_size;
 (void)thread_priority;
 (void)affinity_mask;
 return 0;
}

int APS5_VABI ShareTerminate(void) {
 return 0;
}

int APS5_VABI SystemServiceParamGetInt(int param_id, int* value) {
 (void)param_id;
 if (value) *value = 0;
 return 0;
}

int APS5_VABI SystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 (void)param_id;
 if (buf && buf_size > 0) buf[0] = '\0';
 return 0;
}

}
