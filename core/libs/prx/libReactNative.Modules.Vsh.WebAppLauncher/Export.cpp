#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int ErrorDialogClose(void) {
 return 0;
}

int ErrorDialogOpen(const void* param) {
 (void)param;
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

int SystemServiceParamGetInt(int param_id, int* value) {
 (void)param_id;
 (void)value;
 return 0;
}

int SystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 (void)param_id;
 (void)buf;
 (void)buf_size;
 return 0;
}

}
