#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI ErrorDialogClose(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI ErrorDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI ShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
 (void)feature_flag;
 (void)status;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI ShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
 (void)heap_size;
 (void)thread_priority;
 (void)affinity_mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI ShareTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI SystemServiceParamGetInt(int param_id, int* value) {
 (void)param_id;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI SystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 (void)param_id;
 (void)buf;
 (void)buf_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
