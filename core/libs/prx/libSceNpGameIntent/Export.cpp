#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intent_data, const char* key, char* value_buf, size_t buf_size) {
 (void)intent_data;
 (void)key;
 (void)value_buf;
 (void)buf_size;
 return 0;
}

int sceNpGameIntentInitialize(const void* init_param) {
 (void)init_param;
 return 0;
}

int sceNpGameIntentReceiveIntent(NpGameIntentInfo* intent_info) {
 (void)intent_info;
 return 0;
}

int sceNpGameIntentTerminate(void) {
 return 0;
}

}
