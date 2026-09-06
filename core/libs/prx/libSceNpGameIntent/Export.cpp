#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intent_data, const char* key, char* value_buf, size_t buf_size) {
 (void)intent_data;
 (void)key;
 (void)value_buf;
 (void)buf_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGameIntentInitialize(const void* init_param) {
 (void)init_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGameIntentReceiveIntent(NpGameIntentInfo* intent_info) {
 (void)intent_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGameIntentTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
